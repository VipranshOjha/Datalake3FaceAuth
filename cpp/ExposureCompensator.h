/**
 * ExposureCompensator.h
 * * Datalake 3.0 — Adaptive Exposure Compensation
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <cstdint>
#include "FaceAuthHostObject.h" // For FrameMetadata

namespace datalake {

class ExposureCompensator {
public:
    ExposureCompensator() = default;

    /**
     * Adjusts the matching threshold based on lighting conditions.
     * Uses FrameMetadata bounds explicitly.
     */
    float adjustThreshold(float baseThreshold, const uint8_t* frameData, const FrameMetadata& meta);
};

} // namespace datalake
