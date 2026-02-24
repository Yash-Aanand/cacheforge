#include "server/server.h"
#include "protocol/parser.h"
#include "protocol/response.h"

#include <iostream>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
    #include <ws2tcpip.h>
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    constexpr socket_t INVALID_SOCKET = -1;
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
#endif

namespace cacheforge {

namespace {
    constexpr size_t BUFFER_SIZE = 1024;

#ifdef _WIN32
    class WinsockInit {
    public:
        WinsockInit() {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw std::runtime_error("WSAStartup failed");
            }
        }
        ~WinsockInit() {
            WSACleanup();
        }
    };

    // Global Winsock initializer
    WinsockInit& getWinsockInit() {
        static WinsockInit init;
        return init;
    }
#endif
}

Server::Server(uint16_t port)
    : port_(port)
    , server_fd_(INVALID_SOCKET)
    , running_(false)
{
#ifdef _WIN32
    getWinsockInit();
#endif

    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create socket: " + std::to_string(SOCKET_ERROR_CODE));
    }

    // Set SO_REUSEADDR
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
#else
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
        CLOSE_SOCKET(server_fd_);
        throw std::runtime_error("Failed to set SO_REUSEADDR");
    }

    // Bind
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        CLOSE_SOCKET(server_fd_);
        throw std::runtime_error("Failed to bind to port " + std::to_string(port_));
    }

    // Listen
    if (listen(server_fd_, 1) < 0) {
        CLOSE_SOCKET(server_fd_);
        throw std::runtime_error("Failed to listen on socket");
    }
}

Server::~Server() {
    stop();
    if (server_fd_ != INVALID_SOCKET) {
        CLOSE_SOCKET(server_fd_);
    }
}

void Server::run() {
    running_ = true;
    std::cout << "Server listening on port " << port_ << "\n";

    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        socket_t client_fd = accept(server_fd_,
                                    reinterpret_cast<struct sockaddr*>(&client_addr),
                                    &client_len);

        if (client_fd == INVALID_SOCKET) {
            if (running_) {
                std::cerr << "Accept failed: " << SOCKET_ERROR_CODE << "\n";
            }
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Client connected from " << client_ip << "\n";

        handleClient(client_fd);

        std::cout << "Client disconnected\n";
    }
}

void Server::stop() {
    running_ = false;
}

void Server::handleClient(socket_t client_fd) {
    char buffer[BUFFER_SIZE];

    while (running_) {
        std::memset(buffer, 0, BUFFER_SIZE);

#ifdef _WIN32
        int bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
#else
        ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
#endif

        if (bytes_read <= 0) {
            // Client disconnected or error
            break;
        }

        // Parse and respond
        std::string_view input(buffer, static_cast<size_t>(bytes_read));
        Command cmd = parseCommand(input);
        std::string response = formatResponse(cmd);

#ifdef _WIN32
        send(client_fd, response.c_str(), static_cast<int>(response.size()), 0);
#else
        send(client_fd, response.c_str(), response.size(), 0);
#endif
    }

    CLOSE_SOCKET(client_fd);
}

} // namespace cacheforge
