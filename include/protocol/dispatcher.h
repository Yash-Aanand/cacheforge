#ifndef CACHEFORGE_DISPATCHER_H
#define CACHEFORGE_DISPATCHER_H

#include "protocol/parser.h"
#include "storage/storage.h"
#include <string>

namespace cacheforge {

class Dispatcher {
public:
    explicit Dispatcher(Storage& storage);

    std::string dispatch(const Command& cmd);

private:
    Storage& storage_;
};

} // namespace cacheforge

#endif // CACHEFORGE_DISPATCHER_H
