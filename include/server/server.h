#ifndef CACHEFORGE_SERVER_H
#define CACHEFORGE_SERVER_H

#include <cstdint>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    using socket_t = SOCKET;
#else
    using socket_t = int;
#endif

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
    void handleClient(socket_t client_fd);

    uint16_t port_;
    socket_t server_fd_;
    bool running_;
};

} // namespace cacheforge

#endif // CACHEFORGE_SERVER_H
