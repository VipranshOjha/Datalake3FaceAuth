/**
 * jni_entry.cpp
 * * Datalake 3.0 — JNI Entry Point for Android
 * Hackathon 7.0 | NHAI
 * *
 * This file provides the JNI bridge between the Java FaceAuthModule
 * and the C++ FaceAuthHostObject. It handles:
 *
 *   1. JNI_OnLoad:    Initialize FBJNI runtime tables and store the
 *                     JavaVM pointer globally (needed by MonotonicClock
 *                     for SystemClock.elapsedRealtime() JNI calls on
 *                     the worker thread).
 *
 *   2. nativeInstall: Extract the JSI Runtime and CallInvoker pointers
 *                     from Java, then call installFaceAuth() to register
 *                     the HostObject on the JS global scope.
 *
 * CRITICAL FIX (v2):
 *   JNI_OnLoad MUST call facebook::jni::initialize() to register
 *   FBJNI's internal class lookup tables before ANY fbjni types are
 *   used. Without this, accessing fbjni-managed Java objects from
 *   native code causes SIGSEGV / NoClassDefFoundError at runtime.
 *
 * SECURITY NOTE:
 *   The jlong pointers passed from Java are raw memory addresses.
 *   We validate non-null but cannot verify type safety across the
 *   JNI boundary. The Java side MUST pass correct pointers.
 */

#include <jni.h>
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>

// FBJNI initialization — MUST be included and called in JNI_OnLoad
#include <fbjni/fbjni.h>

#include "FaceAuthHostObject.h"

#include <android/log.h>
#include <cassert>

#define LOG_TAG "DatalakeFaceAuth_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// // Global JavaVM pointer — accessible from any native thread.
// Required by MonotonicClock to call SystemClock.elapsedRealtime()
// via JNI on the NativeWorker thread.
// static JavaVM* g_javaVM = nullptr;
/**
 * Get the globally stored JavaVM pointer.
 * Used by MonotonicClock and any other component that needs JNI
 * access from a non-Java-attached thread.
 *
 * Usage:
 *   JNIEnv* env;
 *   JavaVM* vm = getJavaVM();
 *   bool needsDetach = false;
 *   if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
 *       vm->AttachCurrentThread(&env, nullptr);
 *       needsDetach = true;
 *   }
 *   // ... use env ...
 *   if (needsDetach) vm->DetachCurrentThread();
 */
extern "C" JavaVM* getJavaVM() {
    return g_javaVM;
}

// // JNI_OnLoad — Called when System.loadLibrary("datalake_faceauth")
// //
// CRITICAL: facebook::jni::initialize() MUST be the first call here.
// It registers FBJNI's internal class cache and JNI method ID lookups.
// Without it, any subsequent use of fbjni types (HybridData,
// JavaClass, etc.) will crash with ClassNotFoundException or SIGSEGV.
//
// The initialize() call internally:
//   1. Stores the JavaVM* reference for FBJNI's own use
//   2. Populates JNI class/method lookup caches
//   3. Registers any FBJNI-managed native methods
//
// The second argument is a registration callback where you can
// register additional FBJNI hybrid objects. We pass an empty lambda
// since FaceAuthModule registration is handled separately via the
// manual JNI nativeInstall method.
// extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    // Store globally for cross-thread JNI access (MonotonicClock, etc.)
    g_javaVM = vm;

    // FBJNI Initialization (MUST come first)
    // This registers FBJNI's internal class tables and prepares the
    // JNI environment for safe interop with React Native's native
    // module infrastructure.
    return facebook::jni::initialize(vm, [] {
        // Registration callback for FBJNI hybrid objects.
        //
        // We don't register any HybridClass objects here because
        // FaceAuthModule uses manual JNI registration via
        // Java_com_datalake_faceauth_FaceAuthModule_nativeInstall.
        //
        // If future components (e.g., VisionCamera frame processor
        // plugins) need FBJNI HybridClass registration, add them here:
        //
        // facebook::jni::registerNatives(
        //     "com/datalake/faceauth/SomeHybridClass",
        //     { makeNativeMethod("someMethod", SomeHybridClass::someMethod) }
        // );

        LOGI("FBJNI initialized. Native registration callback complete.\n");
    });

    // NOTE: facebook::jni::initialize() returns JNI_VERSION_1_6 on
    // success or causes a fatal abort on failure. The code below this
    // point is unreachable but kept for documentation clarity.
}

// // nativeInstall — Called from FaceAuthModule.java::install()
// //
// This is a manually registered JNI method (not FBJNI-managed).
// The Java side calls it with raw jlong pointers obtained from
// React Native's internal holders:
//
//   long runtimePtr = context.getJavaScriptContextHolder().get();
//   long callInvokerPtr = /* from CatalystInstance */;
//   nativeInstall(runtimePtr, callInvokerPtr);
//
// POINTER SAFETY:
//   - runtimePtr is a raw jsi::Runtime* cast to jlong
//   - callInvokerPtr is a raw shared_ptr<CallInvoker>* cast to jlong
//   - Both are managed by React Native's bridge lifecycle
//   - They are valid as long as the CatalystInstance is alive
//   - We validate non-null but cannot verify type correctness
// extern "C" JNIEXPORT void JNICALL
Java_com_datalake_faceauth_FaceAuthModule_nativeInstall(
    JNIEnv* env,
    jobject thiz,
    jlong runtimePtr,
    jlong callInvokerPtr)
{
    LOGI("nativeInstall called. runtimePtr=0x%llx, callInvokerPtr=0x%llx\n",
         (unsigned long long)runtimePtr, (unsigned long long)callInvokerPtr);

    // Validate pointers — defensive against bridge teardown races
    if (runtimePtr == 0) {
        LOGE("nativeInstall: runtimePtr is null! "
             "Is the React Native bridge fully initialized?\n");
        return;
    }
    if (callInvokerPtr == 0) {
        LOGE("nativeInstall: callInvokerPtr is null! "
             "Cannot create async JS callbacks.\n");
        return;
    }

    // Cast raw pointers back to C++ types.
    // Safety: these casts mirror what the Java side packed via
    // JavaScriptContextHolder.get() and JSCallInvokerHolder.
    auto* runtime = reinterpret_cast<facebook::jsi::Runtime*>(runtimePtr);

    // CallInvoker is passed as a pointer to a shared_ptr on the Java
    // side. The CallInvokerHolder prevents the shared_ptr from being
    // destroyed while the CatalystInstance is alive.
    auto callInvoker = *reinterpret_cast<
        std::shared_ptr<facebook::react::CallInvoker>*>(callInvokerPtr);

    // Verify the CallInvoker is actually valid before proceeding
    if (!callInvoker) {
        LOGE("nativeInstall: CallInvoker is null after cast! "
             "Bridge may be in a bad state.\n");
        return;
    }

    // Install the FaceAuth JSI HostObject into the JS runtime.
    // After this call, global.__FaceAuthModule is available in JS.
    try {
        datalake::installFaceAuth(*runtime, std::move(callInvoker));
        LOGI("nativeInstall: FaceAuth JSI module installed successfully.\n");
    } catch (const std::exception& e) {
        LOGE("nativeInstall: installFaceAuth threw: %s\n", e.what());
    } catch (...) {
        LOGE("nativeInstall: installFaceAuth threw unknown exception.\n");
    }
}
