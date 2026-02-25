#include "storage/aof_replay.h"
#include "storage/sharded_storage.h"
#include "protocol/parser.h"

#include <fstream>
#include <iostream>

namespace cacheforge {

AOFReplay::AOFReplay(ShardedStorage& storage) : storage_(storage) {}

AOFReplay::Stats AOFReplay::replay(const std::string& path) {
    Stats stats{};
    std::ifstream file(path);
    if (!file.is_open()) {
        return stats;  // No file = fresh start
    }

    std::string line;
    size_t line_num = 0;
    while (std::getline(file, line)) {
        ++line_num;
        if (line.empty()) {
            ++stats.lines_skipped;
            continue;
        }

        try {
            Command cmd = parseCommand(line);
            switch (cmd.type) {
                case CommandType::SET:
                    if (cmd.args.size() >= 2) {
                        storage_.set(cmd.args[0], cmd.args[1]);
                        ++stats.commands_replayed;
                    } else {
                        ++stats.errors;
                        std::cerr << "AOF line " << line_num << " skipped: SET requires 2 arguments\n";
                    }
                    break;
                case CommandType::DEL:
                    if (!cmd.args.empty()) {
                        storage_.del(cmd.args[0]);
                        ++stats.commands_replayed;
                    } else {
                        ++stats.errors;
                        std::cerr << "AOF line " << line_num << " skipped: DEL requires 1 argument\n";
                    }
                    break;
                case CommandType::EXPIRE:
                    if (cmd.args.size() >= 2) {
                        int64_t seconds = std::stoll(cmd.args[1]);
                        if (seconds <= 0) {
                            ++stats.errors;
                            std::cerr << "AOF line " << line_num << " skipped: EXPIRE TTL must be positive\n";
                            break;
                        }
                        storage_.expire(cmd.args[0], seconds);
                        ++stats.commands_replayed;
                    } else {
                        ++stats.errors;
                        std::cerr << "AOF line " << line_num << " skipped: EXPIRE requires 2 arguments\n";
                    }
                    break;
                default:
                    // Skip read-only or unknown commands
                    ++stats.lines_skipped;
                    break;
            }
        } catch (const std::exception& e) {
            ++stats.errors;
            std::cerr << "AOF line " << line_num << " skipped: " << e.what() << "\n";
        }
    }
    return stats;
}

} // namespace cacheforge
