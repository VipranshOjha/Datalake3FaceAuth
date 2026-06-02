#pragma once
#include <vector>
#include <string>
#include <memory>

enum OrtLoggingLevel {
    ORT_LOGGING_LEVEL_VERBOSE,
    ORT_LOGGING_LEVEL_INFO,
    ORT_LOGGING_LEVEL_WARNING,
    ORT_LOGGING_LEVEL_ERROR,
    ORT_LOGGING_LEVEL_FATAL
};

enum GraphOptimizationLevel {
    ORT_DISABLE_ALL,
    ORT_ENABLE_BASIC,
    ORT_ENABLE_EXTENDED,
    ORT_ENABLE_ALL
};

enum OrtAllocatorType { OrtArenaAllocator };
enum OrtMemType { OrtMemTypeDefault };

namespace Ort {

struct Env {
    Env(OrtLoggingLevel, const char*) {}
};

struct SessionOptions {
    void SetIntraOpNumThreads(int) {}
    void SetInterOpNumThreads(int) {}
    void SetGraphOptimizationLevel(GraphOptimizationLevel) {}
};

struct Session {
    Session(const Env&, const char*, const SessionOptions&) {}
    Session(const Env&, const wchar_t*, const SessionOptions&) {}
};

struct MemoryInfo {
    static MemoryInfo CreateCpu(OrtAllocatorType, OrtMemType) { return {}; }
};

struct Value {
    template<typename T>
    static Value CreateTensor(const MemoryInfo&, T*, size_t, const int64_t*, size_t) { return {}; }
    size_t size() const { return 0; }
};

inline void ThrowOnError(int) {}

} // namespace Ort

inline int OrtSessionOptionsAppendExecutionProvider_Nnapi(Ort::SessionOptions&, int) { return 0; }
inline int OrtSessionOptionsAppendExecutionProvider_CoreML(Ort::SessionOptions&, int) { return 0; }
