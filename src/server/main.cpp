#include "server/server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

namespace {
    cacheforge::Server* g_server = nullptr;

    void signalHandler(int /*signal*/) {
        if (g_server) {
            g_server->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 6380;
    size_t num_threads = 0;  // 0 = auto (hardware_concurrency)

    // Simple argument parsing: [port] [threads]
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    if (argc > 2) {
        num_threads = static_cast<size_t>(std::atoi(argv[2]));
    }

    std::cout << "CacheForge server starting on port " << port << "...\n";

    try {
        cacheforge::Server server(port, num_threads);
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
