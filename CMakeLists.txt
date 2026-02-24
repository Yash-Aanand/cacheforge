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

# ThreadSanitizer option
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

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
)

# CLI client executable
add_executable(cache_cli
    tools/cache_cli.cpp
)

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

add_executable(test_storage
    tests/test_storage.cpp
    src/storage/storage.cpp
)

add_executable(test_sharded_storage
    tests/test_sharded_storage.cpp
    src/storage/sharded_storage.cpp
)

add_executable(test_ttl
    tests/test_ttl.cpp
    src/storage/sharded_storage.cpp
)

add_test(NAME parser_tests COMMAND test_parser)
add_test(NAME storage_tests COMMAND test_storage)
add_test(NAME sharded_storage_tests COMMAND test_sharded_storage)
add_test(NAME ttl_tests COMMAND test_ttl)
