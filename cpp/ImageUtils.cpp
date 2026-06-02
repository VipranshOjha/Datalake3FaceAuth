/**
 * ImageUtils.cpp
 * * Datalake 3.0 -- High-Performance Image Manipulation
 * Hackathon 7.0 | NHAI
 * */
#include "ImageUtils.h"
#include <cmath>
#include <algorithm>

#ifdef __ANDROID__
#include <arm_neon.h>
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ImageUtils", __VA_ARGS__)
#elif defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#endif

namespace datalake {

// Android Path: DMA-BUF and NEON YUV->RGB
#ifdef __ANDROID__
bool ImageUtils::convertAHardwareBufferToRGB(AHardwareBuffer* hwBuffer, uint8_t* rgbOut, int outWidth, int outHeight) {
    if (!hwBuffer || !rgbOut) return false;

    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(hwBuffer, &desc);

    void* virtualAddress = nullptr;
    int status = AHardwareBuffer_lock(hwBuffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &virtualAddress);
    if (status != 0 || !virtualAddress) {
        LOGE("Failed to lock AHardwareBuffer");
        return false;
    }

    // Assuming NV21 for hardware buffer
    const uint8_t* yPlane = static_cast<const uint8_t*>(virtualAddress);
    const uint8_t* uvPlane = yPlane + (desc.width * desc.height);

    convertYUVToRGB_NEON(yPlane, uvPlane, desc.width, desc.height, true, rgbOut);

    AHardwareBuffer_unlock(hwBuffer, nullptr);
    return true;
}

void ImageUtils::convertYUVToRGB_NEON(const uint8_t* yPlane, const uint8_t* uvPlane, 
                                      int width, int height, bool isNV21, uint8_t* rgbOut) {
    // High-performance ARM NEON intrinsics stub for YUV->RGB
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            // Simplified fallback for demonstration.
            for (int i = 0; i < 8 && x + i < width; ++i) {
                int cx = x + i;
                int yIdx = y * width + cx;
                int uvIdx = (y / 2) * width + (cx & ~1);

                int yVal = yPlane[yIdx];
                int vVal = isNV21 ? uvPlane[uvIdx] : uvPlane[uvIdx + 1];
                int uVal = isNV21 ? uvPlane[uvIdx + 1] : uvPlane[uvIdx];

                uVal -= 128;
                vVal -= 128;

                int r = yVal + 1.402f * vVal;
                int g = yVal - 0.344f * uVal - 0.714f * vVal;
                int b = yVal + 1.772f * uVal;

                int outIdx = yIdx * 3;
                rgbOut[outIdx] = std::max(0, std::min(255, r));
                rgbOut[outIdx + 1] = std::max(0, std::min(255, g));
                rgbOut[outIdx + 2] = std::max(0, std::min(255, b));
            }
        }
    }
}
#endif

// iOS Path: vImage Accelerate Framework
#ifdef __APPLE__
bool ImageUtils::convertCVPixelBufferToRGB(CVPixelBufferRef pixelBuffer, uint8_t* rgbOut, int outWidth, int outHeight) {
    if (!pixelBuffer || !rgbOut) return false;

    if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
        return false;
    }

    int width = CVPixelBufferGetWidth(pixelBuffer);
    int height = CVPixelBufferGetHeight(pixelBuffer);
    OSType format = CVPixelBufferGetPixelFormatType(pixelBuffer);

    if (format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || 
        format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {

        vImage_Buffer luma = {
            CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0),
            (vImagePixelCount)height,
            (vImagePixelCount)width,
            CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0)
        };

        vImage_Buffer chroma = {
            CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1),
            (vImagePixelCount)(height / 2),
            (vImagePixelCount)(width / 2),
            CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1)
        };

        vImage_Buffer rgbDest = {
            rgbOut,
            (vImagePixelCount)height,
            (vImagePixelCount)width,
            (size_t)(width * 3) // RGB 24-bit
        };

        vImage_YpCbCrToARGB info;
        vImage_YpCbCrPixelRange pixelRange = (format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) 
            ? (vImage_YpCbCrPixelRange){ 0, 128, 255, 255, 255, 1, 255, 0 } 
            : (vImage_YpCbCrPixelRange){ 16, 128, 235, 240, 255, 0, 255, 1 }; 

        vImageConvert_YpCbCrToARGB_GenerateConversion(
            kvImage_YpCbCrToARGBMatrix_ITU_R_601_4,
            &pixelRange,
            &info,
            kvImage420Yp8_CbCr8,
            kvImageRGB888, // Convert directly to RGB888 format natively
            kvImageNoFlags
        );

        vImageConvert_420Yp8_CbCr8ToRGB888(
            &luma, &chroma, &rgbDest, &info, nullptr, 255, kvImageNoFlags
        );
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return true;
}
#endif

// Matrix Operators & Alignment
void ImageUtils::cropAlignFace(const uint8_t* srcRgb, int srcW, int srcH,
                               const std::vector<Point2D>& landmarks5,
                               uint8_t* dstRgb, int dstW, int dstH) {
    // Affine transformation based on 5-point landmarks to correct tilt/yaw
    // (Stub implementation: falls back to standard bilinear resize)
    resizeBilinear(srcRgb, srcW, srcH, dstRgb, dstW, dstH);
}

void ImageUtils::resizeBilinear(const uint8_t* src, int srcW, int srcH, 
                                uint8_t* dst, int dstW, int dstH) {
    float scaleX = static_cast<float>(srcW) / dstW;
    float scaleY = static_cast<float>(srcH) / dstH;

    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {
            float srcX = x * scaleX;
            float srcY = y * scaleY;

            int x0 = std::min(std::max(0, static_cast<int>(srcX)), srcW - 1);
            int x1 = std::min(std::max(0, x0 + 1), srcW - 1);
            int y0 = std::min(std::max(0, static_cast<int>(srcY)), srcH - 1);
            int y1 = std::min(std::max(0, y0 + 1), srcH - 1);

            float dx = srcX - x0;
            float dy = srcY - y0;

            for (int c = 0; c < 3; ++c) {
                float v00 = src[(y0 * srcW + x0) * 3 + c];
                float v10 = src[(y0 * srcW + x1) * 3 + c];
                float v01 = src[(y1 * srcW + x0) * 3 + c];
                float v11 = src[(y1 * srcW + x1) * 3 + c];

                float val = (v00 * (1.0f - dx) + v10 * dx) * (1.0f - dy) + 
                            (v01 * (1.0f - dx) + v11 * dx) * dy;

                dst[(y * dstW + x) * 3 + c] = static_cast<uint8_t>(val);
            }
        }
    }
}

void ImageUtils::normalizeFloat(const uint8_t* src, float* dst, int numPixels) {
    // Map uint8 [0, 255] to float32 [-1, 1] for the ONNX models
    for (int i = 0; i < numPixels * 3; ++i) {
        dst[i] = (static_cast<float>(src[i]) - 127.5f) / 128.0f;
    }
}

} // namespace datalake
