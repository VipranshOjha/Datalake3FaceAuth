/**
 * jni_entry.cpp
 * * Datalake 3.0 -- JNI Entry Point for Android
 * Hackathon 7.0 | NHAI
 * *
 * This file provides the JNI bridge between the Java FaceAuthModule
 * and the C++ FaceAuthHostObject.
 */

#include <jni.h>
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>
#include <ReactCommon/CallInvokerHolder.h>

// FBJNI initialization -- MUST be included and called in JNI_OnLoad
#include <fbjni/fbjni.h>

#include "FaceAuthHostObject.h"

#include <android/log.h>
#include <cassert>

#define LOG_TAG "DatalakeFaceAuth_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

static JavaVM* g_javaVM = nullptr;
/**
 * Get the globally stored JavaVM pointer.
 * Used by MonotonicClock and any other component that needs JNI
 * access from a non-Java-attached thread.
 */
extern "C" JavaVM* getJavaVM() {
    return g_javaVM;
}

// JNI_OnLoad -- Called when System.loadLibrary("datalake_faceauth")
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    // Store globally for cross-thread JNI access
    g_javaVM = vm;

    // FBJNI Initialization (MUST come first)
    return facebook::jni::initialize(vm, [] {
        LOGI("FBJNI initialized. Native registration callback complete.\n");
    });
}

// nativeInstall -- Called from FaceAuthModule.java::install()
extern "C" JNIEXPORT void JNICALL
Java_com_datalake_faceauth_FaceAuthModule_nativeInstall(
    JNIEnv* env,
    jobject thiz,
    jlong runtimePtr,
    jobject callInvokerHolderJavaObj)
{
    LOGI("nativeInstall called. runtimePtr=0x%llx\n", (unsigned long long)runtimePtr);

    // Validate pointers
    if (runtimePtr == 0) {
        LOGE("nativeInstall: runtimePtr is null!\n");
        return;
    }
    if (callInvokerHolderJavaObj == nullptr) {
        LOGE("nativeInstall: callInvokerHolderJavaObj is null!\n");
        return;
    }

    auto* runtime = reinterpret_cast<facebook::jsi::Runtime*>(runtimePtr);

    // Use FBJNI to extract the C++ CallInvoker from the Java Holder object
    auto callInvoker = facebook::jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>{
        reinterpret_cast<facebook::react::CallInvokerHolder::javaobject>(callInvokerHolderJavaObj)
    }->cthis()->getCallInvoker();

    // Verify the CallInvoker is actually valid before proceeding
    if (!callInvoker) {
        LOGE("nativeInstall: CallInvoker is null!\n");
        return;
    }

    // Install the FaceAuth JSI HostObject into the JS runtime.
    datalake::installFaceAuth(*runtime, std::move(callInvoker));
    LOGI("nativeInstall: FaceAuth JSI module installed successfully.\n");
}
