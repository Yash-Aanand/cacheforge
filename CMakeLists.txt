cmake_minimum_required(VERSION 3.16)
project(cacheforge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Static linking for Windows/MinGW to avoid DLL version mismatches
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

# ThreadSanitizer option
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# AddressSanitizer option
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# Threading
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Server executable
add_executable(cache_server
    src/server/main.cpp
    src/server/server.cpp
    src/server/connection.cpp
    src/server/event_loop.cpp
    src/server/thread_pool.cpp
    src/protocol/parser.cpp
    src/protocol/response.cpp
    src/protocol/dispatcher.cpp
    src/storage/sharded_storage.cpp
    src/storage/aof_writer.cpp
    src/storage/aof_replay.cpp
)

# CLI client executable
add_executable(cache_cli
    tools/cache_cli.cpp
)

# Benchmark tool (Linux only — connects to the epoll server)
add_executable(cache_bench
    tools/cache_bench.cpp
)
target_link_libraries(cache_bench Threads::Threads)

# Platform-specific settings
# Server requires Linux (epoll), CLI supports both Windows and Linux
if(WIN32)
    target_link_libraries(cache_cli ws2_32)
endif()

# Tests
enable_testing()

add_executable(test_parser
    tests/test_parser.cpp
    src/protocol/parser.cpp
)

add_executable(test_sharded_storage
    tests/test_sharded_storage.cpp
    src/storage/sharded_storage.cpp
)

add_executable(test_ttl
    tests/test_ttl.cpp
    src/storage/sharded_storage.cpp
)

add_executable(test_lru
    tests/test_lru.cpp
    src/storage/sharded_storage.cpp
)

add_executable(test_aof
    tests/test_aof.cpp
    src/storage/aof_writer.cpp
    src/storage/aof_replay.cpp
    src/storage/sharded_storage.cpp
    src/protocol/parser.cpp
)

add_test(NAME parser_tests COMMAND test_parser)
add_test(NAME sharded_storage_tests COMMAND test_sharded_storage)
add_test(NAME ttl_tests COMMAND test_ttl)
add_test(NAME lru_tests COMMAND test_lru)
add_executable(test_stats
    tests/test_stats.cpp
    src/protocol/dispatcher.cpp
    src/protocol/parser.cpp
    src/protocol/response.cpp
    src/storage/sharded_storage.cpp
    src/storage/aof_writer.cpp
)

add_test(NAME aof_tests COMMAND test_aof)
add_test(NAME stats_tests COMMAND test_stats)
