/**
 * MonotonicClock.cpp
 * * Datalake 3.0 — Anti-Tamper Relative Clock
 * Hackathon 7.0 | NHAI
 * */
#include "MonotonicClock.h"

#ifdef __ANDROID__
#include <time.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <chrono>
#endif

namespace datalake {

MonotonicClock::MonotonicClock() {}

int64_t MonotonicClock::getTimestampMs() {
#ifdef __ANDROID__
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
#elif defined(__APPLE__)
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t time = mach_absolute_time();
    return (time * timebase.numer / timebase.denom) / 1000000LL;
#else
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
#endif
}

} // namespace datalake
