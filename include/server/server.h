#ifndef CACHEFORGE_SERVER_H
#define CACHEFORGE_SERVER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace cacheforge {

class ShardedStorage;
class Dispatcher;
class EventLoop;
class Connection;
class ThreadPool;
class AOFWriter;

class Server {
public:
    explicit Server(uint16_t port = 6380, size_t num_threads = 0,
                    bool aof_enabled = true, const std::string& aof_path = "./cache.aof");
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
    std::unique_ptr<ShardedStorage> storage_;
    std::unique_ptr<AOFWriter> aof_writer_;
    std::unique_ptr<Dispatcher> dispatcher_;
    std::unique_ptr<EventLoop> event_loop_;
    std::unique_ptr<ThreadPool> thread_pool_;

    bool aof_enabled_;
    std::string aof_path_;

    // Use shared_ptr for connections to allow safe capture in worker tasks
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
};

} // namespace cacheforge

#endif // CACHEFORGE_SERVER_H
