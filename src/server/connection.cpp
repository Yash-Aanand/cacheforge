#include "server/connection.h"

#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

namespace cacheforge {

namespace {
    constexpr size_t READ_BUFFER_SIZE = 4096;
}

Connection::Connection(int fd) : fd_(fd) {}

std::vector<std::string> Connection::readAndParse() {
    std::vector<std::string> commands;

    char buffer[READ_BUFFER_SIZE];
    ssize_t bytes_read = recv(fd_, buffer, sizeof(buffer), 0);

    if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            has_error_.store(true, std::memory_order_release);
        }
        return commands;
    }

    if (bytes_read == 0) {
        // EOF - client disconnected
        has_error_.store(true, std::memory_order_release);
        return commands;
    }

    read_buffer_.append(buffer, static_cast<size_t>(bytes_read));

    // Extract complete commands (newline-terminated)
    size_t pos;
    while ((pos = read_buffer_.find('\n')) != std::string::npos) {
        std::string cmd = read_buffer_.substr(0, pos);
        read_buffer_.erase(0, pos + 1);

        // Strip trailing \r if present (for telnet compatibility)
        if (!cmd.empty() && cmd.back() == '\r') {
            cmd.pop_back();
        }

        if (!cmd.empty()) {
            commands.push_back(std::move(cmd));
        }
    }

    return commands;
}

void Connection::queueResponse(std::string response) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_buffer_ += std::move(response);
}

bool Connection::sendResponse(const std::string& response) {
    // Direct send from worker thread
    size_t total_sent = 0;
    while (total_sent < response.size()) {
        ssize_t bytes_sent = send(fd_, response.data() + total_sent,
                                   response.size() - total_sent, MSG_NOSIGNAL);

        if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket buffer full - queue remaining data for epoll to handle
                std::lock_guard<std::mutex> lock(write_mutex_);
                write_buffer_ += response.substr(total_sent);
                return false;
            }
            has_error_.store(true, std::memory_order_release);
            return false;
        }

        total_sent += static_cast<size_t>(bytes_sent);
    }

    return true;
}

bool Connection::flushWriteBuffer() {
    std::lock_guard<std::mutex> lock(write_mutex_);

    if (write_buffer_.empty()) {
        return true;
    }

    ssize_t bytes_sent = send(fd_, write_buffer_.data(), write_buffer_.size(), MSG_NOSIGNAL);

    if (bytes_sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            has_error_.store(true, std::memory_order_release);
        }
        return false;
    }

    write_buffer_.erase(0, static_cast<size_t>(bytes_sent));
    return write_buffer_.empty();
}

bool Connection::wantWrite() const {
    std::lock_guard<std::mutex> lock(write_mutex_);
    return !write_buffer_.empty();
}

bool Connection::trySetInFlight() {
    bool expected = false;
    return in_flight_.compare_exchange_strong(expected, true,
                                               std::memory_order_acq_rel);
}

void Connection::clearInFlight() {
    in_flight_.store(false, std::memory_order_release);
}

} // namespace cacheforge
