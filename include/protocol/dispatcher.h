#ifndef CACHEFORGE_DISPATCHER_H
#define CACHEFORGE_DISPATCHER_H

#include "protocol/parser.h"
#include <atomic>
#include <chrono>
#include <string>

namespace cacheforge {

class ShardedStorage;
class AOFWriter;

class Dispatcher {
public:
    explicit Dispatcher(ShardedStorage& storage, AOFWriter* aof_writer = nullptr);

    std::string dispatch(const Command& cmd);

private:
    ShardedStorage& storage_;
    AOFWriter* aof_writer_;

    std::atomic<size_t> total_requests_{0};
    std::atomic<size_t> total_reads_{0};
    std::atomic<size_t> total_writes_{0};
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_misses_{0};
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
};

} // namespace cacheforge

#endif // CACHEFORGE_DISPATCHER_H
