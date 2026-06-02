/**
 * ImageUtils.h
 * * Datalake 3.0 -- High-Performance Image Manipulation
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <cstdint>
#include <vector>
#include "FaceAuthHostObject.h" // For PixelFormat
#include "LivenessDetector.h" // For Point2D

#ifdef __ANDROID__
#include <android/hardware_buffer.h>
#elif defined(__APPLE__)
#include <CoreVideo/CoreVideo.h>
#endif

namespace datalake {

class ImageUtils {
public:
    // Platform-Specific Color-Space Conversions
#ifdef __ANDROID__
    /**
     * Extracts AHardwareBuffer zero-copy DMA-BUF memory and converts it 
     * directly to RGB.
     */
    static bool convertAHardwareBufferToRGB(AHardwareBuffer* hwBuffer, uint8_t* rgbOut, int outWidth, int outHeight);

    /**
     * High-performance NEON intrinsics conversion for NV21/NV12 to RGB.
     */
    static void convertYUVToRGB_NEON(const uint8_t* yPlane, const uint8_t* uvPlane, 
                                     int width, int height, bool isNV21, uint8_t* rgbOut);
#elif defined(__APPLE__)
    /**
     * Utilizes Apple's vImage API (Accelerate Framework) for zero-copy 
     * conversion from kCVPixelFormatType_420YpCbCr8BiPlanarFullRange to RGB.
     */
    static bool convertCVPixelBufferToRGB(CVPixelBufferRef pixelBuffer, uint8_t* rgbOut, int outWidth, int outHeight);
#endif

    /**
     * Affine alignment using standard 5-point facial landmarks 
     * to correct head tilt/yaw before passing vectors down the pipeline.
     */
    static void cropAlignFace(const uint8_t* srcRgb, int srcW, int srcH,
                              const std::vector<Point2D>& landmarks5,
                              uint8_t* dstRgb, int dstW, int dstH);

    /**
     * Resize bounding box crop down to exactly 112x112 pixels for 
     * MobileFaceNet inference engine.
     */
    static void resizeBilinear(const uint8_t* src, int srcW, int srcH, 
                               uint8_t* dst, int dstW, int dstH);

    /**
     * Map uint8 data to standardized float32 array configuration [-1, 1].
     */
    static void normalizeFloat(const uint8_t* src, float* dst, int numPixels);
};

} // namespace datalake
