/**
 * LivenessDetector.cpp
 * * Datalake 3.0 — Active Liveness Detection
 * Hackathon 7.0 | NHAI
 * */
#include "LivenessDetector.h"
#include <cmath>

namespace datalake {

LivenessDetector::LivenessDetector() {}

void LivenessDetector::reset(const std::vector<std::string>& challenges, int64_t issuedAtMonotonic) {
    challenges_ = challenges;
    issuedAtMonotonic_ = issuedAtMonotonic;
    currentChallengeIdx_ = 0;
    hasInitialState_ = false;
    framesInState_ = 0;
}

float LivenessDetector::calculateEAR(const std::vector<Point2D>& landmarks, bool leftEye) {
    int start = leftEye ? 36 : 42;
    if (landmarks.size() < 68) return 0.0f;

    auto dist = [](const Point2D& p1, const Point2D& p2) {
        return std::sqrt(std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2));
    };

    // p1 = start, p2 = start+1, p3 = start+2, p4 = start+3, p5 = start+4, p6 = start+5
    // EAR = (||p2 - p6|| + ||p3 - p5||) / (2 * ||p1 - p4||)
    float v1 = dist(landmarks[start + 1], landmarks[start + 5]);
    float v2 = dist(landmarks[start + 2], landmarks[start + 4]);
    float h = dist(landmarks[start], landmarks[start + 3]);

    return (v1 + v2) / (2.0f * h + 1e-6f);
}

float LivenessDetector::calculateMAR(const std::vector<Point2D>& landmarks) {
    if (landmarks.size() < 68) return 0.0f;

    auto dist = [](const Point2D& p1, const Point2D& p2) {
        return std::sqrt(std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2));
    };

    // MAR tracks inner lip vertical expansion over horizontal baseline
    // Inner mouth: 60 to 67
    // V1 = ||p61 - p67||, V2 = ||p62 - p66||, V3 = ||p63 - p65||
    // H = ||p60 - p64||
    // MAR = (V1 + V2 + V3) / (3 * H)
    float v1 = dist(landmarks[61], landmarks[67]);
    float v2 = dist(landmarks[62], landmarks[66]);
    float v3 = dist(landmarks[63], landmarks[65]);
    float h = dist(landmarks[60], landmarks[64]);

    return (v1 + v2 + v3) / (3.0f * h + 1e-6f);
}

LivenessResult LivenessDetector::update(const std::vector<Point2D>& landmarks, int64_t currentTimestampMs) {
    LivenessResult result;

    if (challenges_.empty() || currentChallengeIdx_ >= challenges_.size()) {
        result.passed = true;
        result.isComplete = true;
        return result;
    }

    if (currentTimestampMs - issuedAtMonotonic_ > 10000) { // 10s timeout
        result.passed = false;
        result.failureReason = "TIMEOUT";
        result.isComplete = true;
        return result;
    }

    if (landmarks.size() < 68) {
        result.passed = false;
        result.failureReason = "NO_FACE";
        return result;
    }

    float earLeft = calculateEAR(landmarks, true);
    float earRight = calculateEAR(landmarks, false);
    float avgEAR = (earLeft + earRight) / 2.0f;
    float mar = calculateMAR(landmarks);

    // Enforce temporal bound: Baseline state is recorded AFTER the challenge timestamp
    if (!hasInitialState_) {
        initialEAR_ = avgEAR;
        initialMAR_ = mar;
        hasInitialState_ = true;
        framesInState_ = 0;
        result.passed = true;
        return result;
    }

    std::string currentChallenge = challenges_[currentChallengeIdx_];

    if (currentChallenge == "BLINK") {
        if (avgEAR < 0.2f && initialEAR_ > 0.25f) {
            framesInState_++;
            if (framesInState_ > 1) { // Debounce
                currentChallengeIdx_++;
                hasInitialState_ = false; // Reset for next
            }
        }
    } else if (currentChallenge == "SMILE") {
        if (mar > initialMAR_ + 0.15f) {
            framesInState_++;
            if (framesInState_ > 2) {
                currentChallengeIdx_++;
                hasInitialState_ = false;
            }
        }
    } else {
        currentChallengeIdx_++;
    }

    if (currentChallengeIdx_ >= challenges_.size()) {
        result.passed = true;
        result.isComplete = true;
    } else {
        result.passed = true;
        result.isComplete = false;
    }

    return result;
}

} // namespace datalake
