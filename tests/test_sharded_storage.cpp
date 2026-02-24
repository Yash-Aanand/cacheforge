#include "storage/sharded_storage.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace cacheforge;

void test_set_get() {
    ShardedStorage storage;

    storage.set("key1", "value1");
    auto val = storage.get("key1");
    assert(val.has_value());
    assert(*val == "value1");

    // Overwrite
    storage.set("key1", "value2");
    val = storage.get("key1");
    assert(*val == "value2");
}

void test_get_nonexistent() {
    ShardedStorage storage;

    auto val = storage.get("nonexistent");
    assert(!val.has_value());
}

void test_del() {
    ShardedStorage storage;

    storage.set("key1", "value1");
    assert(storage.size() == 1);

    bool deleted = storage.del("key1");
    assert(deleted);
    assert(storage.size() == 0);
    assert(!storage.get("key1").has_value());

    // Delete nonexistent
    deleted = storage.del("nonexistent");
    assert(!deleted);
}

void test_size() {
    ShardedStorage storage;
    assert(storage.size() == 0);

    storage.set("a", "1");
    storage.set("b", "2");
    storage.set("c", "3");
    assert(storage.size() == 3);

    storage.del("b");
    assert(storage.size() == 2);
}

void test_sharding_distribution() {
    ShardedStorage storage;

    // Insert many keys and verify they all work correctly
    // This implicitly tests that different keys go to different shards
    for (int i = 0; i < 1000; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        storage.set(key, value);
    }

    assert(storage.size() == 1000);

    // Verify all values are correct
    for (int i = 0; i < 1000; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string expected = "value_" + std::to_string(i);
        auto val = storage.get(key);
        assert(val.has_value());
        assert(*val == expected);
    }
}

void test_concurrent_access() {
    ShardedStorage storage;
    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 1000;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                storage.set(key, "value");
                auto val = storage.get(key);
                assert(val.has_value());
                assert(*val == "value");
                storage.del(key);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // All keys should be deleted
    assert(storage.size() == 0);
}

void test_concurrent_same_shard() {
    ShardedStorage storage;
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 500;

    // Use keys that will likely hash to same shard for stress testing
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                // All threads compete for similar keys
                std::string key = "shared_key_" + std::to_string(i);
                storage.set(key, "thread_" + std::to_string(t));
                storage.get(key);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Should have ops_per_thread keys (last write wins for each)
    assert(storage.size() == static_cast<size_t>(ops_per_thread));
}

void test_high_concurrency_stress() {
    ShardedStorage storage;
    constexpr int num_threads = 16;
    constexpr int ops_per_thread = 500;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "stress_" + std::to_string(t) + "_" + std::to_string(i);
                storage.set(key, "value_" + std::to_string(i));

                // Read back immediately
                auto val = storage.get(key);
                assert(val.has_value());

                // Sometimes delete
                if (i % 3 == 0) {
                    storage.del(key);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
}

int main() {
    test_set_get();
    std::cout << "test_set_get passed\n";

    test_get_nonexistent();
    std::cout << "test_get_nonexistent passed\n";

    test_del();
    std::cout << "test_del passed\n";

    test_size();
    std::cout << "test_size passed\n";

    test_sharding_distribution();
    std::cout << "test_sharding_distribution passed\n";

    test_concurrent_access();
    std::cout << "test_concurrent_access passed\n";

    test_concurrent_same_shard();
    std::cout << "test_concurrent_same_shard passed\n";

    test_high_concurrency_stress();
    std::cout << "test_high_concurrency_stress passed\n";

    std::cout << "\nAll sharded storage tests passed!\n";
    return 0;
}
