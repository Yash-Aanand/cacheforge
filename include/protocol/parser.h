#ifndef CACHEFORGE_PARSER_H
#define CACHEFORGE_PARSER_H

#include <string>
#include <string_view>
#include <vector>

namespace cacheforge {

enum class CommandType {
    PING,
    SET,
    GET,
    DEL,
    EXPIRE,
    TTL,
    STATS,
    UNKNOWN
};

struct Command {
    CommandType type;
    std::vector<std::string> args;
};

// Parse a command from input buffer
// Returns the parsed command
Command parseCommand(std::string_view input);

// Trim whitespace and handle \r\n tolerance
std::string_view trimCommand(std::string_view input);

} // namespace cacheforge

#endif // CACHEFORGE_PARSER_H
