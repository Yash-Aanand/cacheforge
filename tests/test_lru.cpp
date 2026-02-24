#include "storage/sharded_storage.h"
#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace cacheforge;

void testEvictionOrder() {
    std::cout << "testEvictionOrder... ";
    ShardedStorage storage(32);  // 2 per shard with 16 shards

    // Insert 40 keys
    for (int i = 0; i < 40; i++) {
        storage.set("key" + std::to_string(i), "value");
    }

    // Size should be <= 32
    assert(storage.size() <= 32);
    // Evicted count should be >= 8
    assert(storage.evictedKeysCount() >= 8);

    std::cout << "PASSED (size=" << storage.size()
              << ", evicted=" << storage.evictedKeysCount() << ")\n";
}

void testGetPreventsEviction() {
    std::cout << "testGetPreventsEviction... ";
    ShardedStorage storage(32);
    storage.set("protected", "value");

    // Insert many keys, touching "protected" periodically
    for (int i = 0; i < 100; i++) {
        storage.set("key" + std::to_string(i), "value");
        if (i % 10 == 0) storage.get("protected");
    }

    // "protected" should still exist
    assert(storage.get("protected").has_value());
    std::cout << "PASSED\n";
}

void testEvictedKeysCounter() {
    std::cout << "testEvictedKeysCounter... ";
    ShardedStorage storage(16);  // 1 per shard

    for (int i = 0; i < 32; i++) {
        storage.set("key" + std::to_string(i), "value");
    }

    // Should have evicted at least 16 keys
    assert(storage.evictedKeysCount() >= 16);
    std::cout << "PASSED (evicted=" << storage.evictedKeysCount() << ")\n";
}

void testUpdateExistingKeyNoEviction() {
    std::cout << "testUpdateExistingKeyNoEviction... ";
    ShardedStorage storage(64);  // 4 per shard - ensures no evictions during insert

    // Insert 16 keys (well under capacity)
    for (int i = 0; i < 16; i++) {
        storage.set("key" + std::to_string(i), "value1");
    }
    assert(storage.evictedKeysCount() == 0);  // Verify no evictions during insert

    // Update existing keys - should not cause evictions
    for (int i = 0; i < 16; i++) {
        storage.set("key" + std::to_string(i), "value2");
    }

    assert(storage.evictedKeysCount() == 0);  // Verify no evictions during updates
    assert(storage.size() == 16);
    std::cout << "PASSED\n";
}

void testDeleteRemovesFromLRU() {
    std::cout << "testDeleteRemovesFromLRU... ";
    ShardedStorage storage(32);

    // Insert keys
    for (int i = 0; i < 20; i++) {
        storage.set("key" + std::to_string(i), "value");
    }

    // Delete some keys
    for (int i = 0; i < 10; i++) {
        storage.del("key" + std::to_string(i));
    }

    assert(storage.size() == 10);

    // Insert more keys, should not crash or corrupt LRU
    for (int i = 100; i < 130; i++) {
        storage.set("key" + std::to_string(i), "value");
    }

    assert(storage.size() <= 32);
    std::cout << "PASSED\n";
}

void testLRUWithTTL() {
    std::cout << "testLRUWithTTL... ";
    ShardedStorage storage(32);

    // Mix of TTL and non-TTL keys
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            storage.setWithTTL("key" + std::to_string(i), "value", 3600);
        } else {
            storage.set("key" + std::to_string(i), "value");
        }
    }

    // Insert more to trigger eviction
    for (int i = 100; i < 150; i++) {
        storage.set("key" + std::to_string(i), "value");
    }

    assert(storage.size() <= 32);
    std::cout << "PASSED (size=" << storage.size()
              << ", evicted=" << storage.evictedKeysCount() << ")\n";
}

void testConcurrentLRU() {
    std::cout << "testConcurrentLRU... ";
    ShardedStorage storage(256);  // 16 per shard
    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 500;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                storage.set(key, "value");

                // Read to trigger LRU update
                storage.get(key);

                // Sometimes delete
                if (i % 5 == 0) {
                    storage.del(key);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Should have evicted many keys and maintained consistency
    assert(storage.size() <= 256);
    std::cout << "PASSED (size=" << storage.size()
              << ", evicted=" << storage.evictedKeysCount() << ")\n";
}

void testMinCapacity() {
    std::cout << "testMinCapacity... ";
    // Edge case: max_keys less than NUM_SHARDS
    ShardedStorage storage(8);  // Less than 16 shards

    // Should still work (1 per shard minimum)
    for (int i = 0; i < 32; i++) {
        storage.set("key" + std::to_string(i), "value");
    }

    // Should have capacity of at least 16 (1 per shard)
    assert(storage.size() >= 1);
    assert(storage.size() <= 16);
    std::cout << "PASSED (size=" << storage.size() << ")\n";
}

int main() {
    std::cout << "=== LRU Eviction Tests ===\n";

    testEvictionOrder();
    testGetPreventsEviction();
    testEvictedKeysCounter();
    testUpdateExistingKeyNoEviction();
    testDeleteRemovesFromLRU();
    testLRUWithTTL();
    testConcurrentLRU();
    testMinCapacity();

    std::cout << "\nAll LRU tests passed!\n";
    return 0;
}
