/**
 * LivenessDetector.h
 * * Datalake 3.0 — Active Liveness Detection
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace datalake {

struct Point2D {
    float x, y;
};

struct LivenessResult {
    bool passed = false;
    std::string failureReason; // e.g., "FAILED_BLINK", "TIMEOUT", "SPOOF_DETECTED"
    bool isComplete = false;   // true if the entire sequence is finished
};

class LivenessDetector {
public:
    LivenessDetector();
    ~LivenessDetector() = default;

    /**
     * Resets the state machine with a new challenge sequence.
     */
    void reset(const std::vector<std::string>& challenges, int64_t issuedAtMonotonic);

    /**
     * Updates the state machine with new landmarks.
     * @param landmarks Array of 68 facial landmarks.
     * @param currentTimestampMs Current monotonic time.
     * @return Current status of the liveness challenge.
     */
    LivenessResult update(const std::vector<Point2D>& landmarks, int64_t currentTimestampMs);

private:
    float calculateEAR(const std::vector<Point2D>& landmarks, bool leftEye);
    float calculateMAR(const std::vector<Point2D>& landmarks);

    std::vector<std::string> challenges_;
    size_t currentChallengeIdx_ = 0;
    int64_t issuedAtMonotonic_ = 0;

    // State tracking
    float initialEAR_ = 0.0f;
    float initialMAR_ = 0.0f;
    bool hasInitialState_ = false;
    int framesInState_ = 0;
};

} // namespace datalake
