#ifndef CACHEFORGE_DISPATCHER_H
#define CACHEFORGE_DISPATCHER_H

#include "protocol/parser.h"
#include <string>

namespace cacheforge {

class ShardedStorage;

class Dispatcher {
public:
    explicit Dispatcher(ShardedStorage& storage);

    std::string dispatch(const Command& cmd);

private:
    ShardedStorage& storage_;
};

} // namespace cacheforge

#endif // CACHEFORGE_DISPATCHER_H
