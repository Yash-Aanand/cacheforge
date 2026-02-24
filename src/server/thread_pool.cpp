#include "server/thread_pool.h"

namespace cacheforge {

ThreadPool::ThreadPool(size_t num_threads) {
    // Ensure at least 1 thread
    if (num_threads == 0) {
        num_threads = 1;
    }

    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this](std::stop_token stop_token) {
            workerLoop(stop_token);
        });
    }
}

ThreadPool::~ThreadPool() {
    // Request stop on all workers (jthread does this automatically, but we
    // need to wake them up)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // jthread destructor will request_stop(), but we notify to wake threads
    }
    cv_.notify_all();

    // jthread destructor automatically joins
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop(std::stop_token stop_token) {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            // Wait for task or stop signal
            cv_.wait(lock, stop_token, [this]() {
                return !tasks_.empty();
            });

            // Check if we should stop
            if (stop_token.stop_requested() && tasks_.empty()) {
                return;
            }

            // If woken but no tasks and not stopping, continue waiting
            if (tasks_.empty()) {
                continue;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        // Execute task outside the lock
        task();
    }
}

} // namespace cacheforge
