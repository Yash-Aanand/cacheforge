#ifndef CACHEFORGE_CONNECTION_H
#define CACHEFORGE_CONNECTION_H

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace cacheforge {

class Connection {
public:
    explicit Connection(int fd);

    int fd() const { return fd_; }

    // Read from socket and return complete commands (newline-terminated)
    // Leaves partial data in buffer for next read
    // NOT thread-safe - must only be called from epoll loop
    std::vector<std::string> readAndParse();

    // Queue response for sending (thread-safe)
    void queueResponse(std::string response);

    // Send response directly via send() (thread-safe)
    // Returns true if all data was sent
    bool sendResponse(const std::string& response);

    // Attempt to flush write buffer. Returns true if all data sent.
    // NOT thread-safe - must only be called from epoll loop
    bool flushWriteBuffer();

    // Returns true if there's pending data to write (thread-safe)
    bool wantWrite() const;

    // Returns true if connection encountered an error or EOF (thread-safe)
    bool hasError() const { return has_error_.load(std::memory_order_acquire); }

    // In-flight task management
    bool isInFlight() const { return in_flight_.load(std::memory_order_acquire); }
    bool trySetInFlight();   // Returns true if successfully set (was false)
    void clearInFlight();

private:
    const int fd_;
    std::string read_buffer_;

    // Protected by write_mutex_ for thread-safe access
    mutable std::mutex write_mutex_;
    std::string write_buffer_;

    std::atomic<bool> has_error_{false};
    std::atomic<bool> in_flight_{false};
};

} // namespace cacheforge

#endif // CACHEFORGE_CONNECTION_H
