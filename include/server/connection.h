#ifndef CACHEFORGE_CONNECTION_H
#define CACHEFORGE_CONNECTION_H

#include <string>
#include <vector>

namespace cacheforge {

class Connection {
public:
    explicit Connection(int fd);

    int fd() const { return fd_; }

    // Read from socket and return complete commands (newline-terminated)
    // Leaves partial data in buffer for next read
    std::vector<std::string> readAndParse();

    // Queue response for sending
    void queueResponse(std::string response);

    // Attempt to flush write buffer. Returns true if all data sent.
    bool flushWriteBuffer();

    // Returns true if there's pending data to write
    bool wantWrite() const { return !write_buffer_.empty(); }

    // Returns true if connection encountered an error or EOF
    bool hasError() const { return has_error_; }

private:
    int fd_;
    std::string read_buffer_;
    std::string write_buffer_;
    bool has_error_ = false;
};

} // namespace cacheforge

#endif // CACHEFORGE_CONNECTION_H
