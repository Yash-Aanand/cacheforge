#include "server/server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    cacheforge::Server* g_server = nullptr;

    void signalHandler(int /*signal*/) {
        if (g_server) {
            g_server->stop();
        }
    }

    void printUsage(const char* program) {
        std::cout << "Usage: " << program << " [options]\n"
                  << "Options:\n"
                  << "  -p, --port <port>       Port to listen on (default: 6380)\n"
                  << "  -t, --threads <num>     Number of worker threads (default: auto)\n"
                  << "  --aof-enabled <bool>    Enable AOF persistence (default: true)\n"
                  << "  --aof-path <path>       Path to AOF file (default: ./cache.aof)\n"
                  << "  -h, --help              Show this help message\n";
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 6380;
    size_t num_threads = 0;  // 0 = auto (hardware_concurrency)
    bool aof_enabled = true;
    std::string aof_path = "./cache.aof";

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-p") == 0 || std::strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = static_cast<uint16_t>(std::atoi(argv[++i]));
            }
        } else if (std::strcmp(argv[i], "-t") == 0 || std::strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                num_threads = static_cast<size_t>(std::atoi(argv[++i]));
            }
        } else if (std::strcmp(argv[i], "--aof-enabled") == 0) {
            if (i + 1 < argc) {
                ++i;
                aof_enabled = (std::strcmp(argv[i], "true") == 0 || std::strcmp(argv[i], "1") == 0);
            }
        } else if (std::strcmp(argv[i], "--aof-path") == 0) {
            if (i + 1 < argc) {
                aof_path = argv[++i];
            }
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            // Legacy positional argument support: [port] [threads]
            port = static_cast<uint16_t>(std::atoi(argv[i]));
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                num_threads = static_cast<size_t>(std::atoi(argv[++i]));
            }
        }
    }

    std::cout << "CacheForge server starting on port " << port;
    if (aof_enabled) {
        std::cout << " (AOF: " << aof_path << ")";
    }
    std::cout << "...\n";

    try {
        cacheforge::Server server(port, num_threads, aof_enabled, aof_path);
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
