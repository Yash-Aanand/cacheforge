#include "protocol/parser.h"
#include <algorithm>
#include <cctype>

namespace cacheforge {

std::string_view trimCommand(std::string_view input) {
    // Trim leading whitespace
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) {
        input.remove_prefix(1);
    }

    // Trim trailing whitespace (handles \r\n, \n, spaces)
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) {
        input.remove_suffix(1);
    }

    return input;
}

Command parseCommand(std::string_view input) {
    Command cmd;
    cmd.raw = std::string(input);

    std::string_view trimmed = trimCommand(input);

    // Case-insensitive comparison for PING
    if (trimmed.size() == 4) {
        std::string upper;
        upper.reserve(4);
        for (char c : trimmed) {
            upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (upper == "PING") {
            cmd.type = CommandType::PING;
            return cmd;
        }
    }

    cmd.type = CommandType::UNKNOWN;
    return cmd;
}

} // namespace cacheforge
