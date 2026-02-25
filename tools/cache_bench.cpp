#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

struct BenchConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 6380;
    int threads = 4;
    int requests = 100000;
    int keyspace = 10000;
    double read_ratio = 0.8;
    int value_size = 64;
};

struct ThreadResult {
    std::vector<double> latencies;
    size_t errors = 0;
};

static std::string generateRandomValue(size_t len, std::mt19937& rng) {
    static constexpr char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        result += chars[dist(rng)];
    }
    return result;
}

static int connectToServer(const std::string& host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    // Disable Nagle's algorithm for lower latency
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        close(fd);
        return -1;
    }

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static bool sendCommand(int fd, const std::string& cmd) {
    std::string buf = cmd + "\n";
    size_t total_sent = 0;
    while (total_sent < buf.size()) {
        ssize_t sent = send(fd, buf.data() + total_sent, buf.size() - total_sent, 0);
        if (sent <= 0) return false;
        total_sent += static_cast<size_t>(sent);
    }
    return true;
}

static bool recvResponse(int fd, char* buffer, size_t bufsize) {
    // Read until we see a newline
    size_t total = 0;
    while (total < bufsize - 1) {
        ssize_t n = recv(fd, buffer + total, 1, 0);
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
        if (buffer[total - 1] == '\n') {
            buffer[total] = '\0';
            return true;
        }
    }
    buffer[total] = '\0';
    return true;
}

static void workerThread(const BenchConfig& config, int thread_id,
                          int num_requests, ThreadResult& result) {
    int fd = connectToServer(config.host, config.port);
    if (fd < 0) {
        result.errors = static_cast<size_t>(num_requests);
        return;
    }

    result.latencies.reserve(static_cast<size_t>(num_requests));

    auto seed = static_cast<unsigned>(thread_id * 1000 +
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> ratio_dist(0.0, 1.0);
    std::uniform_int_distribution<int> key_dist(0, config.keyspace - 1);

    // Pre-generate one random value per thread
    std::string value = generateRandomValue(static_cast<size_t>(config.value_size), rng);

    char recv_buf[4096];
    std::string cmd;
    cmd.reserve(128);

    for (int i = 0; i < num_requests; ++i) {
        int key_id = key_dist(rng);

        cmd.clear();
        if (ratio_dist(rng) < config.read_ratio) {
            cmd += "GET key:";
            cmd += std::to_string(key_id);
        } else {
            cmd += "SET key:";
            cmd += std::to_string(key_id);
            cmd += ' ';
            cmd += value;
        }

        auto start = std::chrono::high_resolution_clock::now();
        bool ok = sendCommand(fd, cmd) && recvResponse(fd, recv_buf, sizeof(recv_buf));
        auto end = std::chrono::high_resolution_clock::now();

        if (ok) {
            double us = std::chrono::duration<double, std::micro>(end - start).count();
            result.latencies.push_back(us);
        } else {
            result.errors++;
        }
    }

    close(fd);
}

static BenchConfig parseArgs(int argc, char* argv[]) {
    BenchConfig config;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            config.host = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            config.threads = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--requests") == 0 && i + 1 < argc) {
            config.requests = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--keyspace") == 0 && i + 1 < argc) {
            config.keyspace = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--read-ratio") == 0 && i + 1 < argc) {
            config.read_ratio = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--value-size") == 0 && i + 1 < argc) {
            config.value_size = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: cache_bench [options]\n"
                      << "  --host <addr>       Server host (default: 127.0.0.1)\n"
                      << "  --port <port>       Server port (default: 6380)\n"
                      << "  --threads <n>       Number of client threads (default: 4)\n"
                      << "  --requests <n>      Total requests across all threads (default: 100000)\n"
                      << "  --keyspace <n>      Number of unique keys (default: 10000)\n"
                      << "  --read-ratio <f>    Fraction of GETs, 0.0-1.0 (default: 0.8)\n"
                      << "  --value-size <n>    Size of SET values in bytes (default: 64)\n";
            std::exit(0);
        }
    }
    return config;
}

int main(int argc, char* argv[]) {
    BenchConfig config = parseArgs(argc, argv);

    std::cout << "=== CacheForge Benchmark ===\n"
              << "  Host:        " << config.host << ":" << config.port << "\n"
              << "  Threads:     " << config.threads << "\n"
              << "  Requests:    " << config.requests << "\n"
              << "  Keyspace:    " << config.keyspace << "\n"
              << "  Read ratio:  " << static_cast<int>(config.read_ratio * 100) << "% GET / "
              << static_cast<int>((1.0 - config.read_ratio) * 100) << "% SET\n"
              << "  Value size:  " << config.value_size << " bytes\n"
              << "\nRunning...\n";

    int base = config.requests / config.threads;
    int remainder = config.requests % config.threads;

    std::vector<ThreadResult> results(static_cast<size_t>(config.threads));
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(config.threads));

    auto overall_start = std::chrono::steady_clock::now();

    for (int t = 0; t < config.threads; ++t) {
        int n = base + (t < remainder ? 1 : 0);
        threads.emplace_back(workerThread, std::cref(config), t, n,
                             std::ref(results[static_cast<size_t>(t)]));
    }

    for (auto& th : threads) {
        th.join();
    }

    auto overall_end = std::chrono::steady_clock::now();
    double elapsed_s = std::chrono::duration<double>(overall_end - overall_start).count();

    // Merge latencies
    size_t total_samples = 0;
    size_t total_errors = 0;
    for (auto& r : results) {
        total_samples += r.latencies.size();
        total_errors += r.errors;
    }

    std::vector<double> all_latencies;
    all_latencies.reserve(total_samples);
    for (auto& r : results) {
        all_latencies.insert(all_latencies.end(), r.latencies.begin(), r.latencies.end());
    }

    std::sort(all_latencies.begin(), all_latencies.end());

    double p50 = 0, p95 = 0, p99 = 0;
    if (!all_latencies.empty()) {
        size_t n = all_latencies.size();
        p50 = all_latencies[n * 50 / 100];
        p95 = all_latencies[n * 95 / 100];
        p99 = all_latencies[n * 99 / 100];
    }

    double ops_per_sec = elapsed_s > 0 ? static_cast<double>(total_samples) / elapsed_s : 0;

    std::cout << "\n=== Results ===\n"
              << "  Total ops:    " << total_samples << "\n"
              << "  Elapsed:      " << std::fixed << std::setprecision(2) << elapsed_s << " s\n"
              << "  Throughput:   " << static_cast<long long>(ops_per_sec) << " ops/sec\n"
              << "  Latency p50:  " << static_cast<long long>(p50) << " us\n"
              << "  Latency p95:  " << static_cast<long long>(p95) << " us\n"
              << "  Latency p99:  " << static_cast<long long>(p99) << " us\n"
              << "  Errors:       " << total_errors << "\n";

    return 0;
}
