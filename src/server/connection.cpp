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
            has_error_ = true;
        }
        return commands;
    }

    if (bytes_read == 0) {
        // EOF - client disconnected
        has_error_ = true;
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
    write_buffer_ += std::move(response);
}

bool Connection::flushWriteBuffer() {
    if (write_buffer_.empty()) {
        return true;
    }

    ssize_t bytes_sent = send(fd_, write_buffer_.data(), write_buffer_.size(), MSG_NOSIGNAL);

    if (bytes_sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            has_error_ = true;
        }
        return false;
    }

    write_buffer_.erase(0, static_cast<size_t>(bytes_sent));
    return write_buffer_.empty();
}

} // namespace cacheforge
