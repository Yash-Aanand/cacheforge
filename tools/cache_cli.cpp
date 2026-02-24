#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    #define INVALID_SOCKET -1
    #define CLOSE_SOCKET close
#endif

constexpr size_t BUFFER_SIZE = 4096;

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    uint16_t port = 6380;

    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr),
                sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << "\n";
        CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";
    std::cout << "Type commands (PING, SET key value, GET key, DEL key). Ctrl+C to exit.\n";

    std::string line;
    char buffer[BUFFER_SIZE];

    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        line += "\n";
#ifdef _WIN32
        send(sock, line.c_str(), static_cast<int>(line.size()), 0);
#else
        send(sock, line.c_str(), line.size(), 0);
#endif

        std::memset(buffer, 0, BUFFER_SIZE);
#ifdef _WIN32
        int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
#else
        ssize_t bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
#endif

        if (bytes_read > 0) {
            std::string response(buffer, static_cast<size_t>(bytes_read));
            while (!response.empty() && (response.back() == '\n' || response.back() == '\r')) {
                response.pop_back();
            }
            std::cout << response << "\n";
        } else {
            std::cerr << "Connection closed\n";
            break;
        }
    }

    CLOSE_SOCKET(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
