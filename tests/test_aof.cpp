#include "storage/aof_writer.h"
#include "storage/aof_replay.h"
#include "storage/sharded_storage.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using namespace cacheforge;

namespace fs = std::filesystem;

// Helper to create a unique temp file path
std::string tempAofPath() {
    static int counter = 0;
    return "./test_aof_" + std::to_string(++counter) + "_" + std::to_string(std::time(nullptr)) + ".aof";
}

// Helper to clean up test files
void cleanup(const std::string& path) {
    std::remove(path.c_str());
}

void test_write_and_replay_100_keys() {
    std::cout << "Test: Write and replay 100 keys... ";
    std::string aof_path = tempAofPath();

    // Write 100 keys
    {
        AOFWriter writer(aof_path);
        writer.start();

        for (int i = 0; i < 100; ++i) {
            writer.logSet("key" + std::to_string(i), "value" + std::to_string(i));
        }

        // Give time for async writes
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        writer.stop();
    }

    // Replay into new storage
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == 100);
    assert(stats.errors == 0);
    assert(storage.size() == 100);

    // Verify all keys present
    for (int i = 0; i < 100; ++i) {
        auto val = storage.get("key" + std::to_string(i));
        assert(val.has_value());
        assert(*val == "value" + std::to_string(i));
    }

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_del_command_replayed() {
    std::cout << "Test: DEL command replayed correctly... ";
    std::string aof_path = tempAofPath();

    // Write SET then DEL
    {
        AOFWriter writer(aof_path);
        writer.start();

        writer.logSet("mykey", "myvalue");
        writer.logDel("mykey");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        writer.stop();
    }

    // Replay
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == 2);
    assert(!storage.get("mykey").has_value());

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_expire_command_replayed() {
    std::cout << "Test: EXPIRE command replayed correctly... ";
    std::string aof_path = tempAofPath();

    // Write SET then EXPIRE
    {
        AOFWriter writer(aof_path);
        writer.start();

        writer.logSet("mykey", "myvalue");
        writer.logExpire("mykey", 60);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        writer.stop();
    }

    // Replay
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == 2);
    auto val = storage.get("mykey");
    assert(val.has_value());
    assert(*val == "myvalue");

    // TTL should be set (close to 60)
    int64_t ttl = storage.ttl("mykey");
    assert(ttl >= 58 && ttl <= 60);

    cleanup(aof_path);
    std::cout << "PASSED (ttl=" << ttl << ")\n";
}

void test_corrupted_line_recovery() {
    std::cout << "Test: Recovery from corrupted lines... ";
    std::string aof_path = tempAofPath();

    // Write AOF file manually with some invalid lines
    {
        std::ofstream file(aof_path);
        file << "SET key1 value1\n";
        file << "INVALID_COMMAND\n";
        file << "SET key2 value2\n";
        file << "SET_MISSING_VALUE\n";
        file << "SET key3 value3\n";
    }

    // Replay
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    // Valid SET commands should be replayed
    assert(stats.commands_replayed == 3);
    assert(stats.errors == 0);  // UNKNOWN command is skipped, not error
    assert(stats.lines_skipped >= 1);

    assert(storage.get("key1").value_or("") == "value1");
    assert(storage.get("key2").value_or("") == "value2");
    assert(storage.get("key3").value_or("") == "value3");

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_concurrent_writes() {
    std::cout << "Test: Concurrent writes from multiple threads... ";
    std::string aof_path = tempAofPath();

    const int num_threads = 4;
    const int writes_per_thread = 50;

    {
        AOFWriter writer(aof_path);
        writer.start();

        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&writer, t, writes_per_thread]() {
                for (int i = 0; i < writes_per_thread; ++i) {
                    writer.logSet("key_" + std::to_string(t) + "_" + std::to_string(i),
                                  "value_" + std::to_string(t) + "_" + std::to_string(i));
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        writer.stop();
    }

    // Verify written count
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == num_threads * writes_per_thread);
    assert(storage.size() == num_threads * writes_per_thread);

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_replay_mode_disables_logging() {
    std::cout << "Test: setEnabled(false) prevents logging... ";
    std::string aof_path = tempAofPath();

    {
        AOFWriter writer(aof_path);
        writer.start();

        writer.logSet("key1", "value1");
        writer.setEnabled(false);
        writer.logSet("key2", "value2");  // Should not be logged
        writer.setEnabled(true);
        writer.logSet("key3", "value3");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        writer.stop();
    }

    // Replay and verify only key1 and key3
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == 2);
    assert(storage.get("key1").has_value());
    assert(!storage.get("key2").has_value());
    assert(storage.get("key3").has_value());

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_values_with_spaces() {
    std::cout << "Test: Values with spaces are quoted correctly... ";
    std::string aof_path = tempAofPath();

    {
        AOFWriter writer(aof_path);
        writer.start();

        writer.logSet("greeting", "hello world");
        writer.logSet("sentence", "the quick brown fox");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        writer.stop();
    }

    // Replay
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == 2);
    assert(storage.get("greeting").value_or("") == "hello world");
    assert(storage.get("sentence").value_or("") == "the quick brown fox");

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_values_with_quotes() {
    std::cout << "Test: Values with quotes are escaped correctly... ";
    std::string aof_path = tempAofPath();

    {
        AOFWriter writer(aof_path);
        writer.start();

        writer.logSet("quote_test", "say \"hi\"");
        writer.logSet("backslash_test", "path\\to\\file");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        writer.stop();
    }

    // Replay
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == 2);
    assert(storage.get("quote_test").value_or("") == "say \"hi\"");
    assert(storage.get("backslash_test").value_or("") == "path\\to\\file");

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_pending_and_written_counts() {
    std::cout << "Test: pendingCount and writtenCount metrics... ";
    std::string aof_path = tempAofPath();

    {
        AOFWriter writer(aof_path);

        // Before start, nothing written
        assert(writer.writtenCount() == 0);

        writer.start();

        // Write some commands
        for (int i = 0; i < 10; ++i) {
            writer.logSet("key" + std::to_string(i), "value");
        }

        // Wait for background thread to process
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        assert(writer.writtenCount() == 10);
        assert(writer.pendingCount() == 0);

        writer.stop();
    }

    cleanup(aof_path);
    std::cout << "PASSED\n";
}

void test_empty_aof_file() {
    std::cout << "Test: Empty/missing AOF file is handled gracefully... ";
    std::string aof_path = tempAofPath();

    // No file exists
    ShardedStorage storage;
    AOFReplay replay(storage);
    auto stats = replay.replay(aof_path);

    assert(stats.commands_replayed == 0);
    assert(stats.errors == 0);
    assert(storage.size() == 0);

    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== AOF Tests ===\n\n";

    test_write_and_replay_100_keys();
    test_del_command_replayed();
    test_expire_command_replayed();
    test_corrupted_line_recovery();
    test_concurrent_writes();
    test_replay_mode_disables_logging();
    test_values_with_spaces();
    test_values_with_quotes();
    test_pending_and_written_counts();
    test_empty_aof_file();

    std::cout << "\nAll AOF tests passed!\n";
    return 0;
}
