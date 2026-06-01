/**
 * CircularFrameBuffer.h
 * * Datalake 3.0 — Lock-Free Ring Buffer
 * Hackathon 7.0 | NHAI
 * */
#pragma once

#include <atomic>
#include <vector>
#include <cstdint>
#include "FaceAuthHostObject.h" // For FrameState, FrameMetadata

namespace datalake {

struct FrameSlot {
    std::atomic<FrameState> state{FrameState::EMPTY};
    FrameMetadata metadata;
    std::vector<uint8_t> data;
};

class CircularFrameBuffer {
public:
    // Pre-allocates 5 slots of size 640x480x3
    CircularFrameBuffer();
    ~CircularFrameBuffer() = default;

    // Non-copyable, non-movable
    CircularFrameBuffer(const CircularFrameBuffer&) = delete;
    CircularFrameBuffer& operator=(const CircularFrameBuffer&) = delete;

    // // Producer API (Camera Thread)
    // /**
     * Scans for the oldest slot that is NOT LOCKED_FOR_INFERENCE.
     * Atomically flips it to WRITING, performs memcpy of raw data,
     * and flips it to READY_FOR_INFERENCE.
     * @return true if pushed successfully, false if buffer is full.
     */
    bool pushFrame(const uint8_t* rawData, size_t dataSize, const FrameMetadata& metadata);

    // // Consumer API (Worker Thread)
    // /**
     * Finds the newest frame flagged READY_FOR_INFERENCE.
     * Atomically flips it to LOCKED_FOR_INFERENCE to execute inference.
     * @return Pointer to the locked slot, or nullptr if none available.
     */
    FrameSlot* acquireLatestFrame();

    /**
     * Resets a slot back to EMPTY once inference is complete.
     */
    void releaseFrame(FrameSlot* slot);

private:
    std::vector<FrameSlot> slots_;
    static constexpr size_t CAPACITY = 5;
    static constexpr size_t SLOT_SIZE = 640 * 480 * 3; // ~921.6 KB per slot
};

} // namespace datalake
