#include "protocol/dispatcher.h"
#include "protocol/response.h"
#include "storage/sharded_storage.h"

namespace cacheforge {

Dispatcher::Dispatcher(ShardedStorage& storage) : storage_(storage) {}

std::string Dispatcher::dispatch(const Command& cmd) {
    switch (cmd.type) {
        case CommandType::PING:
            return pongResponse();

        case CommandType::SET:
            if (cmd.args.size() < 2) {
                return errorResponse("wrong number of arguments for 'set' command");
            }
            storage_.set(cmd.args[0], cmd.args[1]);
            return okResponse();

        case CommandType::GET: {
            if (cmd.args.empty()) {
                return errorResponse("wrong number of arguments for 'get' command");
            }
            auto value = storage_.get(cmd.args[0]);
            if (value) {
                return valueResponse(*value);
            }
            return nilResponse();
        }

        case CommandType::DEL:
            if (cmd.args.empty()) {
                return errorResponse("wrong number of arguments for 'del' command");
            }
            return integerResponse(storage_.del(cmd.args[0]) ? 1 : 0);

        case CommandType::EXPIRE: {
            if (cmd.args.size() < 2) {
                return errorResponse("wrong number of arguments for 'expire' command");
            }
            int64_t seconds;
            try {
                seconds = std::stoll(cmd.args[1]);
            } catch (...) {
                return errorResponse("value is not an integer or out of range");
            }
            return integerResponse(storage_.expire(cmd.args[0], seconds) ? 1 : 0);
        }

        case CommandType::TTL: {
            if (cmd.args.empty()) {
                return errorResponse("wrong number of arguments for 'ttl' command");
            }
            return integerResponse(storage_.ttl(cmd.args[0]));
        }

        case CommandType::UNKNOWN:
        default:
            return errorResponse("unknown command");
    }
}

} // namespace cacheforge
