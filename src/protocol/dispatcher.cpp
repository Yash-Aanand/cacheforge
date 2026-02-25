#include "protocol/dispatcher.h"
#include "protocol/response.h"
#include "storage/sharded_storage.h"
#include "storage/aof_writer.h"

namespace cacheforge {

Dispatcher::Dispatcher(ShardedStorage& storage, AOFWriter* aof_writer)
    : storage_(storage), aof_writer_(aof_writer) {}

std::string Dispatcher::dispatch(const Command& cmd) {
    total_requests_++;

    switch (cmd.type) {
        case CommandType::PING:
            return pongResponse();

        case CommandType::SET:
            if (cmd.args.size() < 2) {
                return errorResponse("wrong number of arguments for 'set' command");
            }
            total_writes_++;
            storage_.set(cmd.args[0], cmd.args[1]);
            if (aof_writer_) {
                aof_writer_->logSet(cmd.args[0], cmd.args[1]);
            }
            return okResponse();

        case CommandType::GET: {
            if (cmd.args.empty()) {
                return errorResponse("wrong number of arguments for 'get' command");
            }
            total_reads_++;
            auto value = storage_.get(cmd.args[0]);
            if (value) {
                cache_hits_++;
                return valueResponse(*value);
            }
            cache_misses_++;
            return nilResponse();
        }

        case CommandType::DEL:
            if (cmd.args.empty()) {
                return errorResponse("wrong number of arguments for 'del' command");
            }
            total_writes_++;
            {
                bool deleted = storage_.del(cmd.args[0]);
                if (deleted && aof_writer_) {
                    aof_writer_->logDel(cmd.args[0]);
                }
                return integerResponse(deleted ? 1 : 0);
            }

        case CommandType::EXPIRE: {
            if (cmd.args.size() < 2) {
                return errorResponse("wrong number of arguments for 'expire' command");
            }
            int64_t seconds;
            try {
                seconds = std::stoll(cmd.args[1]);
            } catch (const std::exception&) {
                return errorResponse("value is not an integer or out of range");
            }
            bool success = storage_.expire(cmd.args[0], seconds);
            if (success && aof_writer_) {
                aof_writer_->logExpire(cmd.args[0], seconds);
            }
            return integerResponse(success ? 1 : 0);
        }

        case CommandType::TTL: {
            if (cmd.args.empty()) {
                return errorResponse("wrong number of arguments for 'ttl' command");
            }
            return integerResponse(storage_.ttl(cmd.args[0]));
        }

        case CommandType::STATS: {
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

            std::string stats;
            stats += "total_requests:" + std::to_string(total_requests_.load());
            stats += ",total_reads:" + std::to_string(total_reads_.load());
            stats += ",total_writes:" + std::to_string(total_writes_.load());
            stats += ",cache_hits:" + std::to_string(cache_hits_.load());
            stats += ",cache_misses:" + std::to_string(cache_misses_.load());
            stats += ",expired_keys:" + std::to_string(storage_.expiredKeysCount());
            stats += ",evicted_keys:" + std::to_string(storage_.evictedKeysCount());
            stats += ",current_keys:" + std::to_string(storage_.size());
            stats += ",uptime_seconds:" + std::to_string(uptime);

            return valueResponse(stats);
        }

        case CommandType::UNKNOWN:
        default:
            return errorResponse("unknown command");
    }
}

} // namespace cacheforge
