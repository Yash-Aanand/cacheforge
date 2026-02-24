#include <iostream>
#include <cstring>
#include <cstdlib>

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

constexpr size_t BUFFER_SIZE = 1024;

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    uint16_t port = 6380;

    // Simple argument parsing
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

    // Create socket
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Connect
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);

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

    // Send PING
    const char* ping_cmd = "PING\n";
#ifdef _WIN32
    send(sock, ping_cmd, static_cast<int>(strlen(ping_cmd)), 0);
#else
    send(sock, ping_cmd, strlen(ping_cmd), 0);
#endif

    std::cout << "Sent: PING\n";

    // Receive response
    char buffer[BUFFER_SIZE];
    std::memset(buffer, 0, BUFFER_SIZE);

#ifdef _WIN32
    int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
#else
    ssize_t bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
#endif

    if (bytes_read > 0) {
        // Trim newline for display
        std::string response(buffer, static_cast<size_t>(bytes_read));
        while (!response.empty() && (response.back() == '\n' || response.back() == '\r')) {
            response.pop_back();
        }
        std::cout << "Received: " << response << "\n";
    } else {
        std::cerr << "Failed to receive response\n";
    }

    // Cleanup
    CLOSE_SOCKET(sock);
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
