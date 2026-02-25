#include "protocol/response.h"

namespace cacheforge {

std::string pongResponse() {
    return "+PONG\n";
}

std::string okResponse() {
    return "+OK\n";
}

std::string valueResponse(const std::string& value) {
    std::string result;
    result.reserve(1 + value.size() + 1);
    result.append("$");
    result.append(value);
    result.append("\n");
    return result;
}

std::string nilResponse() {
    return "$nil\n";
}

std::string integerResponse(int value) {
    std::string num = std::to_string(value);
    std::string result;
    result.reserve(1 + num.size() + 1);
    result.append(":");
    result.append(num);
    result.append("\n");
    return result;
}

std::string errorResponse(const std::string& message) {
    std::string result;
    result.reserve(5 + message.size() + 1);
    result.append("-ERR ");
    result.append(message);
    result.append("\n");
    return result;
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
