/**
 * ModelManager.h
 * * Datalake 3.0 — ONNX Runtime Session Management
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <string>
#include <memory>

// Forward declarations for ONNX Runtime C++ API
namespace Ort {
    struct Env;
    struct SessionOptions;
}

namespace datalake {

class ModelManager {
public:
    ModelManager();
    ~ModelManager();

    /**
     * Initializes the ONNX Runtime environment and configures session options.
     */
    bool initialize(const std::string& modelDir);

    Ort::Env* getEnv() const { return env_.get(); }
    Ort::SessionOptions* getSessionOptions() const { return sessionOptions_.get(); }

private:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::SessionOptions> sessionOptions_;
    bool initialized_ = false;
};

} // namespace datalake
