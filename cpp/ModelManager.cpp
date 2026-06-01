/**
 * ModelManager.cpp
 * * Datalake 3.0 — ONNX Runtime Session Management
 * Hackathon 7.0 | NHAI
 * */
#include "ModelManager.h"
#include <onnxruntime_cxx_api.h>

// Android logging
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "DatalakeFaceAuth_Models"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace datalake {

ModelManager::ModelManager() {}
ModelManager::~ModelManager() {}

bool ModelManager::initialize(const std::string& modelDir) {
    if (initialized_) return true;

    LOGI("ModelManager initializing ONNX Runtime from %s\n", modelDir.c_str());

    try {
        // Initialize global Ort::Env session environment with ORT_LOGGING_LEVEL_WARNING
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "FaceAuth");

        sessionOptions_ = std::make_unique<Ort::SessionOptions>();

        // Force SetIntraOpNumThreads(1) and SetInterOpNumThreads(1) to prevent CPU thread starvation
        sessionOptions_->SetIntraOpNumThreads(1);
        sessionOptions_->SetInterOpNumThreads(1);

        // Enable total optimization
        sessionOptions_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Implement platform-specific hardware accelerators
#ifdef __ANDROID__
        try {
            // Append NNAPI for Android conditionally
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_Nnapi(*sessionOptions_, 0));
            LOGI("Successfully enabled NNAPI execution provider.\n");
        } catch (const std::exception& e) {
            LOGE("Failed to enable NNAPI, falling back to CPU: %s\n", e.what());
        }
#elif defined(__APPLE__)
        try {
            // Append CoreML for iOS conditionally
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CoreML(*sessionOptions_, 0));
            LOGI("Successfully enabled CoreML execution provider.\n");
        } catch (const std::exception& e) {
            LOGE("Failed to enable CoreML, falling back to CPU: %s\n", e.what());
        }
#endif

        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        LOGE("Error initializing ModelManager: %s\n", e.what());
        return false;
    }
}

} // namespace datalake
