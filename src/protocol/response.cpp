#include "protocol/response.h"

namespace cacheforge {

std::string pongResponse() {
    return "+PONG\n";
}

std::string okResponse() {
    return "+OK\n";
}

std::string valueResponse(const std::string& value) {
    return "$" + value + "\n";
}

std::string nilResponse() {
    return "$nil\n";
}

std::string integerResponse(int value) {
    return ":" + std::to_string(value) + "\n";
}

std::string errorResponse(const std::string& message) {
    return "-ERR " + message + "\n";
}

std::string formatResponse(const Command& cmd) {
    switch (cmd.type) {
        case CommandType::PING:
            return pongResponse();
        case CommandType::SET:
            return errorResponse("wrong number of arguments for 'set' command");
        case CommandType::GET:
            return errorResponse("wrong number of arguments for 'get' command");
        case CommandType::DEL:
            return errorResponse("wrong number of arguments for 'del' command");
        case CommandType::UNKNOWN:
        default:
            return errorResponse("unknown command");
    }
}

} // namespace cacheforge
