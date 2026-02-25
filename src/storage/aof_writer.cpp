#include "storage/aof_writer.h"

#include <iostream>
#include <vector>

namespace cacheforge {

AOFWriter::AOFWriter(const std::string& path, std::chrono::milliseconds fsync_interval)
    : path_(path)
    , fsync_interval_(fsync_interval)
    , last_fsync_(std::chrono::steady_clock::now())
{
}

AOFWriter::~AOFWriter() {
    stop();
}

void AOFWriter::start() {
    // Open file in append mode
    file_ = std::make_unique<std::ofstream>(path_, std::ios::app | std::ios::binary);
    if (!file_->is_open()) {
        throw std::runtime_error("Failed to open AOF file: " + path_);
    }

    // Start background writer thread
    writer_thread_ = std::jthread([this](std::stop_token stop_token) {
        writerLoop(stop_token);
    });
}

void AOFWriter::stop() {
    stopped_.store(true, std::memory_order_release);

    if (writer_thread_.joinable()) {
        writer_thread_.request_stop();
        cv_.notify_all();
        writer_thread_.join();
    }

    // Flush any remaining data
    if (file_ && file_->is_open()) {
        file_->flush();
        file_->close();
    }
}

void AOFWriter::setEnabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_release);
}

bool AOFWriter::isEnabled() const {
    return enabled_.load(std::memory_order_acquire);
}

size_t AOFWriter::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

size_t AOFWriter::writtenCount() const {
    return written_count_.load(std::memory_order_relaxed);
}

void AOFWriter::logSet(const std::string& key, const std::string& value) {
    if (!enabled_.load(std::memory_order_acquire)) return;
    std::string cmd = "SET " + quoteIfNeeded(key) + " " + quoteIfNeeded(value);
    enqueue(std::move(cmd));
}

void AOFWriter::logDel(const std::string& key) {
    if (!enabled_.load(std::memory_order_acquire)) return;
    enqueue("DEL " + quoteIfNeeded(key));
}

void AOFWriter::logExpire(const std::string& key, int64_t seconds) {
    if (!enabled_.load(std::memory_order_acquire)) return;
    enqueue("EXPIRE " + quoteIfNeeded(key) + " " + std::to_string(seconds));
}

void AOFWriter::enqueue(std::string command) {
    if (stopped_.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(command));
    }
    cv_.notify_one();
}

std::string AOFWriter::quoteIfNeeded(const std::string& s) {
    if (s.find_first_of(" \t\"\\") == std::string::npos) {
        return s;  // No quoting needed
    }
    std::string result = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') result += '\\';
        result += c;
    }
    result += '"';
    return result;
}

void AOFWriter::writerLoop(std::stop_token stop_token) {
    while (true) {
        std::vector<std::string> batch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // GCC 11 compatible: manual stop_token check in predicate
            cv_.wait_for(lock, fsync_interval_, [&] {
                return !queue_.empty() || stop_token.stop_requested();
            });

            if (stop_token.stop_requested() && queue_.empty()) return;

            batch.reserve(queue_.size());
            while (!queue_.empty()) {
                batch.push_back(std::move(queue_.front()));
                queue_.pop();
            }
        }

        try {
            if (!file_ || !file_->is_open()) {
                std::cerr << "AOF writer: file not open, dropping " << batch.size() << " commands\n";
                continue;
            }

            for (const auto& cmd : batch) {
                *file_ << cmd << '\n';
                if (!file_->good()) {
                    std::cerr << "AOF writer: write error, stream in bad state\n";
                    break;
                }
                written_count_.fetch_add(1, std::memory_order_relaxed);
            }

            // Flush to OS buffer if interval elapsed
            auto now = std::chrono::steady_clock::now();
            if (!batch.empty() || now - last_fsync_ >= fsync_interval_) {
                file_->flush();
                if (!file_->good()) {
                    std::cerr << "AOF writer: flush error, stream in bad state\n";
                }
                last_fsync_ = now;
            }
        } catch (const std::exception& e) {
            std::cerr << "AOF writer: exception in write loop: " << e.what() << "\n";
        }
    }
}

} // namespace cacheforge
