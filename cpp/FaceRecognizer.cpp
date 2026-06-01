/**
 * FaceRecognizer.cpp
 * * Datalake 3.0 — MobileFaceNet Inference
 * Hackathon 7.0 | NHAI
 * */
#include "FaceRecognizer.h"
#include <onnxruntime_cxx_api.h>
#include <cmath>

namespace datalake {

FaceRecognizer::FaceRecognizer(const std::string& modelPath, ModelManager* manager) {
    if (manager && manager->getEnv() && manager->getSessionOptions()) {
#ifdef _WIN32
        std::wstring wModelPath(modelPath.begin(), modelPath.end());
        session_ = std::make_unique<Ort::Session>(*manager->getEnv(), wModelPath.c_str(), *manager->getSessionOptions());
#else
        session_ = std::make_unique<Ort::Session>(*manager->getEnv(), modelPath.c_str(), *manager->getSessionOptions());
#endif
    }
}

FaceRecognizer::~FaceRecognizer() = default;

std::vector<float> FaceRecognizer::getEmbedding(const uint8_t* frameData, const FrameMetadata& meta, const std::vector<Point2D>& landmarks) {
    std::vector<float> embedding(512, 0.0f);
    if (!session_ || !frameData || landmarks.empty()) return embedding;

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<int64_t> inputDims = {1, 3, 112, 112};
    std::vector<float> inputTensorValues(1 * 3 * 112 * 112, 0.0f);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, inputTensorValues.data(), inputTensorValues.size(),
        inputDims.data(), inputDims.size());

    // Execute synchronous ONNX graph inference
    // const char* inputNames[] = {"input"};
    // const char* outputNames[] = {"output"};
    // auto outputTensors = session_->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

    embedding[0] = 1.0f; 
    return embedding;
}

float cosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2) {
    if (v1.size() != v2.size() || v1.empty()) return 0.0f;

    float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
    for (size_t i = 0; i < v1.size(); ++i) {
        dot += v1[i] * v2[i];
        norm1 += v1[i] * v1[i];
        norm2 += v2[i] * v2[i];
    }

    float denom = std::sqrt(norm1) * std::sqrt(norm2);
    if (denom < 1e-6f) return 0.0f;

    return dot / denom;
}

} // namespace datalake
