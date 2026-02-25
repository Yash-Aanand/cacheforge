#include "protocol/parser.h"
#include <cassert>
#include <iostream>

using namespace cacheforge;

void test_ping() {
    Command cmd = parseCommand("PING");
    assert(cmd.type == CommandType::PING);
    assert(cmd.args.empty());

    cmd = parseCommand("ping");
    assert(cmd.type == CommandType::PING);

    cmd = parseCommand("  PING  \r\n");
    assert(cmd.type == CommandType::PING);
}

void test_set() {
    Command cmd = parseCommand("SET foo bar");
    assert(cmd.type == CommandType::SET);
    assert(cmd.args.size() == 2);
    assert(cmd.args[0] == "foo");
    assert(cmd.args[1] == "bar");

    cmd = parseCommand("set mykey myvalue");
    assert(cmd.type == CommandType::SET);
    assert(cmd.args[0] == "mykey");
    assert(cmd.args[1] == "myvalue");

    // Missing value
    cmd = parseCommand("SET onlykey");
    assert(cmd.type == CommandType::SET);
    assert(cmd.args.empty());
}

void test_get() {
    Command cmd = parseCommand("GET foo");
    assert(cmd.type == CommandType::GET);
    assert(cmd.args.size() == 1);
    assert(cmd.args[0] == "foo");

    // Missing key
    cmd = parseCommand("GET");
    assert(cmd.type == CommandType::GET);
    assert(cmd.args.empty());
}

void test_del() {
    Command cmd = parseCommand("DEL foo");
    assert(cmd.type == CommandType::DEL);
    assert(cmd.args.size() == 1);
    assert(cmd.args[0] == "foo");

    cmd = parseCommand("del bar");
    assert(cmd.type == CommandType::DEL);
    assert(cmd.args[0] == "bar");
}

void test_unknown() {
    Command cmd = parseCommand("INVALID");
    assert(cmd.type == CommandType::UNKNOWN);

    cmd = parseCommand("");
    assert(cmd.type == CommandType::UNKNOWN);

    cmd = parseCommand("   ");
    assert(cmd.type == CommandType::UNKNOWN);
}

void test_quoted_strings() {
    Command cmd = parseCommand("SET foo \"hello world\"");
    assert(cmd.type == CommandType::SET);
    assert(cmd.args.size() == 2);
    assert(cmd.args[0] == "foo");
    assert(cmd.args[1] == "hello world");
}

void test_expire() {
    Command cmd = parseCommand("EXPIRE mykey 60");
    assert(cmd.type == CommandType::EXPIRE);
    assert(cmd.args.size() == 2);
    assert(cmd.args[0] == "mykey");
    assert(cmd.args[1] == "60");

    cmd = parseCommand("expire foo 3600");
    assert(cmd.type == CommandType::EXPIRE);
    assert(cmd.args[0] == "foo");
    assert(cmd.args[1] == "3600");

    // Missing seconds
    cmd = parseCommand("EXPIRE onlykey");
    assert(cmd.type == CommandType::EXPIRE);
    assert(cmd.args.empty());
}

void test_ttl() {
    Command cmd = parseCommand("TTL mykey");
    assert(cmd.type == CommandType::TTL);
    assert(cmd.args.size() == 1);
    assert(cmd.args[0] == "mykey");

    cmd = parseCommand("ttl foo");
    assert(cmd.type == CommandType::TTL);
    assert(cmd.args[0] == "foo");

    // Missing key
    cmd = parseCommand("TTL");
    assert(cmd.type == CommandType::TTL);
    assert(cmd.args.empty());
}

void test_stats() {
    Command cmd = parseCommand("STATS");
    assert(cmd.type == CommandType::STATS);
    assert(cmd.args.empty());

    cmd = parseCommand("stats");
    assert(cmd.type == CommandType::STATS);
    assert(cmd.args.empty());

    cmd = parseCommand("  STATS  \r\n");
    assert(cmd.type == CommandType::STATS);
    assert(cmd.args.empty());
}

int main() {
    test_ping();
    std::cout << "test_ping passed\n";

    test_set();
    std::cout << "test_set passed\n";

    test_get();
    std::cout << "test_get passed\n";

    test_del();
    std::cout << "test_del passed\n";

    test_unknown();
    std::cout << "test_unknown passed\n";

    test_quoted_strings();
    std::cout << "test_quoted_strings passed\n";

    test_expire();
    std::cout << "test_expire passed\n";

    test_ttl();
    std::cout << "test_ttl passed\n";

    test_stats();
    std::cout << "test_stats passed\n";

    std::cout << "\nAll parser tests passed!\n";
    return 0;
}
