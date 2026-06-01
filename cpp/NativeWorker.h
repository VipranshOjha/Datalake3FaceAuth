/**
 * NativeWorker.h
 * * Datalake 3.0 — High-Priority Background Worker Thread
 * Hackathon 7.0 | NHAI
 * *
 * Dedicated background thread for AI inference workloads. Isolates all
 * heavy computation (face detection, landmark extraction, liveness
 * checks, embedding generation) from the React Native JS thread.
 *
 * DESIGN DECISIONS:
 *   1. Single worker thread (not a pool) — the AI pipeline is strictly
 *      sequential per frame, so parallelism within a single pipeline
 *      execution adds complexity without benefit on mobile ARM.
 *   2. Platform-specific priority boost ensures inference tasks don't
 *      get starved by lower-priority system threads.
 *   3. Task queue with condition_variable — zero-spin waiting when idle.
 *   4. Graceful shutdown drains the queue before terminating.
 *
 * THREAD SAFETY:
 *   - enqueue() is safe to call from any thread.
 *   - All internal state is mutex-protected.
 *   - The worker thread NEVER touches jsi::Runtime.
 */

#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <cstddef>
#include <string>

namespace datalake {

class NativeWorker {
public:
    /**
     * Construct and immediately spawn the worker thread.
     * The thread is boosted to high priority using platform APIs.
     *
     * @param name  Human-readable thread name for debugger/profiler
     */
    explicit NativeWorker(const std::string& name = "DatalakeFaceAuthWorker");

    /**
     * Destructor — calls shutdown() if the thread is still alive.
     */
    ~NativeWorker();

    // Non-copyable, non-movable (owns a running thread)
    NativeWorker(const NativeWorker&) = delete;
    NativeWorker& operator=(const NativeWorker&) = delete;
    NativeWorker(NativeWorker&&) = delete;
    NativeWorker& operator=(NativeWorker&&) = delete;

    /**
     * Enqueue a task for execution on the background thread.
     * Thread-safe — can be called from the JS thread, camera thread, etc.
     *
     * @param task  The work to execute (must NOT access jsi::Runtime)
     */
    void enqueue(std::function<void()> task);

    /**
     * Gracefully shut down the worker.
     * Signals the thread to stop and waits for it to join.
     * Any tasks remaining in the queue are drained (executed) before exit.
     */
    void shutdown();

    /**
     * Check if the worker thread is alive and accepting tasks.
     */
    bool isRunning() const;

    /**
     * Return the number of tasks waiting in the queue.
     * Note: this is approximate due to potential race conditions.
     */
    size_t pendingTasks() const;

    /**
     * Get the total number of tasks successfully executed since startup.
     */
    uint64_t completedTasks() const;

    /**
     * Get the average task execution time in milliseconds.
     */
    double averageTaskTimeMs() const;

private:
    /**
     * Main loop — runs on the dedicated background thread.
     * Waits on the condition variable, dequeues tasks, and executes them
     * with performance timing instrumentation.
     */
    void workerLoop();

    /**
     * Set platform-specific thread priority and name.
     * Called once at the start of workerLoop().
     *
     * Android: setpriority(PRIO_PROCESS, 0, -10) for high-priority
     *          + pthread_setname_np for debugger visibility
     * iOS:     pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED)
     *          + pthread_setname_np for Instruments visibility
     */
    void applyThreadPriority();

    // Internal state
    std::string                         threadName_;
    std::thread                         thread_;
    mutable std::mutex                  mutex_;
    std::condition_variable             cv_;
    std::queue<std::function<void()>>   taskQueue_;

    std::atomic<bool>                   running_{false};
    std::atomic<bool>                   shutdownRequested_{false};

    // Performance counters
    std::atomic<uint64_t>               totalTasksCompleted_{0};
    std::atomic<double>                 cumulativeTaskTimeMs_{0.0};
};

}  // namespace datalake
