/**
 * MonotonicClock.h
 * * Datalake 3.0 — Anti-Tamper Relative Clock
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <cstdint>

namespace datalake {

class MonotonicClock {
public:
    MonotonicClock();
    ~MonotonicClock() = default;

    /**
     * Returns a monotonic timestamp (ms since boot).
     * This is highly resistant to user tampering (e.g., changing device time).
     */
    int64_t getTimestampMs();

private:
    int64_t ntpOffsetMs_ = 0; // Simulated offset from NTP sync
};

} // namespace datalake
