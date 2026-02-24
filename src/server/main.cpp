#include "server/server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

namespace {
    cacheforge::Server* g_server = nullptr;

    void signalHandler(int signal) {
        if (g_server) {
            std::cout << "\nReceived signal " << signal << ", shutting down...\n";
            g_server->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 6380;

    // Simple argument parsing for port
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    std::cout << "CacheForge server starting on port " << port << "...\n";

    try {
        cacheforge::Server server(port);
        g_server = &server;

        // Set up signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Server stopped.\n";
    return 0;
}
