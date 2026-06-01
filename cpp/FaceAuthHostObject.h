/**
 * FaceAuthHostObject.h
 * * Datalake 3.0 — Offline Face Recognition & Liveness Detection Module
 * Hackathon 7.0 | NHAI
 * *
 * JSI HostObject that exposes the native face authentication pipeline
 * to the React Native JavaScript runtime. This is the single entry
 * point for all JS ↔ Native communication.
 *
 * THREAD SAFETY CONTRACT:
 *   - All jsi::Runtime interactions happen ONLY on the JS thread.
 *   - Heavy work is dispatched to NativeWorker (background thread).
 *   - Results return to JS via CallInvoker::invokeAsync().
 *   - Background lambdas capture ONLY plain C++ types (no jsi objects).
 *
 * MEMORY CONTRACT:
 *   - Zero heap allocations on hot paths.
 *   - All pipeline components are pre-allocated at init time.
 *   - Shared ownership via std::shared_ptr for cross-thread safety.
 */

#pragma once

#include <jsi/jsi.h>
#include <ReactCommon/CallInvoker.h>

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

// // Forward declarations — no header coupling to pipeline internals
// namespace datalake {
class NativeWorker;
class CircularFrameBuffer;
class FaceDetector;
class FaceLandmarker;
class LivenessDetector;
class FaceRecognizer;
class ExposureCompensator;
class ModelManager;
class StorageEngine;
class SyncManager;
class MonotonicClock;

// // Shared Frame State Contract (Thread-Isolation Memory Protocol)
// //
// These types define the cross-thread memory isolation contract between
// the camera ingestion thread (producer) and the NativeWorker inference
// thread (consumer) for the CircularFrameBuffer.
//
// TRANSITION RULES (enforced via atomic CAS):
//
//   Camera thread:  EMPTY → WRITING → READY_FOR_INFERENCE
//                   (skips slots in LOCKED_FOR_INFERENCE — no blocking)
//
//   Worker thread:  READY_FOR_INFERENCE → LOCKED_FOR_INFERENCE → EMPTY
//                   (releases slot after inference completes)
//
// This guarantees zero data corruption without mutex contention on
// the camera hot path (critical for maintaining 30fps on 3GB RAM devices).
// /**
 * FrameState — atomic state flag for each circular buffer slot.
 * uint8_t backing for minimal cache line pressure across 5 slots.
 */
enum class FrameState : uint8_t {
    EMPTY,                  ///< Slot contains no valid data. Camera thread may write.
    WRITING,                ///< Camera thread is actively writing frame data.
    READY_FOR_INFERENCE,    ///< Frame data is complete. Worker thread may claim.
    LOCKED_FOR_INFERENCE    ///< Worker thread has claimed this slot for inference.
                            ///< Camera thread MUST skip this slot.
};

/**
 * PixelFormat — supported camera frame pixel formats.
 * Used by PlatformImageHandler to select the correct conversion path.
 */
enum class PixelFormat : uint8_t {
    NV21,                   ///< Android default (YCrCb 4:2:0 semi-planar)
    NV12,                   ///< iOS default (YCbCr 4:2:0 semi-planar)
    YUV_420_888,            ///< Android Camera2 API flexible YUV
    BGRA_8888,              ///< iOS alternative / preview format
    RGB_888                 ///< Post-conversion working format
};

/**
 * FrameMetadata — lightweight descriptor for a buffered frame.
 * Travels with the frame data pointer through the pipeline.
 * No heap allocations — all fixed-size fields.
 */
struct FrameMetadata {
    int width              = 0;
    int height             = 0;
    PixelFormat format     = PixelFormat::NV21;
    int64_t timestampMs    = 0;      ///< Monotonic timestamp at capture
    int rotation           = 0;      ///< Degrees (0, 90, 180, 270)
    bool isFrontCamera     = true;
};

// // Result & Status Structures (plain C++ — safe to cross thread boundary)
// /**
 * VerificationResult — returned from the complete pipeline execution.
 * Contains only metadata; NEVER contains raw image data or embeddings.
 */
struct VerificationResult {
    bool success           = false;
    float confidence       = 0.0f;
    std::string userId;
    std::string livenessStatus;   // "PASSED", "FAILED_BLINK", "FAILED_SMILE", "TIMEOUT", "SPOOF_DETECTED"
    int64_t timestampMonotonic = 0;
    double latitude        = 0.0;
    double longitude       = 0.0;
    std::string authToken;
};

/**
 * ModuleStatus — health check snapshot for diagnostics.
 */
struct ModuleStatus {
    bool modelsLoaded      = false;
    int bufferSlotsFree    = 0;
    int64_t clockOffsetMs  = 0;
    int pendingSyncCount   = 0;
    int enrolledUserCount  = 0;
    bool workerAlive       = false;
};

/**
 * LivenessChallengeSequence — randomized challenge order for anti-spoof.
 */
struct LivenessChallengeSequence {
    std::vector<std::string> challenges;  // e.g., ["BLINK", "SMILE"] or ["SMILE", "BLINK"]
    int64_t issuedAtMonotonic = 0;
    int timeoutMs          = 10000;       // 10 second timeout per challenge
};

// // JSI HostObject — Primary Bridge
// class FaceAuthHostObject : public facebook::jsi::HostObject {
public:
    /**
     * Construct with the JS thread's CallInvoker for safe async callbacks.
     * All pipeline components are lazily initialized via initializeModels().
     */
    explicit FaceAuthHostObject(
        std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker);

    ~FaceAuthHostObject() override;

    // JSI HostObject interface
    /**
     * Property getter — routes JS property access to native implementations.
     * Maps: nativeVerifyUser, initializeModels, getModuleStatus,
     *        startLivenessChallenge, enrollUser, setSyncEndpoint
     */
    facebook::jsi::Value get(
        facebook::jsi::Runtime& rt,
        const facebook::jsi::PropNameID& name) override;

    /**
     * Property setter — currently no writable properties.
     */
    void set(
        facebook::jsi::Runtime& rt,
        const facebook::jsi::PropNameID& name,
        const facebook::jsi::Value& value) override;

    /**
     * Enumerate available methods for JS introspection.
     */
    std::vector<facebook::jsi::PropNameID> getPropertyNames(
        facebook::jsi::Runtime& rt) override;

private:
    // Pipeline components (shared ownership for thread safety)
    std::shared_ptr<facebook::react::CallInvoker> callInvoker_;
    std::shared_ptr<NativeWorker>          worker_;
    std::shared_ptr<CircularFrameBuffer>   frameBuffer_;
    std::shared_ptr<ModelManager>          modelManager_;
    std::shared_ptr<FaceDetector>          faceDetector_;
    std::shared_ptr<FaceLandmarker>        faceLandmarker_;
    std::shared_ptr<LivenessDetector>      livenessDetector_;
    std::shared_ptr<FaceRecognizer>        faceRecognizer_;
    std::shared_ptr<ExposureCompensator>   exposureCompensator_;
    std::shared_ptr<StorageEngine>         storageEngine_;
    std::shared_ptr<SyncManager>           syncManager_;
    std::shared_ptr<MonotonicClock>        monotonicClock_;

    // Internal state
    std::atomic<bool> modelsInitialized_{false};
    std::string syncEndpointUrl_;

    // Enrolled user embedding cache (brute-force, up to 5000)
    struct EnrolledUser {
        std::string userId;
        std::vector<float> embedding;  // 512D feature vector
    };
    std::vector<EnrolledUser> enrolledUsers_;
    mutable std::mutex enrolledUsersMutex_;

    // Internal method implementations
    /**
     * Execute the full verification pipeline on the background thread.
     * Called from nativeVerifyUser() after dispatching to NativeWorker.
     */
    VerificationResult executePipeline(const std::string& userId);

    /**
     * Convert a VerificationResult to a JSI Object on the JS thread.
     */
    static facebook::jsi::Object resultToJSI(
        facebook::jsi::Runtime& rt,
        const VerificationResult& result);

    /**
     * Convert a ModuleStatus to a JSI Object on the JS thread.
     */
    static facebook::jsi::Object statusToJSI(
        facebook::jsi::Runtime& rt,
        const ModuleStatus& status);
};

// // Install function — called from platform-specific glue (JNI / ObjC++)
// /**
 * Install the FaceAuth JSI module into the JavaScript runtime.
 * Sets global.__FaceAuthModule as an accessor for the HostObject.
 *
 * @param runtime   The JSI runtime (obtained from platform bridge)
 * @param callInvoker  The JS thread CallInvoker for async callbacks
 */
void installFaceAuth(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<facebook::react::CallInvoker> callInvoker);

}  // namespace datalake
