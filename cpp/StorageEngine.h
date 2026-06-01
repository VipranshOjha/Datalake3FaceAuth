/**
 * StorageEngine.h
 * * Datalake 3.0 — Offline SQLite Storage with nanopb BLOBs
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <string>
#include <vector>
#include "FaceAuthHostObject.h" // For VerificationResult

// Forward declare sqlite3
struct sqlite3;

namespace datalake {

struct SyncRecord {
    int id;
    std::string idempotencyKey;
    std::vector<uint8_t> blob;
};

class StorageEngine {
public:
    explicit StorageEngine(const std::string& dbPath);
    ~StorageEngine();

    /**
     * Serializes result via nanopb and stores binary BLOB with unique key.
     */
    bool saveRecord(const VerificationResult& result);

    /**
     * Fetch pending records for sync (sync_state = 0). Max default 100.
     */
    std::vector<SyncRecord> getPendingRecords(int limit = 100);

    /**
     * Atomically updates state for Two-Phase Commit:
     * 0 = PENDING, 1 = SYNCING, 2 = VERIFIED
     */
    bool updateSyncState(const std::vector<int>& recordIds, int state);

    /**
     * Executes clean cryptographic erasure of verified sync blobs.
     */
    void purgeVerified();

private:
    sqlite3* db_ = nullptr;
    std::string dbPath_;

    void initializeSchema();
};

} // namespace datalake
