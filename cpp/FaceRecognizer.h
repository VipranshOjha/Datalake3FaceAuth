/**
 * FaceRecognizer.h
 * * Datalake 3.0 — MobileFaceNet Inference
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "FaceAuthHostObject.h"
#include "LivenessDetector.h"
#include "ModelManager.h"

namespace Ort {
    struct Session;
}

namespace datalake {

class FaceRecognizer {
public:
    FaceRecognizer(const std::string& modelPath, ModelManager* manager);
    ~FaceRecognizer();

    std::vector<float> getEmbedding(const uint8_t* frameData, const FrameMetadata& meta, const std::vector<Point2D>& landmarks);

private:
    std::unique_ptr<Ort::Session> session_;
};

float cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2);

} // namespace datalake
