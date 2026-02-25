#include "server/server.h"
#include "server/connection.h"
#include "server/event_loop.h"
#include "server/thread_pool.h"
#include "protocol/parser.h"
#include "protocol/dispatcher.h"
#include "storage/sharded_storage.h"
#include "storage/aof_writer.h"
#include "storage/aof_replay.h"

#include <iostream>
#include <cstring>
#include <stdexcept>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace cacheforge {

namespace {
    void setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            throw std::runtime_error("fcntl F_GETFL failed");
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            throw std::runtime_error("fcntl F_SETFL failed");
        }
    }
}

Server::Server(uint16_t port, size_t num_threads, bool aof_enabled, const std::string& aof_path)
    : port_(port)
    , server_fd_(-1)
    , running_(false)
    , storage_(std::make_unique<ShardedStorage>())
    , aof_enabled_(aof_enabled)
    , aof_path_(aof_path)
{
    // Initialize AOF if enabled
    if (aof_enabled_) {
        aof_writer_ = std::make_unique<AOFWriter>(aof_path_);
        aof_writer_->setEnabled(false);  // Disable during replay

        AOFReplay replay(*storage_);
        auto stats = replay.replay(aof_path_);
        std::cout << "AOF: " << stats.commands_replayed << " commands replayed";
        if (stats.errors > 0) {
            std::cout << " (" << stats.errors << " errors)";
        }
        std::cout << "\n";

        aof_writer_->setEnabled(true);
        aof_writer_->start();
    }

    // Create dispatcher with optional AOF writer
    dispatcher_ = std::make_unique<Dispatcher>(*storage_, aof_writer_.get());
    event_loop_ = std::make_unique<EventLoop>();
    thread_pool_ = std::make_unique<ThreadPool>(num_threads == 0 ? std::thread::hardware_concurrency() : num_threads);
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // Set SO_REUSEADDR
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to set SO_REUSEADDR");
    }

    // Bind
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to bind to port " + std::to_string(port_));
    }

    // Listen with larger backlog for concurrent connections
    if (listen(server_fd_, 128) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to listen on socket");
    }

    // Set server socket to non-blocking
    setNonBlocking(server_fd_);

    // Add server socket to epoll
    event_loop_->addFd(server_fd_, EPOLLIN);

    // Start background expiration sweep
    storage_->startExpirationSweep();
}

Server::~Server() {
    stop();

    // Stop AOF writer to ensure all pending writes are flushed
    if (aof_writer_) {
        aof_writer_->stop();
    }

    // Clear connections before destroying thread pool to ensure all tasks complete
    // shared_ptr ensures connections survive until tasks finish
    connections_.clear();

    if (server_fd_ >= 0) {
        close(server_fd_);
    }
}

void Server::run() {
    running_ = true;
    std::cout << "Server listening on port " << port_
              << " with " << thread_pool_->size() << " worker threads\n";

    while (running_) {
        auto events = event_loop_->wait(100); // 100ms timeout for responsive shutdown

        for (const auto& [fd, ev] : events) {
            if (fd == server_fd_) {
                // New connection
                acceptConnection();
            } else {
                // Client event
                if (ev & (EPOLLERR | EPOLLHUP)) {
                    closeConnection(fd);
                    continue;
                }

                if (ev & EPOLLIN) {
                    handleRead(fd);
                }

                // Check if connection still exists (might have been closed in handleRead)
                auto it = connections_.find(fd);
                if (it == connections_.end()) {
                    continue;
                }

                if (ev & EPOLLOUT) {
                    handleWrite(fd);
                }
            }
        }
    }
}

void Server::stop() {
    running_ = false;
}

void Server::acceptConnection() {
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_,
                               reinterpret_cast<struct sockaddr*>(&client_addr),
                               &client_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more pending connections
                break;
            }
            std::cerr << "Accept failed: " << strerror(errno) << "\n";
            break;
        }

        // Set non-blocking
        setNonBlocking(client_fd);

        // Create connection and add to epoll
        connections_[client_fd] = std::make_shared<Connection>(client_fd);
        event_loop_->addFd(client_fd, EPOLLIN);

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Client connected from " << client_ip << " (fd=" << client_fd << ")\n";
    }
}

void Server::handleRead(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }

    auto conn = it->second;  // shared_ptr copy

    // Skip if a task is already in-flight for this connection
    if (conn->isInFlight()) {
        return;
    }

    auto commands = conn->readAndParse();

    if (conn->hasError()) {
        closeConnection(fd);
        return;
    }

    // Process each complete command
    for (const auto& cmd_str : commands) {
        // Try to set in-flight flag
        if (!conn->trySetInFlight()) {
            // Task already in-flight, this shouldn't happen for first command
            // but could happen for subsequent commands in same read
            // Queue the command for later by putting it back... but we can't
            // easily do that. Instead, process synchronously.
            Command cmd = parseCommand(cmd_str);
            std::string response = dispatcher_->dispatch(cmd);
            conn->queueResponse(std::move(response));
            continue;
        }

        // Capture shared_ptr and command for the worker task
        Command cmd = parseCommand(cmd_str);
        Dispatcher* dispatcher = dispatcher_.get();

        thread_pool_->submit([conn, cmd, dispatcher]() {
            std::string response = dispatcher->dispatch(cmd);
            conn->sendResponse(response);
            conn->clearInFlight();
        });

        // Only one task at a time per connection
        // If there are more commands, they'll be processed on next read
        break;
    }

    // Update epoll events if we have data to write
    updateEpollEvents(fd);
}

void Server::handleWrite(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }

    auto& conn = it->second;
    conn->flushWriteBuffer();

    if (conn->hasError()) {
        closeConnection(fd);
        return;
    }

    // Update epoll events
    updateEpollEvents(fd);
}

void Server::closeConnection(int fd) {
    std::cout << "Client disconnected (fd=" << fd << ")\n";

    event_loop_->removeFd(fd);
    connections_.erase(fd);  // shared_ptr may still be held by in-flight task
    close(fd);
}

void Server::updateEpollEvents(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }

    uint32_t events = EPOLLIN;
    if (it->second->wantWrite()) {
        events |= EPOLLOUT;
    }

    event_loop_->modifyFd(fd, events);
}

} // namespace cacheforge
