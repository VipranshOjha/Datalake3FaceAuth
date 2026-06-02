/**
 * FaceAuthModule.java
 * * Datalake 3.0 — React Native Java Module (Android)
 * Hackathon 7.0 | NHAI
 * *
 * Java-side React Native module that loads the native library
 * (libdatalake_faceauth.so) and exposes the install() method
 * to JavaScript. The install() method bridges the JSI runtime
 * into C++ where the FaceAuthHostObject is registered.
 *
 * LIFECYCLE:
 *   1. React Native loads this module during app startup
 *   2. Static initializer loads libdatalake_faceauth.so → JNI_OnLoad
 *   3. JS calls FaceAuthModule.install() → nativeInstall()
 *   4. C++ installFaceAuth() sets global.__FaceAuthModule
 *   5. JS can now call global.__FaceAuthModule.nativeVerifyUser(), etc.
 */

package com.datalake.faceauth;

import android.util.Log;

import androidx.annotation.NonNull;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;

public class FaceAuthModule extends ReactContextBaseJavaModule {

    private static final String TAG = "DatalakeFaceAuth";
    private static final String MODULE_NAME = "FaceAuthModule";

    // Load the native library on class initialization.
    // This triggers JNI_OnLoad in jni_entry.cpp.
    static {
        try {
            System.loadLibrary("datalake_faceauth");
            Log.i(TAG, "libdatalake_faceauth.so loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load libdatalake_faceauth.so: " + e.getMessage());
            throw e;
        }
    }

    public FaceAuthModule(ReactApplicationContext reactContext) {
        super(reactContext);
    }

    @Override
    @NonNull
    public String getName() {
        return MODULE_NAME;
    }

    /**
     * Install the JSI bindings. Called from JavaScript:
     *
     *   const { NativeModules } = require('react-native');
     *   NativeModules.FaceAuthModule.install();
     *
     * This is a BLOCKING SYNCHRONOUS method — it runs on the JS thread
     * and returns only after the C++ HostObject is fully registered.
     * This is intentional: we need the HostObject to be available
     * immediately after install() returns so that subsequent JS calls
     * to global.__FaceAuthModule work without race conditions.
     */
    @ReactMethod(isBlockingSynchronousMethod = true)
    public boolean install() {
        try {
            ReactApplicationContext context = getReactApplicationContext();

            // Get the raw JSI runtime pointer from React Native
            long runtimePtr = context.getJavaScriptContextHolder().get();

            if (runtimePtr == 0) {
                Log.e(TAG, "install(): JSI runtime pointer is null. "
                        + "Is the React Native bridge initialized?");
                return false;
            }

            // Get the CallInvokerHolder for async JS thread callbacks
            // This allows our C++ background thread to safely dispatch
            // results back to the JS thread.
            Object callInvokerHolder = context.getCatalystInstance()
                    .getJSCallInvokerHolder();

            if (callInvokerHolder == null) {
                Log.e(TAG, "install(): CallInvokerHolder is null.");
                return false;
            }

            // Call into C++ to register the JSI HostObject
            nativeInstall(runtimePtr, callInvokerHolder);

            Log.i(TAG, "install(): FaceAuth JSI module installed successfully");
            return true;

        } catch (Exception e) {
            Log.e(TAG, "install() failed: " + e.getMessage(), e);
            return false;
        }
    }

    /**
     * JNI bridge to C++ installFaceAuth().
     * Implemented in android/src/main/cpp/jni_entry.cpp
     */
    private static native void nativeInstall(long runtimePtr, Object callInvokerHolder);
}
