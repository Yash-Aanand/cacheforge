#include "protocol/dispatcher.h"
#include "protocol/parser.h"
#include "storage/sharded_storage.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

using namespace cacheforge;

// Parse STATS response: strip "$" prefix and trailing "\n", split on "," then ":"
std::unordered_map<std::string, std::string> parseStatsResponse(const std::string& response) {
    std::unordered_map<std::string, std::string> result;

    // Response format: "$value\n" — strip leading '$' and trailing '\n'
    std::string body = response;
    if (!body.empty() && body.front() == '$') {
        body = body.substr(1);
    }
    if (!body.empty() && body.back() == '\n') {
        body.pop_back();
    }

    // Split on ','
    std::istringstream stream(body);
    std::string pair;
    while (std::getline(stream, pair, ',')) {
        auto colon = pair.find(':');
        if (colon != std::string::npos) {
            result[pair.substr(0, colon)] = pair.substr(colon + 1);
        }
    }

    return result;
}

void test_stats_initial() {
    ShardedStorage storage;
    Dispatcher dispatcher(storage);

    Command cmd;
    cmd.type = CommandType::STATS;
    std::string response = dispatcher.dispatch(cmd);

    auto stats = parseStatsResponse(response);

    // STATS itself is request #1
    assert(stats["total_requests"] == "1");
    assert(stats["total_reads"] == "0");
    assert(stats["total_writes"] == "0");
    assert(stats["cache_hits"] == "0");
    assert(stats["cache_misses"] == "0");
    assert(stats["expired_keys"] == "0");
    assert(stats["evicted_keys"] == "0");
    assert(stats["current_keys"] == "0");
    assert(std::stoi(stats["uptime_seconds"]) >= 0);
}

void test_stats_after_operations() {
    ShardedStorage storage;
    Dispatcher dispatcher(storage);

    // 3x SET
    dispatcher.dispatch(parseCommand("SET key1 val1"));
    dispatcher.dispatch(parseCommand("SET key2 val2"));
    dispatcher.dispatch(parseCommand("SET key3 val3"));

    // 2x GET (hits)
    dispatcher.dispatch(parseCommand("GET key1"));
    dispatcher.dispatch(parseCommand("GET key2"));

    // 1x GET (miss)
    dispatcher.dispatch(parseCommand("GET nonexistent"));

    // 1x DEL
    dispatcher.dispatch(parseCommand("DEL key3"));

    // STATS (request #8)
    std::string response = dispatcher.dispatch(parseCommand("STATS"));
    auto stats = parseStatsResponse(response);

    assert(stats["total_requests"] == "8");
    assert(stats["total_reads"] == "3");
    assert(stats["total_writes"] == "4");
    assert(stats["cache_hits"] == "2");
    assert(stats["cache_misses"] == "1");
    assert(stats["current_keys"] == "2");
}

void test_stats_expired_keys() {
    ShardedStorage storage;
    storage.startExpirationSweep();
    Dispatcher dispatcher(storage);

    // SET with TTL of 1 second
    dispatcher.dispatch(parseCommand("SET tempkey tempval"));
    dispatcher.dispatch(parseCommand("EXPIRE tempkey 1"));

    // Wait for expiry + sweep
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    // GET should miss (expired)
    dispatcher.dispatch(parseCommand("GET tempkey"));

    std::string response = dispatcher.dispatch(parseCommand("STATS"));
    auto stats = parseStatsResponse(response);

    assert(std::stoul(stats["expired_keys"]) >= 1);
    assert(stats["cache_misses"] == "1");

    storage.stopExpirationSweep();
}

void test_stats_evicted_keys() {
    // Small capacity: 16 keys total (1 per shard)
    ShardedStorage storage(16);
    Dispatcher dispatcher(storage);

    // Overfill: insert enough keys to cause evictions
    for (int i = 0; i < 32; i++) {
        std::string key = "key" + std::to_string(i);
        std::string val = "val" + std::to_string(i);
        Command cmd;
        cmd.type = CommandType::SET;
        cmd.args = {key, val};
        dispatcher.dispatch(cmd);
    }

    std::string response = dispatcher.dispatch(parseCommand("STATS"));
    auto stats = parseStatsResponse(response);

    assert(std::stoul(stats["evicted_keys"]) > 0);
}

void test_stats_current_keys() {
    ShardedStorage storage;
    Dispatcher dispatcher(storage);

    // SET 5 keys
    dispatcher.dispatch(parseCommand("SET a 1"));
    dispatcher.dispatch(parseCommand("SET b 2"));
    dispatcher.dispatch(parseCommand("SET c 3"));
    dispatcher.dispatch(parseCommand("SET d 4"));
    dispatcher.dispatch(parseCommand("SET e 5"));

    // DEL 2 keys
    dispatcher.dispatch(parseCommand("DEL a"));
    dispatcher.dispatch(parseCommand("DEL b"));

    std::string response = dispatcher.dispatch(parseCommand("STATS"));
    auto stats = parseStatsResponse(response);

    assert(stats["current_keys"] == "3");
}

int main() {
    test_stats_initial();
    std::cout << "test_stats_initial passed\n";

    test_stats_after_operations();
    std::cout << "test_stats_after_operations passed\n";

    test_stats_expired_keys();
    std::cout << "test_stats_expired_keys passed\n";

    test_stats_evicted_keys();
    std::cout << "test_stats_evicted_keys passed\n";

    test_stats_current_keys();
    std::cout << "test_stats_current_keys passed\n";

    std::cout << "\nAll stats tests passed!\n";
    return 0;
}
