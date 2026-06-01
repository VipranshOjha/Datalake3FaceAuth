/**
 * SyncManager.h
 * * Datalake 3.0 — Two-Phase Commit Sync Engine
 * Hackathon 7.0 | NHAI
 * */
#pragma once
#include <memory>
#include <string>
#include "StorageEngine.h"
#include "MonotonicClock.h"

namespace datalake {

class SyncManager {
public:
    SyncManager(std::shared_ptr<StorageEngine> storage, std::shared_ptr<MonotonicClock> clock);
    ~SyncManager() = default;

    void setEndpoint(const std::string& url);
    void triggerSync();

private:
    std::shared_ptr<StorageEngine> storage_;
    std::shared_ptr<MonotonicClock> clock_;
    std::string endpointUrl_;
};

} // namespace datalake
