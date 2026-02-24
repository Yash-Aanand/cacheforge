#include "storage/sharded_storage.h"

namespace cacheforge {

ShardedStorage::ShardedStorage() = default;

ShardedStorage::~ShardedStorage() {
    stopExpirationSweep();
}

void ShardedStorage::set(const std::string& key, const std::string& value) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    shard.data[key] = Entry{value, std::nullopt};
}

void ShardedStorage::setWithTTL(const std::string& key, const std::string& value, int64_t seconds) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    shard.data[key] = Entry{value, expires_at};
}

std::optional<std::string> ShardedStorage::get(const std::string& key) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.data.find(key);
    if (it == shard.data.end()) {
        return std::nullopt;
    }
    if (isExpired(it->second)) {
        shard.data.erase(it);
        expired_keys_.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }
    return it->second.value;
}

bool ShardedStorage::del(const std::string& key) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.data.find(key);
    if (it == shard.data.end()) {
        return false;
    }
    if (isExpired(it->second)) {
        shard.data.erase(it);
        expired_keys_.fetch_add(1, std::memory_order_relaxed);
        return false;  // Treat expired key as non-existent
    }
    shard.data.erase(it);
    return true;
}

size_t ShardedStorage::size() const {
    size_t total = 0;
    for (const auto& shard : shards_) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        total += shard.data.size();
    }
    return total;
}

bool ShardedStorage::expire(const std::string& key, int64_t seconds) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.data.find(key);
    if (it == shard.data.end()) {
        return false;
    }
    if (isExpired(it->second)) {
        shard.data.erase(it);
        expired_keys_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    it->second.expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    return true;
}

int64_t ShardedStorage::ttl(const std::string& key) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto it = shard.data.find(key);
    if (it == shard.data.end()) {
        return -2;  // Key doesn't exist
    }
    if (isExpired(it->second)) {
        shard.data.erase(it);
        expired_keys_.fetch_add(1, std::memory_order_relaxed);
        return -2;  // Key doesn't exist (expired)
    }
    if (!it->second.expires_at) {
        return -1;  // Key has no TTL
    }
    auto now = std::chrono::steady_clock::now();
    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
        *it->second.expires_at - now
    ).count();
    return remaining > 0 ? remaining : 0;
}

void ShardedStorage::startExpirationSweep() {
    if (!expiration_thread_.joinable()) {
        expiration_thread_ = std::jthread([this](std::stop_token stop_token) {
            expirationLoop(stop_token);
        });
    }
}

void ShardedStorage::stopExpirationSweep() {
    if (expiration_thread_.joinable()) {
        expiration_thread_.request_stop();
        expiration_thread_.join();
    }
}

void ShardedStorage::expirationLoop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        for (auto& shard : shards_) {
            if (stop_token.stop_requested()) break;
            sweepShard(shard);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void ShardedStorage::sweepShard(Shard& shard) {
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto now = std::chrono::steady_clock::now();
    size_t scanned = 0;
    for (auto it = shard.data.begin(); it != shard.data.end() && scanned < 100; ) {
        ++scanned;
        if (it->second.expires_at && now >= *it->second.expires_at) {
            it = shard.data.erase(it);
            expired_keys_.fetch_add(1, std::memory_order_relaxed);
        } else {
            ++it;
        }
    }
}

} // namespace cacheforge
