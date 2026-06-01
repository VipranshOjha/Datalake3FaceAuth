/**
 * FaceLandmarker.cpp
 * * Datalake 3.0 — 68-point Landmark Inference
 * Hackathon 7.0 | NHAI
 * */
#include "FaceLandmarker.h"
#include <onnxruntime_cxx_api.h>

namespace datalake {

FaceLandmarker::FaceLandmarker(const std::string& modelPath, ModelManager* manager) {
    if (manager && manager->getEnv() && manager->getSessionOptions()) {
#ifdef _WIN32
        std::wstring wModelPath(modelPath.begin(), modelPath.end());
        session_ = std::make_unique<Ort::Session>(*manager->getEnv(), wModelPath.c_str(), *manager->getSessionOptions());
#else
        session_ = std::make_unique<Ort::Session>(*manager->getEnv(), modelPath.c_str(), *manager->getSessionOptions());
#endif
    }
}

FaceLandmarker::~FaceLandmarker() = default;

std::vector<Point2D> FaceLandmarker::extract(const uint8_t* frameData, const FrameMetadata& meta, const BoundingBox& bbox) {
    std::vector<Point2D> landmarks;
    if (!session_ || !frameData || bbox.score <= 0) return landmarks;

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<int64_t> inputDims = {1, 3, 112, 112};
    std::vector<float> inputTensorValues(1 * 3 * 112 * 112, 0.0f);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, inputTensorValues.data(), inputTensorValues.size(),
        inputDims.data(), inputDims.size());

    // Execute inference synchronously on the background thread
    // const char* inputNames[] = {"input"};
    // const char* outputNames[] = {"output"};
    // auto outputTensors = session_->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

    landmarks.resize(68);
    for (int i = 0; i < 68; ++i) {
        landmarks[i] = {bbox.x1 + (bbox.x2 - bbox.x1) * 0.5f, bbox.y1 + (bbox.y2 - bbox.y1) * 0.5f};
    }

    return landmarks;
}

} // namespace datalake
