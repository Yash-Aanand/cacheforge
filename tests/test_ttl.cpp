#include "storage/sharded_storage.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace cacheforge;

void testExpireExistingKey() {
    std::cout << "Test: EXPIRE on existing key returns 1... ";
    ShardedStorage storage;
    storage.set("mykey", "myvalue");
    assert(storage.expire("mykey", 60) == true);
    std::cout << "PASSED\n";
}

void testExpireNonExistentKey() {
    std::cout << "Test: EXPIRE on non-existent key returns 0... ";
    ShardedStorage storage;
    assert(storage.expire("nokey", 60) == false);
    std::cout << "PASSED\n";
}

void testTTLWithExpiration() {
    std::cout << "Test: TTL on key with TTL returns correct seconds... ";
    ShardedStorage storage;
    storage.set("mykey", "myvalue");
    storage.expire("mykey", 10);
    int64_t ttl = storage.ttl("mykey");
    // TTL should be between 9 and 10 (accounting for execution time)
    assert(ttl >= 9 && ttl <= 10);
    std::cout << "PASSED (ttl=" << ttl << ")\n";
}

void testTTLNoExpiration() {
    std::cout << "Test: TTL on key without TTL returns -1... ";
    ShardedStorage storage;
    storage.set("mykey", "myvalue");
    assert(storage.ttl("mykey") == -1);
    std::cout << "PASSED\n";
}

void testTTLNonExistentKey() {
    std::cout << "Test: TTL on non-existent key returns -2... ";
    ShardedStorage storage;
    assert(storage.ttl("nokey") == -2);
    std::cout << "PASSED\n";
}

void testKeyDisappearsAfterTTL() {
    std::cout << "Test: Key disappears after TTL expires (lazy expiration)... ";
    ShardedStorage storage;
    storage.set("expiring", "value");
    storage.expire("expiring", 1);

    // Key should exist immediately
    auto val = storage.get("expiring");
    assert(val.has_value());
    assert(*val == "value");

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // Key should be gone (lazy expiration on GET)
    val = storage.get("expiring");
    assert(!val.has_value());

    // TTL should return -2
    assert(storage.ttl("expiring") == -2);

    std::cout << "PASSED\n";
}

void testBackgroundSweep() {
    std::cout << "Test: Background sweep cleans up keys nobody reads... ";
    ShardedStorage storage;
    storage.startExpirationSweep();

    // Set keys with short TTL
    for (int i = 0; i < 50; ++i) {
        storage.setWithTTL("sweep_key_" + std::to_string(i), "value", 1);
    }

    // Verify keys exist
    assert(storage.size() == 50);

    // Wait for expiration + sweep (1s TTL + 500ms sweep interval + buffer)
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Keys should be swept (without us reading them)
    assert(storage.size() == 0);
    assert(storage.expiredKeysCount() == 50);

    storage.stopExpirationSweep();
    std::cout << "PASSED\n";
}

void testSetWithTTL() {
    std::cout << "Test: setWithTTL creates key with expiration... ";
    ShardedStorage storage;
    storage.setWithTTL("ttlkey", "value", 5);

    auto val = storage.get("ttlkey");
    assert(val.has_value());
    assert(*val == "value");

    int64_t remaining = storage.ttl("ttlkey");
    assert(remaining >= 4 && remaining <= 5);

    std::cout << "PASSED (ttl=" << remaining << ")\n";
}

void testSetResetsExpiration() {
    std::cout << "Test: SET resets TTL (removes expiration)... ";
    ShardedStorage storage;
    storage.setWithTTL("mykey", "value1", 5);

    // Verify TTL is set
    assert(storage.ttl("mykey") > 0);

    // Regular SET should clear TTL
    storage.set("mykey", "value2");

    // TTL should be -1 (no expiration)
    assert(storage.ttl("mykey") == -1);

    auto val = storage.get("mykey");
    assert(val.has_value());
    assert(*val == "value2");

    std::cout << "PASSED\n";
}

void testDelOnExpiredKey() {
    std::cout << "Test: DEL on expired key returns 0... ";
    ShardedStorage storage;
    storage.setWithTTL("expkey", "value", 1);

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // DEL should return false (key considered non-existent)
    assert(storage.del("expkey") == false);

    std::cout << "PASSED\n";
}

void testExpireOnExpiredKey() {
    std::cout << "Test: EXPIRE on already expired key returns 0... ";
    ShardedStorage storage;
    storage.setWithTTL("expkey", "value", 1);

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // EXPIRE should return false (key expired)
    assert(storage.expire("expkey", 60) == false);

    std::cout << "PASSED\n";
}

void testConcurrentTTLOperations() {
    std::cout << "Test: Concurrent TTL operations... ";
    ShardedStorage storage;
    storage.startExpirationSweep();

    std::vector<std::thread> threads;
    const int num_threads = 8;
    const int ops_per_thread = 100;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);

                // Set with TTL
                storage.setWithTTL(key, "value", 2);

                // Check TTL
                storage.ttl(key);

                // Get value
                storage.get(key);

                // Update expiration
                storage.expire(key, 3);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All operations should complete without crashes or deadlocks
    storage.stopExpirationSweep();
    std::cout << "PASSED\n";
}

void testExpiredKeysCounter() {
    std::cout << "Test: Expired keys counter increments correctly... ";
    ShardedStorage storage;

    // Set keys that will expire immediately
    storage.setWithTTL("exp1", "v", 0);
    storage.setWithTTL("exp2", "v", 0);
    storage.setWithTTL("exp3", "v", 0);

    // Small delay to ensure expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Access them to trigger lazy expiration
    storage.get("exp1");
    storage.get("exp2");
    storage.get("exp3");

    assert(storage.expiredKeysCount() == 3);

    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== TTL Tests ===\n\n";

    testExpireExistingKey();
    testExpireNonExistentKey();
    testTTLWithExpiration();
    testTTLNoExpiration();
    testTTLNonExistentKey();
    testKeyDisappearsAfterTTL();
    testSetWithTTL();
    testSetResetsExpiration();
    testDelOnExpiredKey();
    testExpireOnExpiredKey();
    testBackgroundSweep();
    testConcurrentTTLOperations();
    testExpiredKeysCounter();

    std::cout << "\nAll TTL tests passed!\n";
    return 0;
}
