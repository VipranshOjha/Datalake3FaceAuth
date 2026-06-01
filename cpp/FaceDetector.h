/**
 * FaceDetector.h
 * * Datalake 3.0 — BlazeFace Inference wrapper
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "FaceAuthHostObject.h"
#include "ModelManager.h"

// Forward declare ONNX Runtime types to avoid heavy includes in headers
namespace Ort {
    struct Session;
}

namespace datalake {

struct BoundingBox {
    float x1, y1, x2, y2;
    float score;
};

struct DetectionResult {
    bool faceFound = false;
    BoundingBox bbox;
};

class FaceDetector {
public:
    FaceDetector(const std::string& modelPath, ModelManager* manager);
    ~FaceDetector();

    DetectionResult detect(const uint8_t* frameData, const FrameMetadata& meta);

private:
    std::unique_ptr<Ort::Session> session_;
};

} // namespace datalake
