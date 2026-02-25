# CacheForge

A high-performance, multithreaded in-memory cache server written in modern C++20 with Redis-compatible protocol.

## Features

- **Sharded key-value storage** with fine-grained locking for high concurrency
- **LRU eviction** — configurable max capacity with least-recently-used eviction
- **TTL expiration** — per-key time-to-live with background sweep
- **AOF persistence** — append-only file logging with crash recovery and replay
- **Epoll-based event loop** — non-blocking I/O for thousands of concurrent connections
- **Thread pool** — configurable worker threads for parallel command execution
- **Redis-compatible protocol** — works with standard Redis CLI tools

## Architecture

```
  Clients
    |
    v
+-----------+     +-----------+     +------------+     +------------------+
|  Event    | --> |  Parser   | --> | Dispatcher | --> |   Thread Pool    |
|  Loop     |     | (RESP-    |     | (command   |     |  (N workers)     |
| (epoll)   |     |  like)    |     |  routing)  |     +--------+---------+
+-----------+     +-----------+     +------------+              |
                                         |                      v
                                         |              +------------------+
                                         +-- AOF -----> | Sharded Storage  |
                                         |  Writer      | (16 shards,      |
                                         |              |  LRU + TTL)      |
                                         v              +------------------+
                                    [cache.aof]
```

## Build Instructions

Requires Linux or WSL with GCC 12+ (C++20 support) and CMake 3.20+.

```bash
# Clone and build
git clone <repo-url> cacheforge
cd cacheforge
mkdir build-linux && cd build-linux
cmake ..
make -j$(nproc)
```

## Usage

### Start the server

```bash
# Default: port 6380, auto threads, AOF enabled
./cacheforge_server

# Custom configuration
./cacheforge_server -p 6380 -t 4 --aof-enabled true --aof-path ./cache.aof
```

### Interactive CLI

```bash
./cache_cli localhost 6380
> SET greeting "hello world"
+OK
> GET greeting
$hello world
> EXPIRE greeting 60
:1
> TTL greeting
:59
> DEL greeting
:1
```

### Benchmark

```bash
# Run with 4 client threads, 10000 operations each
./cache_bench localhost 6380 4 10000
```

## Benchmark Results

Measured on Linux with loopback networking, varying client thread counts:

| Threads | Throughput    | p50   | p95   | p99   |
|---------|--------------|-------|-------|-------|
| 1       | 14,913 ops/s | 61us  | 103us | 157us |
| 2       | 21,684 ops/s | 87us  | 126us | 175us |
| 4       | 40,785 ops/s | 86us  | 176us | 262us |
| 8       | 41,274 ops/s | 145us | 332us | 459us |
| 16      | 39,585 ops/s | 322us | 708us | 950us |

## Protocol Reference

CacheForge uses a line-based text protocol. Commands are newline-terminated.

| Command                    | Description                        | Response       |
|----------------------------|------------------------------------|----------------|
| `PING`                     | Health check                       | `+PONG`        |
| `SET <key> <value>`        | Store a key-value pair             | `+OK`          |
| `SET <key> <value> EX <s>` | Store with TTL in seconds          | `+OK`          |
| `GET <key>`                | Retrieve value by key              | `$<value>` or `$nil` |
| `DEL <key>`                | Delete a key                       | `:1` or `:0`   |
| `EXPIRE <key> <seconds>`   | Set TTL on existing key            | `:1` or `:0`   |
| `TTL <key>`                | Get remaining TTL (-1=none, -2=missing) | `:<seconds>` |
| `STATS`                    | Server statistics                  | Multi-line     |

## Project Structure

```
cacheforge/
├── CMakeLists.txt
├── include/
│   ├── protocol/
│   │   ├── dispatcher.h       # Command routing
│   │   ├── parser.h           # Protocol parsing
│   │   └── response.h         # Response formatting
│   ├── server/
│   │   ├── connection.h       # Per-client connection state
│   │   ├── event_loop.h       # Epoll wrapper
│   │   ├── server.h           # Main server class
│   │   └── thread_pool.h      # Worker thread pool
│   └── storage/
│       ├── aof_replay.h       # AOF file replay on startup
│       ├── aof_writer.h       # Async append-only file writer
│       └── sharded_storage.h  # Sharded hash map with LRU + TTL
├── src/
│   ├── protocol/
│   │   ├── dispatcher.cpp
│   │   ├── parser.cpp
│   │   └── response.cpp
│   ├── server/
│   │   ├── connection.cpp
│   │   ├── event_loop.cpp
│   │   ├── main.cpp
│   │   ├── server.cpp
│   │   └── thread_pool.cpp
│   └── storage/
│       ├── aof_replay.cpp
│       ├── aof_writer.cpp
│       └── sharded_storage.cpp
├── tests/
│   ├── test_aof.cpp
│   ├── test_lru.cpp
│   ├── test_parser.cpp
│   ├── test_sharded_storage.cpp
│   ├── test_stats.cpp
│   └── test_ttl.cpp
└── tools/
    ├── cache_bench.cpp        # Benchmark tool
    └── cache_cli.cpp          # Interactive CLI client
```
