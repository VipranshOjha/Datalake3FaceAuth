/**
 * CircularFrameBuffer.cpp
 * * Datalake 3.0 — Lock-Free Ring Buffer Implementation
 * Hackathon 7.0 | NHAI
 * */
#include "CircularFrameBuffer.h"
#include <cstring>
#include <iostream>

namespace datalake {

CircularFrameBuffer::CircularFrameBuffer() : slots_(CAPACITY) {
    for (auto& slot : slots_) {
        slot.data.resize(SLOT_SIZE);
        slot.state.store(FrameState::EMPTY, std::memory_order_relaxed);
    }
}

bool CircularFrameBuffer::pushFrame(const uint8_t* rawData, size_t dataSize, const FrameMetadata& metadata) {
    if (!rawData || dataSize > SLOT_SIZE) return false;

    // Find the oldest slot that is NOT LOCKED_FOR_INFERENCE.
    // We prefer EMPTY slots, but will overwrite the oldest READY_FOR_INFERENCE if necessary.
    int targetIdx = -1;
    int64_t oldestTimestamp = INT64_MAX;

    for (size_t i = 0; i < CAPACITY; ++i) {
        FrameState state = slots_[i].state.load(std::memory_order_acquire);
        if (state == FrameState::EMPTY) {
            targetIdx = i;
            break; // EMPTY is always best
        } else if (state == FrameState::READY_FOR_INFERENCE) {
            if (slots_[i].metadata.timestampMs < oldestTimestamp) {
                oldestTimestamp = slots_[i].metadata.timestampMs;
                targetIdx = i;
            }
        }
    }

    if (targetIdx == -1) {
        return false; // All slots are locked or writing (unlikely)
    }

    // Try to lock it for WRITING
    FrameState expected = slots_[targetIdx].state.load(std::memory_order_relaxed);
    if (expected == FrameState::LOCKED_FOR_INFERENCE || expected == FrameState::WRITING) {
        return false; // State changed concurrently
    }

    if (!slots_[targetIdx].state.compare_exchange_strong(expected, FrameState::WRITING, std::memory_order_acquire)) {
        return false; // CAS failed
    }

    // Now in WRITING state. Safe to memcpy. Zero-allocation on hot path.
    std::memcpy(slots_[targetIdx].data.data(), rawData, dataSize);
    slots_[targetIdx].metadata = metadata;

    // Flip to READY_FOR_INFERENCE
    slots_[targetIdx].state.store(FrameState::READY_FOR_INFERENCE, std::memory_order_release);
    return true;
}

FrameSlot* CircularFrameBuffer::acquireLatestFrame() {
    int bestIdx = -1;
    int64_t newestTimestamp = -1;

    // Find newest READY_FOR_INFERENCE
    for (size_t i = 0; i < CAPACITY; ++i) {
        if (slots_[i].state.load(std::memory_order_acquire) == FrameState::READY_FOR_INFERENCE) {
            if (slots_[i].metadata.timestampMs > newestTimestamp) {
                newestTimestamp = slots_[i].metadata.timestampMs;
                bestIdx = i;
            }
        }
    }

    if (bestIdx != -1) {
        FrameState expected = FrameState::READY_FOR_INFERENCE;
        if (slots_[bestIdx].state.compare_exchange_strong(expected, FrameState::LOCKED_FOR_INFERENCE, std::memory_order_acquire)) {
            // Aggressively clear older READY frames to EMPTY so camera thread can use them
            for (size_t i = 0; i < CAPACITY; ++i) {
                if (static_cast<int>(i) != bestIdx) {
                    FrameState olderExpected = FrameState::READY_FOR_INFERENCE;
                    slots_[i].state.compare_exchange_strong(olderExpected, FrameState::EMPTY, std::memory_order_release);
                }
            }
            return &slots_[bestIdx];
        }
    }
    return nullptr;
}

void CircularFrameBuffer::releaseFrame(FrameSlot* slot) {
    if (slot) {
        slot->state.store(FrameState::EMPTY, std::memory_order_release);
    }
}

} // namespace datalake
