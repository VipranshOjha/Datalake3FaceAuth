/**
 * FaceFrameProcessorPlugin.h
 * * Datalake 3.0 — VisionCamera Frame Processor Binding
 * Hackathon 7.0 | NHAI
 * *
 * Registration binding class for react-native-vision-camera's Frame 
 * Processor plugin framework. This enables direct, low-overhead native 
 * pointer parsing from the camera feed.
 */

#pragma once
#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>

namespace datalake {

/**
 * Installs the "scanFaces" Frame Processor Plugin into the JSI runtime.
 * Provides the VisionCamera integration endpoint for the camera ingest thread.
 */
void installFaceFrameProcessorPlugin(
    facebook::jsi::Runtime& runtime, 
    std::shared_ptr<facebook::react::CallInvoker> callInvoker);

} // namespace datalake
