#ifndef CACHEFORGE_AOF_REPLAY_H
#define CACHEFORGE_AOF_REPLAY_H

#include <cstddef>
#include <string>

namespace cacheforge {

class ShardedStorage;

class AOFReplay {
public:
    struct Stats {
        size_t commands_replayed = 0;
        size_t lines_skipped = 0;
        size_t errors = 0;
    };

    explicit AOFReplay(ShardedStorage& storage);

    Stats replay(const std::string& path);

private:
    ShardedStorage& storage_;
};

} // namespace cacheforge

#endif // CACHEFORGE_AOF_REPLAY_H
