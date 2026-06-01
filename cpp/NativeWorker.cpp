/**
 * NativeWorker.cpp
 * * Datalake 3.0 — High-Priority Background Worker Thread
 * Hackathon 7.0 | NHAI
 * *
 * Implementation of the dedicated AI inference worker thread.
 * All face detection, landmark extraction, liveness checking, and
 * embedding generation runs here — never on the JS thread.
 */

#include "NativeWorker.h"

#include <chrono>

// Platform-specific thread priority APIs
#ifdef __ANDROID__
#include <sys/resource.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#define LOG_TAG "DatalakeWorker"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#elif defined(__APPLE__)
#include <pthread.h>
#include <cstdio>
#define LOGI(...) fprintf(stdout, "[DatalakeWorker] " __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "[DatalakeWorker:WARN] " __VA_ARGS__)
#define LOGE(...) fprintf(stderr, "[DatalakeWorker:ERROR] " __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) fprintf(stdout, "[DatalakeWorker] " __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "[DatalakeWorker:WARN] " __VA_ARGS__)
#define LOGE(...) fprintf(stderr, "[DatalakeWorker:ERROR] " __VA_ARGS__)
#endif

namespace datalake {

// // Construction
// NativeWorker::NativeWorker(const std::string& name)
    : threadName_(name)
{
    running_.store(true);
    shutdownRequested_.store(false);

    // Spawn the worker thread immediately.
    // The thread will block on the condition variable until tasks arrive.
    thread_ = std::thread(&NativeWorker::workerLoop, this);

    LOGI("Worker thread '%s' spawned (tid will be set on thread start)\n",
         threadName_.c_str());
}

// // Destruction
// NativeWorker::~NativeWorker() {
    if (running_.load()) {
        shutdown();
    }
}

// // Enqueue — Thread-safe task submission
// void NativeWorker::enqueue(std::function<void()> task) {
    if (!running_.load()) {
        LOGW("Attempted to enqueue task on stopped worker '%s'\n",
             threadName_.c_str());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskQueue_.push(std::move(task));
    }

    // Wake the worker thread (single consumer, so notify_one is correct)
    cv_.notify_one();
}

// // Shutdown — Graceful termination with queue drain
// void NativeWorker::shutdown() {
    LOGI("Shutting down worker '%s'...\n", threadName_.c_str());

    shutdownRequested_.store(true);
    cv_.notify_all();

    if (thread_.joinable()) {
        thread_.join();
    }

    running_.store(false);

    LOGI("Worker '%s' shut down. Total tasks completed: %llu, avg time: %.2f ms\n",
         threadName_.c_str(),
         static_cast<unsigned long long>(totalTasksCompleted_.load()),
         averageTaskTimeMs());
}

// // Status Queries
// bool NativeWorker::isRunning() const {
    return running_.load();
}

size_t NativeWorker::pendingTasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return taskQueue_.size();
}

uint64_t NativeWorker::completedTasks() const {
    return totalTasksCompleted_.load();
}

double NativeWorker::averageTaskTimeMs() const {
    uint64_t completed = totalTasksCompleted_.load();
    if (completed == 0) return 0.0;
    return cumulativeTaskTimeMs_.load() / static_cast<double>(completed);
}

// // Worker Loop — Runs on the dedicated background thread
// void NativeWorker::workerLoop() {
    // Step 1: Apply platform-specific thread priority FIRST
    applyThreadPriority();

    LOGI("Worker loop started on thread '%s'\n", threadName_.c_str());

    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            // Wait until:
            //   (a) a task is available, OR
            //   (b) shutdown was requested
            cv_.wait(lock, [this] {
                return !taskQueue_.empty() || shutdownRequested_.load();
            });

            // If shutdown requested AND queue is empty, exit
            if (shutdownRequested_.load() && taskQueue_.empty()) {
                LOGI("Worker '%s': shutdown requested, queue drained. Exiting.\n",
                     threadName_.c_str());
                break;
            }

            // If shutdown requested but queue has tasks, drain them first
            if (taskQueue_.empty()) {
                continue;
            }

            task = std::move(taskQueue_.front());
            taskQueue_.pop();
        }

        // Execute the task outside the lock with performance timing
        auto start = std::chrono::high_resolution_clock::now();

        task();

        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(
            end - start).count();

        totalTasksCompleted_.fetch_add(1);

        // Atomic double add (relaxed ordering is fine for stats)
        double expected = cumulativeTaskTimeMs_.load();
        while (!cumulativeTaskTimeMs_.compare_exchange_weak(
            expected, expected + elapsedMs)) {
            // CAS retry loop for atomic double addition
        }

        LOGI("Task #%llu completed in %.2f ms (avg: %.2f ms)\n",
             static_cast<unsigned long long>(totalTasksCompleted_.load()),
             elapsedMs,
             averageTaskTimeMs());
    }

    LOGI("Worker loop exited for '%s'\n", threadName_.c_str());
}

// // Platform-Specific Thread Priority Boost
// void NativeWorker::applyThreadPriority() {
#ifdef __ANDROID__
    // // ANDROID: Boost thread priority for responsive AI inference
    // // Set thread name for debugger/profiler visibility
    pthread_setname_np(pthread_self(), threadName_.c_str());

    // Boost priority: -10 is "audio" level priority on Android.
    // Normal threads run at 0, UI thread at -4.
    // We use -10 to ensure AI inference doesn't get starved.
    int result = setpriority(PRIO_PROCESS, 0, -10);
    if (result != 0) {
        LOGW("Failed to set thread priority to -10 (errno: %d). "
             "Falling back to default priority.\n", errno);
    } else {
        LOGI("Android thread priority set to -10 (high)\n");
    }

    // Attempt to set SCHED_FIFO for real-time scheduling
    // This may fail without root, which is acceptable — we fall back
    // to the nice-based priority above.
    struct sched_param param;
    param.sched_priority = 1;  // Lowest FIFO priority
    result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (result != 0) {
        LOGW("Could not set SCHED_FIFO (expected on non-root). "
             "Using nice-based priority.\n");
    } else {
        LOGI("SCHED_FIFO enabled for worker thread.\n");
    }

#elif defined(__APPLE__)
    // // iOS: Use QoS class for system-level scheduling priority
    // // Set thread name for Instruments visibility
    pthread_setname_np(threadName_.c_str());

    // QOS_CLASS_USER_INITIATED: second-highest priority class.
    // Above QOS_CLASS_DEFAULT and QOS_CLASS_UTILITY.
    // Below QOS_CLASS_USER_INTERACTIVE (reserved for UI).
    int result = pthread_set_qos_class_self_np(
        QOS_CLASS_USER_INITIATED, 0);
    if (result != 0) {
        LOGW("Failed to set QOS_CLASS_USER_INITIATED (error: %d)\n", result);
    } else {
        LOGI("iOS QoS set to USER_INITIATED\n");
    }

#else
    // // Desktop / other: just set the thread name
    // LOGI("No platform-specific priority boost available.\n");
#endif
}

}  // namespace datalake
