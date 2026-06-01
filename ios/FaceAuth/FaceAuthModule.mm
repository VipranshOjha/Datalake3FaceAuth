/**
 * FaceAuthModule.mm
 * * Datalake 3.0 — iOS Objective-C++ Bridge
 * Hackathon 7.0 | NHAI
 * *
 * Bridges the React Native iOS runtime to the C++ FaceAuthHostObject.
 * Uses RCTCxxBridge to extract the JSI Runtime pointer and
 * jsCallInvoker, then calls installFaceAuth() to register the
 * HostObject on the JavaScript global scope.
 *
 * THREAD SAFETY:
 *   install() runs on the JS thread (it's a blocking synchronous
 *   method). All subsequent HostObject interactions are managed
 *   by the C++ layer's thread isolation protocol.
 */

#import <React/RCTBridgeModule.h>
#import <React/RCTBridge+Private.h>

#import <jsi/jsi.h>
#import <ReactCommon/CallInvoker.h>

// Include the shared C++ installation function
#include "FaceAuthHostObject.h"

#import <Foundation/Foundation.h>

// // FaceAuthModule — React Native Bridge Module (iOS)
// @interface FaceAuthModule : NSObject <RCTBridgeModule>
@end

@implementation FaceAuthModule

// Register this module with React Native under the name "FaceAuthModule"
RCT_EXPORT_MODULE(FaceAuthModule)

// Ensure module runs on the JS thread (required for JSI access)
+ (BOOL)requiresMainQueueSetup {
    return NO;  // Module can be initialized on any thread
}

/**
 * Install the JSI bindings.
 * Called from JavaScript:
 *
 *   const { NativeModules } = require('react-native');
 *   NativeModules.FaceAuthModule.install();
 *
 * This is a BLOCKING SYNCHRONOUS method — returns only after
 * the C++ HostObject is fully registered on global.__FaceAuthModule.
 */
RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install)
{
    @try {
        // Get the current React Native bridge
        RCTBridge *bridge = [RCTBridge currentBridge];
        if (!bridge) {
            NSLog(@"[DatalakeFaceAuth] ERROR: RCTBridge is nil. "
                   "Cannot install JSI module.");
            return @(NO);
        }

        // Cast to RCTCxxBridge to access the JSI runtime
        RCTCxxBridge *cxxBridge = (RCTCxxBridge *)bridge;
        if (!cxxBridge) {
            NSLog(@"[DatalakeFaceAuth] ERROR: Failed to cast to RCTCxxBridge. "
                   "Is the New Architecture enabled?");
            return @(NO);
        }

        // Extract the raw JSI Runtime pointer
        facebook::jsi::Runtime *runtime =
            (facebook::jsi::Runtime *)cxxBridge.runtime;

        if (!runtime) {
            NSLog(@"[DatalakeFaceAuth] ERROR: JSI Runtime is null. "
                   "Bridge may not be fully initialized.");
            return @(NO);
        }

        // Get the JS thread CallInvoker for async result delivery
        auto jsCallInvoker = bridge.jsCallInvoker;
        if (!jsCallInvoker) {
            NSLog(@"[DatalakeFaceAuth] ERROR: jsCallInvoker is null.");
            return @(NO);
        }

        // Install the FaceAuth HostObject into the JS runtime
        datalake::installFaceAuth(*runtime, jsCallInvoker);

        NSLog(@"[DatalakeFaceAuth] JSI module installed successfully at "
               "global.__FaceAuthModule");
        return @(YES);

    } @catch (NSException *exception) {
        NSLog(@"[DatalakeFaceAuth] EXCEPTION during install: %@ — %@",
              exception.name, exception.reason);
        return @(NO);
    }
}

@end
