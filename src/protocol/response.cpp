#include "protocol/response.h"

namespace cacheforge {

std::string pongResponse() {
    return "+PONG\n";
}

std::string errorResponse(const std::string& message) {
    return "-ERR " + message + "\n";
}

std::string formatResponse(const Command& cmd) {
    switch (cmd.type) {
        case CommandType::PING:
            return pongResponse();
        case CommandType::UNKNOWN:
        default:
            return errorResponse("unknown command");
    }
}

} // namespace cacheforge
