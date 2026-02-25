#ifndef CACHEFORGE_AOF_WRITER_H
#define CACHEFORGE_AOF_WRITER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace cacheforge {

class AOFWriter {
public:
    explicit AOFWriter(const std::string& path,
                       std::chrono::milliseconds fsync_interval = std::chrono::milliseconds{100});
    ~AOFWriter();

    // Disable copy
    AOFWriter(const AOFWriter&) = delete;
    AOFWriter& operator=(const AOFWriter&) = delete;

    // Type-safe logging (handles quoting automatically)
    void logSet(const std::string& key, const std::string& value);
    void logDel(const std::string& key);
    void logExpire(const std::string& key, int64_t seconds);

    void start();                           // Start background writer thread
    void stop();                            // Stop and flush pending writes
    void setEnabled(bool enabled);          // Disable during replay
    bool isEnabled() const;
    size_t pendingCount() const;
    size_t writtenCount() const;

private:
    void writerLoop(std::stop_token stop_token);
    void enqueue(std::string command);
    static std::string quoteIfNeeded(const std::string& s);

    std::string path_;
    std::chrono::milliseconds fsync_interval_;
    std::queue<std::string> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::jthread writer_thread_;
    std::atomic<bool> enabled_{true};
    std::atomic<bool> stopped_{false};
    std::atomic<size_t> written_count_{0};
    std::unique_ptr<std::ofstream> file_;
    std::chrono::steady_clock::time_point last_fsync_;
};

} // namespace cacheforge

#endif // CACHEFORGE_AOF_WRITER_H
