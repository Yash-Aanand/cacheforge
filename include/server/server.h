#ifndef CACHEFORGE_SERVER_H
#define CACHEFORGE_SERVER_H

#include <cstdint>
#include <string>

namespace cacheforge {

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
    void handleClient(int client_fd);

    uint16_t port_;
    int server_fd_;
    bool running_;
};

} // namespace cacheforge

#endif // CACHEFORGE_SERVER_H
