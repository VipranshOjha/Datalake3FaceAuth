/**
 * PlatformImageHandler_Android.cpp
 * * Datalake 3.0 — Android AHardwareBuffer Frame Ingestion
 * Hackathon 7.0 | NHAI
 * */
#include "PlatformImageHandler.h"
#include <android/hardware_buffer.h>
#include <android/log.h>

#define LOG_TAG "DatalakeFaceAuth_Img"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace datalake {

class PlatformImageHandlerAndroid : public PlatformImageHandler {
public:
    void processIncomingFrame(void* nativeFrame, CircularFrameBuffer* buffer) override {
        if (!nativeFrame || !buffer) return;

        AHardwareBuffer* hwBuffer = static_cast<AHardwareBuffer*>(nativeFrame);
        AHardwareBuffer_Desc desc;
        AHardwareBuffer_describe(hwBuffer, &desc);

        void* virtualAddress = nullptr;
        int status = AHardwareBuffer_lock(hwBuffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &virtualAddress);

        if (status == 0 && virtualAddress != nullptr) {
            size_t dataSize = (desc.width * desc.height * 3) / 2; // NV21/NV12 size

            FrameMetadata meta;
            meta.width = desc.width;
            meta.height = desc.height;
            meta.format = PixelFormat::NV21;
            meta.timestampMs = 0; 

            // Map straight to the finalized pushFrame method
            buffer->pushFrame(static_cast<const uint8_t*>(virtualAddress), dataSize, meta);

            AHardwareBuffer_unlock(hwBuffer, nullptr);
        } else {
            LOGE("Failed to lock AHardwareBuffer: error code %d", status);
        }
    }
};

std::unique_ptr<PlatformImageHandler> createPlatformImageHandler() {
    return std::make_unique<PlatformImageHandlerAndroid>();
}

} // namespace datalake
