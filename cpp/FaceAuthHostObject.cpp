/**
 * FaceAuthHostObject.cpp
 * * Datalake 3.0 — JSI HostObject Implementation
 * Hackathon 7.0 | NHAI
 * *
 * Full implementation of the FaceAuth JSI bridge. Routes JavaScript
 * property accesses to native C++ functions, manages the async pipeline
 * dispatch, and marshals results back to the JS thread.
 *
 * CRITICAL THREAD SAFETY RULES ENFORCED HERE:
 *   1. jsi::Runtime& is ONLY used inside get(), set(), and lambdas
 *      passed to CallInvoker::invokeAsync() (all on JS thread).
 *   2. CallInvoker::invokeAsync runs its lambda on the JS thread but
 *      does NOT provide a jsi::Runtime& parameter. To work around this,
 *      we capture a raw jsi::Runtime* inside the Promise executor
 *      (which runs on the JS thread and has `rt` in scope). Since
 *      invokeAsync also executes on the JS thread where the Runtime
 *      is alive, dereferencing this pointer is safe.
 *   3. Background lambdas (on NativeWorker) capture ONLY:
 *      - std::string (copied by value)
 *      - std::shared_ptr to pipeline components
 *      - Raw jsi::Runtime* (never dereferenced on BG thread)
 *      - std::shared_ptr to jsi::Function wrappers
 *   4. Raw jsi::Value, jsi::Object, jsi::String are NEVER captured.
 */

#include "FaceAuthHostObject.h"
#include "NativeWorker.h"

// Pipeline components
#include "CircularFrameBuffer.h"
#include "FaceDetector.h"
#include "FaceLandmarker.h"
#include "LivenessDetector.h"
#include "FaceRecognizer.h"
#include "ExposureCompensator.h"
#include "ModelManager.h"
#include "StorageEngine.h"
#include "SyncManager.h"
#include "MonotonicClock.h"

#include <chrono>
#include <algorithm>
#include <sstream>

// Android logging
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "DatalakeFaceAuth"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace datalake {

using namespace facebook::jsi;
using namespace facebook::react;

// // Construction / Destruction
// FaceAuthHostObject::FaceAuthHostObject(
    std::shared_ptr<CallInvoker> jsCallInvoker)
    : callInvoker_(std::move(jsCallInvoker))
{
    // Spawn the background worker thread immediately.
    // It sits idle until tasks are enqueued.
    worker_ = std::make_shared<NativeWorker>("DatalakeFaceAuth");

    LOGI("FaceAuthHostObject created. Worker thread spawned.\n");
}

FaceAuthHostObject::~FaceAuthHostObject() {
    if (worker_) {
        worker_->shutdown();
    }
    LOGI("FaceAuthHostObject destroyed.\n");
}

// // JSI Property Getter — Routes JS calls to native implementations
// Value FaceAuthHostObject::get(Runtime& rt, const PropNameID& name) {
    auto propName = name.utf8(rt);

    // // nativeVerifyUser(userId: string) → Promise<VerificationResult>
    // if (propName == "nativeVerifyUser") {
        return Function::createFromHostFunction(
            rt, name, 1,  // 1 argument: userId
            [this](Runtime& rt,
                   const Value& thisVal,
                   const Value* args,
                   size_t count) -> Value {

                if (count < 1 || !args[0].isString()) {
                    throw JSError(rt, "nativeVerifyUser requires a string userId argument");
                }

                // Extract plain C++ string ON the JS thread
                std::string userId = args[0].asString(rt).utf8(rt);

                if (!modelsInitialized_.load()) {
                    throw JSError(rt, "Models not initialized. Call initializeModels() first.");
                }

                // Create a JS Promise via the constructor
                auto promiseCtor = rt.global()
                    .getPropertyAsFunction(rt, "Promise");

                auto promise = promiseCtor.callAsConstructor(
                    rt,
                    Function::createFromHostFunction(
                        rt,
                        PropNameID::forAscii(rt, "executor"),
                        2,  // resolve, reject
                        [this, userId](
                            Runtime& rt,
                            const Value& thisVal,
                            const Value* args,
                            size_t count) -> Value {

                            // // PROMISE RESOLUTION STRATEGY:
                            //
                            // CallInvoker::invokeAsync() executes its
                            // lambda on the JS thread, but does NOT pass
                            // a jsi::Runtime& parameter. To call
                            // resolve->call(rt, ...) we need a Runtime&.
                            //
                            // Solution: Capture a raw Runtime* here in
                            // the executor (which runs on the JS thread
                            // and has `rt` in scope). invokeAsync also
                            // runs on the JS thread, so dereferencing
                            // the pointer is safe — same thread, Runtime
                            // is guaranteed alive while the bridge exists.
                            //
                            // This is the standard pattern used by
                            // react-native-mmkv, react-native-reanimated,
                            // and other production JSI modules.
                            // Runtime* rtPtr = &rt;
                            // Capture resolve/reject as shared Functions.
                            // shared_ptr because they must survive the
                            // background thread hop and be alive when
                            // invokeAsync fires on the JS thread.
                            auto resolve = std::make_shared<Function>(
                                args[0].asObject(rt).asFunction(rt));
                            auto reject = std::make_shared<Function>(
                                args[1].asObject(rt).asFunction(rt));

                            // Capture pipeline component shared_ptrs.
                            // These are safe to cross thread boundaries.
                            auto worker       = worker_;
                            auto callInvoker  = callInvoker_;
                            auto frameBuffer  = frameBuffer_;
                            auto faceDetector = faceDetector_;
                            auto landmarker   = faceLandmarker_;
                            auto liveness     = livenessDetector_;
                            auto recognizer   = faceRecognizer_;
                            auto exposure     = exposureCompensator_;
                            auto storage      = storageEngine_;
                            auto clock        = monotonicClock_;

                            // Snapshot enrolled users (deep copy) for
                            // thread-safe read access on the worker.
                            std::vector<EnrolledUser> enrolledSnapshot;
                            {
                                std::lock_guard<std::mutex> lock(enrolledUsersMutex_);
                                enrolledSnapshot = enrolledUsers_;
                            }

                            // Dispatch to background worker thread.
                            // This lambda runs on NativeWorker's thread.
                            // It MUST NOT touch jsi::Runtime or any
                            // jsi objects. Only plain C++ types.
                            worker->enqueue([
                                userId,
                                rtPtr,
                                resolve,
                                reject,
                                callInvoker,
                                frameBuffer,
                                faceDetector,
                                landmarker,
                                liveness,
                                recognizer,
                                exposure,
                                storage,
                                clock,
                                enrolledSnapshot = std::move(enrolledSnapshot)
                            ]() {
                                // // BACKGROUND THREAD — NO JSI ACCESS HERE
                                // // BACKGROUND THREAD
                                VerificationResult result = this->executePipeline(userId);

                                // // Return to JS thread via CallInvoker.
                                // invokeAsync runs this lambda on the JS
                                // thread where rtPtr is valid and alive.
                                // callInvoker->invokeAsync(
                                    [rtPtr, resolve, reject, result]() {
                                    // // JS THREAD — safe to use jsi::Runtime here
                                    // Runtime& rt = *rtPtr;
                                    try {
                                        // Marshal the C++ result into a
                                        // jsi::Object and resolve the Promise.
                                        auto jsResult = resultToJSI(rt, result);
                                        resolve->call(rt, std::move(jsResult));
                                    } catch (const std::exception& e) {
                                        // If marshalling fails, reject the
                                        // Promise with an error message.
                                        auto errorMsg = String::createFromUtf8(
                                            rt, std::string("Pipeline error: ") + e.what());
                                        reject->call(rt, std::move(errorMsg));
                                    }
                                });
                            });

                            return Value::undefined();
                        }
                    )
                );

                return promise;
            }
        );
    }

    // // initializeModels(modelDir: string) → boolean
    // if (propName == "initializeModels") {
        return Function::createFromHostFunction(
            rt, name, 1,
            [this](Runtime& rt,
                   const Value& thisVal,
                   const Value* args,
                   size_t count) -> Value {

                if (count < 1 || !args[0].isString()) {
                    throw JSError(rt, "initializeModels requires a string modelDir argument");
                }

                std::string modelDir = args[0].asString(rt).utf8(rt);

                LOGI("Initializing models from: %s\n", modelDir.c_str());

                modelManager_ = std::make_shared<ModelManager>();
                if (!modelManager_->initialize(modelDir)) {
                    throw JSError(rt, "Failed to initialize ModelManager");
                }

                faceDetector_ = std::make_shared<FaceDetector>(modelDir + "/blazeface_int8.onnx", modelManager_.get());
                faceLandmarker_ = std::make_shared<FaceLandmarker>(modelDir + "/landmark68_int8.onnx", modelManager_.get());
                faceRecognizer_ = std::make_shared<FaceRecognizer>(modelDir + "/mobilefacenet_int8.onnx", modelManager_.get());
                exposureCompensator_ = std::make_shared<ExposureCompensator>();
                livenessDetector_ = std::make_shared<LivenessDetector>();
                frameBuffer_ = std::make_shared<CircularFrameBuffer>();
                storageEngine_ = std::make_shared<StorageEngine>(modelDir + "/../databases");
                monotonicClock_ = std::make_shared<MonotonicClock>();
                syncManager_ = std::make_shared<SyncManager>(storageEngine_, monotonicClock_);

                modelsInitialized_.store(true);

                LOGI("Models initialized successfully.\n");
                return Value(true);
            }
        );
    }

    // // getModuleStatus() → ModuleStatus object
    // if (propName == "getModuleStatus") {
        return Function::createFromHostFunction(
            rt, name, 0,
            [this](Runtime& rt,
                   const Value& thisVal,
                   const Value* args,
                   size_t count) -> Value {

                ModuleStatus status;
                status.modelsLoaded = modelsInitialized_.load();
                status.workerAlive = worker_ ? worker_->isRunning() : false;
                status.bufferSlotsFree = 5;
                status.clockOffsetMs = 0;
                status.pendingSyncCount = 0;

                {
                    std::lock_guard<std::mutex> lock(enrolledUsersMutex_);
                    status.enrolledUserCount = static_cast<int>(enrolledUsers_.size());
                }

                return statusToJSI(rt, status);
            }
        );
    }

    // // startLivenessChallenge() → LivenessChallengeSequence object
    // if (propName == "startLivenessChallenge") {
        return Function::createFromHostFunction(
            rt, name, 0,
            [this](Runtime& rt,
                   const Value& thisVal,
                   const Value* args,
                   size_t count) -> Value {

                // Generate randomized challenge sequence
                LivenessChallengeSequence seq;
                seq.challenges = {"BLINK", "SMILE"};
                seq.timeoutMs = 10000;

                // Randomize order using monotonic clock as seed
                auto now = std::chrono::steady_clock::now();
                auto seed = static_cast<unsigned>(
                    now.time_since_epoch().count() & 0xFFFFFFFF);
                std::mt19937 rng(seed);
                std::shuffle(seq.challenges.begin(),
                             seq.challenges.end(), rng);

                seq.issuedAtMonotonic = std::chrono::duration_cast<
                    std::chrono::milliseconds>(now.time_since_epoch()).count();

                // livenessDetector_->reset(seq.challenges, seq.issuedAtMonotonic);

                // Build JSI response object
                Object result(rt);
                Array challengeArray(rt, seq.challenges.size());
                for (size_t i = 0; i < seq.challenges.size(); ++i) {
                    challengeArray.setValueAtIndex(
                        rt, i,
                        String::createFromUtf8(rt, seq.challenges[i]));
                }
                result.setProperty(rt, "challenges", std::move(challengeArray));
                result.setProperty(rt, "issuedAtMonotonic",
                    Value(static_cast<double>(seq.issuedAtMonotonic)));
                result.setProperty(rt, "timeoutMs",
                    Value(seq.timeoutMs));

                return std::move(result);
            }
        );
    }

    // // enrollUser(userId: string, featureData: ArrayBuffer) → boolean
    // if (propName == "enrollUser") {
        return Function::createFromHostFunction(
            rt, name, 2,
            [this](Runtime& rt,
                   const Value& thisVal,
                   const Value* args,
                   size_t count) -> Value {

                if (count < 2 || !args[0].isString()) {
                    throw JSError(rt, "enrollUser requires (userId: string, featureData: ArrayBuffer)");
                }

                std::string userId = args[0].asString(rt).utf8(rt);

                // Extract the ArrayBuffer containing the 512D float vector
                auto arrayBuffer = args[1].asObject(rt).getArrayBuffer(rt);
                const float* data = reinterpret_cast<const float*>(arrayBuffer.data(rt));
                size_t numFloats = arrayBuffer.size(rt) / sizeof(float);

                if (numFloats != 512) {
                    throw JSError(rt, "Feature vector must be 512-dimensional (512 floats)");
                }

                // Store in enrolled users cache
                EnrolledUser user;
                user.userId = userId;
                user.embedding.assign(data, data + numFloats);

                {
                    std::lock_guard<std::mutex> lock(enrolledUsersMutex_);

                    // Check capacity (max 5000 users)
                    if (enrolledUsers_.size() >= 5000) {
                        LOGE("Enrolled user cache full (5000). Cannot enroll %s\n",
                             userId.c_str());
                        return Value(false);
                    }

                    // Check for duplicate userId — update if exists
                    auto it = std::find_if(enrolledUsers_.begin(),
                                           enrolledUsers_.end(),
                                           [&userId](const EnrolledUser& u) {
                                               return u.userId == userId;
                                           });
                    if (it != enrolledUsers_.end()) {
                        it->embedding = std::move(user.embedding);
                        LOGI("Updated enrollment for user: %s\n", userId.c_str());
                    } else {
                        enrolledUsers_.push_back(std::move(user));
                        LOGI("Enrolled new user: %s (total: %zu)\n",
                             userId.c_str(), enrolledUsers_.size());
                    }
                }

                return Value(true);
            }
        );
    }

    // // setSyncEndpoint(url: string) → undefined
    // if (propName == "setSyncEndpoint") {
        return Function::createFromHostFunction(
            rt, name, 1,
            [this](Runtime& rt,
                   const Value& thisVal,
                   const Value* args,
                   size_t count) -> Value {

                if (count < 1 || !args[0].isString()) {
                    throw JSError(rt, "setSyncEndpoint requires a string URL argument");
                }

                syncEndpointUrl_ = args[0].asString(rt).utf8(rt);

                // syncManager_->setEndpoint(syncEndpointUrl_);

                LOGI("Sync endpoint set to: %s\n", syncEndpointUrl_.c_str());
                return Value::undefined();
            }
        );
    }

    // Unknown property
    return Value::undefined();
}

// // JSI Property Setter (no writable properties currently)
// void FaceAuthHostObject::set(
    Runtime& rt,
    const PropNameID& name,
    const Value& value)
{
    // No writable properties — silently ignore
}

// // Enumerate Properties
// std::vector<PropNameID> FaceAuthHostObject::getPropertyNames(Runtime& rt) {
    std::vector<PropNameID> props;
    props.push_back(PropNameID::forAscii(rt, "nativeVerifyUser"));
    props.push_back(PropNameID::forAscii(rt, "initializeModels"));
    props.push_back(PropNameID::forAscii(rt, "getModuleStatus"));
    props.push_back(PropNameID::forAscii(rt, "startLivenessChallenge"));
    props.push_back(PropNameID::forAscii(rt, "enrollUser"));
    props.push_back(PropNameID::forAscii(rt, "setSyncEndpoint"));
    return props;
}

// // Pipeline Execution (Background Thread)
// VerificationResult FaceAuthHostObject::executePipeline(
    const std::string& userId)
{
    VerificationResult result;
    result.userId = userId;

    auto pipelineStart = std::chrono::high_resolution_clock::now();

    int64_t currentTimestamp = monotonicClock_ ? monotonicClock_->getTimestampMs() : 0;

    // Step 1: Acquire latest frame from CircularFrameBuffer
    auto frame = frameBuffer_ ? frameBuffer_->acquireLatestFrame() : nullptr;
    if (!frame) { result.livenessStatus = "NO_FRAME"; return result; }

    // Step 2: Face Detection (BlazeFace)
    auto detection = faceDetector_ ? faceDetector_->detect(frame->data.data(), frame->metadata) : DetectionResult();
    if (!detection.faceFound) { 
        result.livenessStatus = "NO_FACE"; 
        frameBuffer_->releaseFrame(frame);
        return result; 
    }

    // Step 3: Landmark Extraction (68-point)
    auto landmarks = faceLandmarker_ ? faceLandmarker_->extract(frame->data.data(), frame->metadata, detection.bbox) : std::vector<Point2D>();
    if (landmarks.empty()) {
        result.livenessStatus = "LANDMARK_FAIL";
        frameBuffer_->releaseFrame(frame);
        return result;
    }

    // Step 4: Liveness Verification (EAR/MAR temporal delta)
    if (livenessDetector_) {
        auto livenessResult = livenessDetector_->update(landmarks, currentTimestamp);
        if (!livenessResult.passed) {
            result.livenessStatus = livenessResult.failureReason;
            frameBuffer_->releaseFrame(frame);
            return result;
        }
        if (!livenessResult.isComplete) {
            result.livenessStatus = "CHALLENGE_IN_PROGRESS";
            frameBuffer_->releaseFrame(frame);
            return result;
        }
    }

    // Step 5: Face Alignment & Feature Extraction
    auto embedding = faceRecognizer_ ? faceRecognizer_->getEmbedding(frame->data.data(), frame->metadata, landmarks) : std::vector<float>();

    // Step 6: Exposure Compensation & Threshold Adjustment
    float threshold = 0.60f; // baseline threshold
    if (exposureCompensator_) {
        threshold = exposureCompensator_->adjustThreshold(
            0.60f, frame->data.data(), frame->metadata);
    }

    // Release frame after extraction is complete
    if (frameBuffer_) frameBuffer_->releaseFrame(frame);

    // Step 7: Brute-force Cosine Similarity (up to 5000)
    float bestScore = -1.0f;
    std::string bestUserId;
    {
        std::lock_guard<std::mutex> lock(enrolledUsersMutex_);
        for (const auto& enrolled : enrolledUsers_) {
            float score = cosineSimilarity(embedding, enrolled.embedding);
            if (score > bestScore) {
                bestScore = score;
                bestUserId = enrolled.userId;
            }
        }
    }

    // Compare against requested user
    result.success = (bestUserId == userId && bestScore >= threshold);
    result.confidence = bestScore;
    result.livenessStatus = "PASSED";
    result.timestampMonotonic = currentTimestamp;

    // Step 8: Store result via StorageEngine
    if (storageEngine_) {
        storageEngine_->saveRecord(result);
    }

    auto pipelineEnd = std::chrono::high_resolution_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        pipelineEnd - pipelineStart).count();

    LOGI("Pipeline completed in %lld ms for user: %s\n",
         static_cast<long long>(durationMs), userId.c_str());

    return result;
}

// // JSI Object Marshalling Helpers (JS thread only)
// Object FaceAuthHostObject::resultToJSI(
    Runtime& rt,
    const VerificationResult& result)
{
    Object obj(rt);
    obj.setProperty(rt, "success", Value(result.success));
    obj.setProperty(rt, "confidence", Value(static_cast<double>(result.confidence)));
    obj.setProperty(rt, "userId",
        String::createFromUtf8(rt, result.userId));
    obj.setProperty(rt, "livenessStatus",
        String::createFromUtf8(rt, result.livenessStatus));
    obj.setProperty(rt, "timestampMonotonic",
        Value(static_cast<double>(result.timestampMonotonic)));
    obj.setProperty(rt, "latitude",
        Value(result.latitude));
    obj.setProperty(rt, "longitude",
        Value(result.longitude));
    return obj;
}

Object FaceAuthHostObject::statusToJSI(
    Runtime& rt,
    const ModuleStatus& status)
{
    Object obj(rt);
    obj.setProperty(rt, "modelsLoaded", Value(status.modelsLoaded));
    obj.setProperty(rt, "bufferSlotsFree", Value(status.bufferSlotsFree));
    obj.setProperty(rt, "clockOffsetMs",
        Value(static_cast<double>(status.clockOffsetMs)));
    obj.setProperty(rt, "pendingSyncCount", Value(status.pendingSyncCount));
    obj.setProperty(rt, "enrolledUserCount", Value(status.enrolledUserCount));
    obj.setProperty(rt, "workerAlive", Value(status.workerAlive));
    return obj;
}

// // Install — Called from platform glue (JNI / ObjC++)
// void installFaceAuth(
    Runtime& runtime,
    std::shared_ptr<CallInvoker> callInvoker)
{
    LOGI("Installing FaceAuth JSI module...\n");

    // Create the HostObject and attach to the global JS scope
    auto hostObject = std::make_shared<FaceAuthHostObject>(
        std::move(callInvoker));

    auto jsiObject = Object::createFromHostObject(runtime, hostObject);

    runtime.global().setProperty(
        runtime,
        "__FaceAuthModule",
        std::move(jsiObject));

    LOGI("FaceAuth JSI module installed at global.__FaceAuthModule\n");
}

}  // namespace datalake
