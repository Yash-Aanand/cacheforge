#ifndef CACHEFORGE_SHARDED_STORAGE_H
#define CACHEFORGE_SHARDED_STORAGE_H

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace cacheforge {

struct Entry {
    std::string value;
    std::optional<std::chrono::steady_clock::time_point> expires_at;
    std::list<std::string>::iterator lru_iter;  // O(1) access to LRU position
};

class ShardedStorage {
public:
    static constexpr size_t NUM_SHARDS = 16;

    explicit ShardedStorage(size_t max_keys = 100000);
    ~ShardedStorage();

    // Disable copy
    ShardedStorage(const ShardedStorage&) = delete;
    ShardedStorage& operator=(const ShardedStorage&) = delete;

    void set(const std::string& key, const std::string& value);
    void setWithTTL(const std::string& key, const std::string& value, int64_t seconds);
    std::optional<std::string> get(const std::string& key);  // non-const for lazy expiration
    bool del(const std::string& key);
    size_t size() const;

    // TTL operations
    bool expire(const std::string& key, int64_t seconds);
    int64_t ttl(const std::string& key);

    // Expiration sweep control
    void startExpirationSweep();
    void stopExpirationSweep();

    // Metrics
    size_t expiredKeysCount() const { return expired_keys_.load(std::memory_order_relaxed); }
    size_t evictedKeysCount() const { return evicted_keys_.load(std::memory_order_relaxed); }

private:
    struct Shard {
        mutable std::mutex mutex;
        std::unordered_map<std::string, Entry> data;
        std::list<std::string> lru_order;  // front = MRU, back = LRU
    };

    // Get shard index using bitwise AND (faster than modulo for power of 2)
    size_t shardIndex(const std::string& key) const {
        return std::hash<std::string>{}(key) & (NUM_SHARDS - 1);
    }

    Shard& getShard(const std::string& key) {
        return shards_[shardIndex(key)];
    }

    bool isExpired(const Entry& entry) const {
        if (!entry.expires_at) return false;
        return std::chrono::steady_clock::now() >= *entry.expires_at;
    }

    void expirationLoop(std::stop_token stop_token);
    void sweepShard(Shard& shard);

    // Helper: remove expired entry from shard (assumes lock held, entry is expired)
    void removeExpiredEntry(Shard& shard, std::unordered_map<std::string, Entry>::iterator it);

    // Helper: evict LRU entries until shard has room for one more
    void evictIfNeeded(Shard& shard);

    // Helper: insert or update key in shard (assumes lock held)
    void insertOrUpdate(Shard& shard, const std::string& key, const std::string& value,
                        std::optional<std::chrono::steady_clock::time_point> expires_at);

    mutable std::array<Shard, NUM_SHARDS> shards_;
    std::jthread expiration_thread_;
    std::atomic<size_t> expired_keys_{0};
    std::atomic<size_t> evicted_keys_{0};
    size_t max_keys_;
    size_t max_keys_per_shard_;
};

} // namespace cacheforge

#endif // CACHEFORGE_SHARDED_STORAGE_H
