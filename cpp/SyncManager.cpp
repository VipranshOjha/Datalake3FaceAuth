/**
 * SyncManager.cpp
 * * Datalake 3.0 — Two-Phase Commit Sync Engine
 * Hackathon 7.0 | NHAI
 * */
#include "SyncManager.h"

// Android logging fallback
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "SyncManager", __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#endif

namespace datalake {

SyncManager::SyncManager(std::shared_ptr<StorageEngine> storage, std::shared_ptr<MonotonicClock> clock)
    : storage_(storage), clock_(clock) {}

void SyncManager::setEndpoint(const std::string& url) {
    endpointUrl_ = url;
}

void SyncManager::triggerSync() {
    if (!storage_ || endpointUrl_.empty()) return;

    // PHASE 1 (State 1: SYNCING)
    // Query max 100 pending blobs to prevent buffer overflow constraints
    auto pending = storage_->getPendingRecords(100);
    if (pending.empty()) return;

    std::vector<int> recordIds;
    recordIds.reserve(pending.size());
    for (const auto& rec : pending) {
        recordIds.push_back(rec.id);
    }

    // Atomically transition from 0 (PENDING) -> 1 (SYNCING)
    storage_->updateSyncState(recordIds, 1);

    // Mock the outgoing vector transmission of datalake_proto_SyncPayload
    // network payload to AWS Gateway. Real implementation delegates libcurl post here.
    bool networkSuccess = true; // Simulated success signature

    // PHASE 2 (State 2: VERIFIED)
    if (networkSuccess) {
        // Validation token received; flip to 2 (VERIFIED)
        storage_->updateSyncState(recordIds, 2);

        // Execute clean cryptographic erasure
        storage_->purgeVerified();

        LOGI("Successfully synced and purged %zu records.\n", pending.size());
    } else {
        // Rollback state machine cleanly preventing data loss
        storage_->updateSyncState(recordIds, 0);
    }
}

} // namespace datalake
