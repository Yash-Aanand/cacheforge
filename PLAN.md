# CacheForge - Redis-Compatible In-Memory Cache

A high-performance, Redis-compatible in-memory cache server built in C++20.

## Architecture Overview

CacheForge is designed as a learning project that implements core Redis functionality incrementally:

### Phase 1: Foundation (Current)
- Basic TCP server with blocking I/O
- Single client support
- PING/PONG protocol

### Phase 2: Data Structures
- In-memory key-value store (std::unordered_map)
- GET/SET/DEL commands
- String values

### Phase 3: Event Loop
- epoll-based I/O multiplexing (Linux)
- Multiple concurrent connections
- Non-blocking sockets

### Phase 4: Threading
- Thread pool for command execution
- Connection handling optimization

### Phase 5: Persistence
- AOF (Append-Only File) logging
- RDB snapshots

### Phase 6: Advanced Features
- TTL/expiration
- Pub/Sub
- Transactions (MULTI/EXEC)

## Protocol

CacheForge uses a simplified Redis-like text protocol:

**Commands:**
```
PING\n           -> +PONG\n
SET key value\n  -> +OK\n
GET key\n        -> +value\n or $-1\n (nil)
DEL key\n        -> :1\n (count deleted)
```

**Response Prefixes:**
- `+` Simple string
- `-` Error
- `:` Integer
- `$` Bulk string

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Running

```bash
# Start server (default port 6380)
./cache_server

# Connect with CLI
./cache_cli
```

## Design Principles

1. **Incremental Complexity** - Each phase builds on the previous
2. **Clean Abstractions** - Separate protocol, storage, and networking
3. **Modern C++** - C++20 features (std::jthread, std::format, designated initializers), RAII, no raw pointers
4. **Cross-Platform** - Windows and Linux support
