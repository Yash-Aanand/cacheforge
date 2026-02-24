#include "storage/sharded_storage.h"

#include <algorithm>

namespace cacheforge {

ShardedStorage::ShardedStorage(size_t max_keys)
    : max_keys_(max_keys),
      max_keys_per_shard_(std::max(size_t{1}, max_keys / NUM_SHARDS)) {}

ShardedStorage::~ShardedStorage() {
    stopExpirationSweep();
}

void ShardedStorage::removeExpiredEntry(Shard& shard,
                                        std::unordered_map<std::string, Entry>::iterator it) {
    shard.lru_order.erase(it->second.lru_iter);
    shard.data.erase(it);
    expired_keys_.fetch_add(1, std::memory_order_relaxed);
}

void ShardedStorage::evictIfNeeded(Shard& shard) {
    while (shard.data.size() >= max_keys_per_shard_) {
        const std::string& lru_key = shard.lru_order.back();
        shard.data.erase(lru_key);
        shard.lru_order.pop_back();
        evicted_keys_.fetch_add(1, std::memory_order_relaxed);
    }
}

void ShardedStorage::insertOrUpdate(
    Shard& shard,
    const std::string& key,
    const std::string& value,
    std::optional<std::chrono::steady_clock::time_point> expires_at) {

    auto it = shard.data.find(key);
    if (it != shard.data.end()) {
        // Update existing key, move to front (MRU)
        it->second.value = value;
        it->second.expires_at = expires_at;
        shard.lru_order.splice(shard.lru_order.begin(), shard.lru_order, it->second.lru_iter);
    } else {
        // New key: evict if needed, then insert at front (MRU)
        evictIfNeeded(shard);
        shard.lru_order.push_front(key);
        shard.data[key] = Entry{value, expires_at, shard.lru_order.begin()};
    }
}

void ShardedStorage::set(const std::string& key, const std::string& value) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    insertOrUpdate(shard, key, value, std::nullopt);
}

void ShardedStorage::setWithTTL(const std::string& key, const std::string& value, int64_t seconds) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);
    auto expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    insertOrUpdate(shard, key, value, expires_at);
}

std::optional<std::string> ShardedStorage::get(const std::string& key) {
    Shard& shard = getShard(key);
    std::lock_guard<std::mutex> lock(shard.mutex);

    auto it = shard.data.find(key);
    if (it == shard.data.end()) {
        return std::nullopt;
    }
    if (isExpired(it->second)) {
        removeExpiredEntry(shard, it);
        return std::nullopt;
    }

    // Move to front (MRU)
    shard.lru_order.splice(shard.lru_order.begin(), shard.lru_order, it->second.lru_iter);
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
        removeExpiredEntry(shard, it);
        return false;  // Treat expired key as non-existent
    }

    shard.lru_order.erase(it->second.lru_iter);
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
        removeExpiredEntry(shard, it);
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
        removeExpiredEntry(shard, it);
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
    constexpr size_t MAX_SCAN_PER_SWEEP = 100;

    for (auto it = shard.data.begin(); it != shard.data.end() && scanned < MAX_SCAN_PER_SWEEP; ) {
        ++scanned;
        if (it->second.expires_at && now >= *it->second.expires_at) {
            shard.lru_order.erase(it->second.lru_iter);
            it = shard.data.erase(it);
            expired_keys_.fetch_add(1, std::memory_order_relaxed);
        } else {
            ++it;
        }
    }
}

} // namespace cacheforge
