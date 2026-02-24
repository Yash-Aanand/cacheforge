#ifndef CACHEFORGE_THREAD_POOL_H
#define CACHEFORGE_THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace cacheforge {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Disable copy
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a task to the pool
    void submit(std::function<void()> task);

    // Get number of worker threads
    size_t size() const { return workers_.size(); }

private:
    void workerLoop(std::stop_token stop_token);

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
};

} // namespace cacheforge

#endif // CACHEFORGE_THREAD_POOL_H
