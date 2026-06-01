/**
 * FaceLandmarker.h
 * * Datalake 3.0 — 68-point Landmark Inference
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "FaceDetector.h"
#include "LivenessDetector.h"
#include "ModelManager.h"

namespace Ort {
    struct Session;
}

namespace datalake {

class FaceLandmarker {
public:
    FaceLandmarker(const std::string& modelPath, ModelManager* manager);
    ~FaceLandmarker();

    std::vector<Point2D> extract(const uint8_t* frameData, const FrameMetadata& meta, const BoundingBox& bbox);

private:
    std::unique_ptr<Ort::Session> session_;
};

} // namespace datalake
