/**
 * FaceFrameProcessorPlugin.cpp
 * * Datalake 3.0 — VisionCamera Frame Processor Binding
 * Hackathon 7.0 | NHAI
 * */
#include "FaceFrameProcessorPlugin.h"
#include "CircularFrameBuffer.h"
#include "ImageUtils.h"
#include "PlatformImageHandler.h"

namespace datalake {

using namespace facebook::jsi;

extern CircularFrameBuffer* g_FrameBuffer;

void installFaceFrameProcessorPlugin(Runtime& runtime, std::shared_ptr<facebook::react::CallInvoker> callInvoker) {
    auto scanFacesPlugin = Function::createFromHostFunction(
        runtime,
        PropNameID::forAscii(runtime, "scanFaces"),
        1, // arguments: (frame)
        [](Runtime& rt, const Value& thisVal, const Value* args, size_t count) -> Value {
            // Lazy instantiate handler to avoid allocations on the camera hot-path
            static thread_local std::unique_ptr<PlatformImageHandler> handlerInstance = nullptr;

            if (count < 1 || !args[0].isObject()) {
                throw JSError(rt, "scanFaces expects a Frame object");
            }

            void* nativeFrame = nullptr;
            // Native frame extraction stub
            // #ifdef __ANDROID__
            //   nativeFrame = frame->getHardwareBuffer();
            // #elif defined(__APPLE__)
            //   nativeFrame = frame->getPixelBuffer();
            // #endif

            if (g_FrameBuffer && nativeFrame) {
                if (!handlerInstance) {
                    handlerInstance = createPlatformImageHandler();
                }
                handlerInstance->processIncomingFrame(nativeFrame, g_FrameBuffer);
            }

            return Value::undefined();
        }
    );

    runtime.global().setProperty(runtime, "scanFaces", std::move(scanFacesPlugin));
}

} // namespace datalake
