#include "protocol/parser.h"
#include <algorithm>
#include <cctype>

namespace cacheforge {

namespace {
    std::string toUpper(std::string_view str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return result;
    }

    std::vector<std::string> tokenize(std::string_view input) {
        std::vector<std::string> tokens;
        size_t i = 0;

        while (i < input.size()) {
            // Skip whitespace
            while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) {
                ++i;
            }
            if (i >= input.size()) break;

            std::string token;
            token.reserve(32);
            if (input[i] == '"') {
                // Quoted string
                ++i;
                while (i < input.size() && input[i] != '"') {
                    if (input[i] == '\\' && i + 1 < input.size()) {
                        ++i;
                    }
                    token += input[i++];
                }
                if (i < input.size()) ++i; // skip closing quote
            } else {
                // Unquoted token
                while (i < input.size() && !std::isspace(static_cast<unsigned char>(input[i]))) {
                    token += input[i++];
                }
            }

            if (!token.empty()) {
                tokens.push_back(std::move(token));
            }
        }

        return tokens;
    }
}

std::string_view trimCommand(std::string_view input) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) {
        input.remove_prefix(1);
    }
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) {
        input.remove_suffix(1);
    }
    return input;
}

Command parseCommand(std::string_view input) {
    Command cmd;
    cmd.type = CommandType::UNKNOWN;

    std::string_view trimmed = trimCommand(input);
    if (trimmed.empty()) {
        return cmd;
    }

    std::vector<std::string> tokens = tokenize(trimmed);
    if (tokens.empty()) {
        return cmd;
    }

    std::string cmdName = toUpper(tokens[0]);

    if (cmdName == "PING") {
        cmd.type = CommandType::PING;
    } else if (cmdName == "SET") {
        cmd.type = CommandType::SET;
        if (tokens.size() >= 3) {
            cmd.args.push_back(std::move(tokens[1]));  // key
            cmd.args.push_back(std::move(tokens[2]));  // value
        }
    } else if (cmdName == "GET") {
        cmd.type = CommandType::GET;
        if (tokens.size() >= 2) {
            cmd.args.push_back(std::move(tokens[1]));  // key
        }
    } else if (cmdName == "DEL") {
        cmd.type = CommandType::DEL;
        if (tokens.size() >= 2) {
            cmd.args.push_back(std::move(tokens[1]));  // key
        }
    } else if (cmdName == "EXPIRE") {
        cmd.type = CommandType::EXPIRE;
        if (tokens.size() >= 3) {
            cmd.args.push_back(std::move(tokens[1]));  // key
            cmd.args.push_back(std::move(tokens[2]));  // seconds
        }
    } else if (cmdName == "TTL") {
        cmd.type = CommandType::TTL;
        if (tokens.size() >= 2) {
            cmd.args.push_back(std::move(tokens[1]));  // key
        }
    } else if (cmdName == "STATS") {
        cmd.type = CommandType::STATS;
    }

    return cmd;
}

} // namespace cacheforge
