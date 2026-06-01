/**
 * PlatformImageHandler_iOS.cpp
 * * Datalake 3.0 — iOS Accelerate Frame Ingestion
 * Hackathon 7.0 | NHAI
 * */
#include "PlatformImageHandler.h"

#ifdef __APPLE__
#include <CoreVideo/CoreVideo.h>
#include <Accelerate/Accelerate.h>

namespace datalake {

class PlatformImageHandlerIOS : public PlatformImageHandler {
public:
    void processIncomingFrame(void* nativeFrame, CircularFrameBuffer* buffer) override {
        if (!nativeFrame || !buffer) return;

        CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(nativeFrame);

        if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess) {
            int width = CVPixelBufferGetWidth(pixelBuffer);
            int height = CVPixelBufferGetHeight(pixelBuffer);

            size_t dataSize = CVPixelBufferGetDataSize(pixelBuffer);
            uint8_t* rawData = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));

            FrameMetadata meta;
            meta.width = width;
            meta.height = height;
            meta.format = PixelFormat::BGRA_8888;
            meta.timestampMs = 0;

            // Map straight to the finalized pushFrame method
            buffer->pushFrame(rawData, dataSize, meta);

            CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
        }
    }
};

std::unique_ptr<PlatformImageHandler> createPlatformImageHandler() {
    return std::make_unique<PlatformImageHandlerIOS>();
}

} // namespace datalake
#endif
