#ifndef CACHEFORGE_RESPONSE_H
#define CACHEFORGE_RESPONSE_H

#include <string>
#include "protocol/parser.h"

namespace cacheforge {

// Format a response for the given command
std::string formatResponse(const Command& cmd);

// Standard responses
std::string pongResponse();
std::string okResponse();
std::string valueResponse(const std::string& value);
std::string nilResponse();
std::string integerResponse(int value);
std::string errorResponse(const std::string& message);

} // namespace cacheforge

#endif // CACHEFORGE_RESPONSE_H
