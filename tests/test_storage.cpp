#include "storage/storage.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace cacheforge;

void test_set_get() {
    Storage storage;

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
    Storage storage;

    auto val = storage.get("nonexistent");
    assert(!val.has_value());
}

void test_del() {
    Storage storage;

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
    Storage storage;
    assert(storage.size() == 0);

    storage.set("a", "1");
    storage.set("b", "2");
    storage.set("c", "3");
    assert(storage.size() == 3);

    storage.del("b");
    assert(storage.size() == 2);
}

void test_concurrent_access() {
    Storage storage;
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 1000;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                storage.set(key, "value");
                storage.get(key);
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

int main() {
    test_set_get();
    std::cout << "test_set_get passed\n";

    test_get_nonexistent();
    std::cout << "test_get_nonexistent passed\n";

    test_del();
    std::cout << "test_del passed\n";

    test_size();
    std::cout << "test_size passed\n";

    test_concurrent_access();
    std::cout << "test_concurrent_access passed\n";

    std::cout << "\nAll storage tests passed!\n";
    return 0;
}
