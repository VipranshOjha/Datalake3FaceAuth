/**
 * PlatformImageHandler.h
 * * Datalake 3.0 — Platform-Specific Image Ingestion
 * Hackathon 7.0 | NHAI
 * *
 * Abstract interface for zero-copy camera frame ingestion.
 * 
 * Android uses AHardwareBuffer.
 * iOS uses vImage (Accelerate Framework) on CVPixelBufferRef.
 */

#pragma once
#include <memory>
#include "CircularFrameBuffer.h"

namespace datalake {

class PlatformImageHandler {
public:
    virtual ~PlatformImageHandler() = default;

    /**
     * Process an incoming native frame from the camera and push it 
     * directly into the CircularFrameBuffer using the fastest available
     * platform-specific path.
     *
     * @param nativeFrame The platform-specific frame pointer 
     *                    (AHardwareBuffer* on Android, CVPixelBufferRef on iOS)
     * @param buffer The destination lock-free circular buffer
     */
    virtual void processIncomingFrame(void* nativeFrame, CircularFrameBuffer* buffer) = 0;
};

/**
 * Factory function to instantiate the correct handler for the current OS.
 * Implemented in PlatformImageHandler_Android.cpp and PlatformImageHandler_iOS.cpp.
 */
std::unique_ptr<PlatformImageHandler> createPlatformImageHandler();

} // namespace datalake
