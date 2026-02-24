#ifndef CACHEFORGE_SERVER_H
#define CACHEFORGE_SERVER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace cacheforge {

class Storage;
class Dispatcher;
class EventLoop;
class Connection;

class Server {
public:
    explicit Server(uint16_t port = 6380);
    ~Server();

    // Disable copy
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Start the server (blocking)
    void run();

    // Stop the server
    void stop();

private:
    void acceptConnection();
    void handleRead(int fd);
    void handleWrite(int fd);
    void closeConnection(int fd);
    void updateEpollEvents(int fd);

    uint16_t port_;
    int server_fd_;
    std::atomic<bool> running_;
    std::unique_ptr<Storage> storage_;
    std::unique_ptr<Dispatcher> dispatcher_;
    std::unique_ptr<EventLoop> event_loop_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
};

} // namespace cacheforge

#endif // CACHEFORGE_SERVER_H
