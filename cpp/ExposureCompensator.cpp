/**
 * ExposureCompensator.cpp
 * * Datalake 3.0 — Adaptive Exposure Compensation
 * Hackathon 7.0 | NHAI
 * */
#include "ExposureCompensator.h"
#include <algorithm>
#include <cmath>

namespace datalake {

float ExposureCompensator::adjustThreshold(float baseThreshold, const uint8_t* frameData, const FrameMetadata& meta) {
    if (!frameData || meta.width <= 0 || meta.height <= 0) return baseThreshold;

    const int stride = 4;
    int64_t sum = 0;
    int count = 0;

    // Explicit format checking for bounds safety
    int channels = 1;
    if (meta.format == PixelFormat::RGB_888 || meta.format == PixelFormat::BGR_888) {
        channels = 3;
    } else if (meta.format == PixelFormat::RGBA_8888 || meta.format == PixelFormat::BGRA_8888) {
        channels = 4;
    }

    for (int y = 0; y < meta.height; y += stride) {
        for (int x = 0; x < meta.width; x += stride) {
            int idx = (y * meta.width + x) * channels;
            // First channel roughly correlates to Luma in YUV or R in RGB
            sum += frameData[idx];
            count++;
        }
    }

    float avgBrightness = static_cast<float>(sum) / count;

    float adjustment = 0.0f;
    if (avgBrightness < 60.0f) {
        adjustment = -0.05f * ((60.0f - avgBrightness) / 60.0f);
    } else if (avgBrightness > 200.0f) {
        adjustment = -0.05f * ((avgBrightness - 200.0f) / 55.0f);
    }

    adjustment = std::max(-0.05f, adjustment);

    return std::max(0.0f, baseThreshold + adjustment);
}

} // namespace datalake
