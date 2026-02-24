#ifndef CACHEFORGE_EVENT_LOOP_H
#define CACHEFORGE_EVENT_LOOP_H

#include <cstdint>
#include <vector>
#include <utility>

namespace cacheforge {

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Disable copy
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void addFd(int fd, uint32_t events);
    void modifyFd(int fd, uint32_t events);
    void removeFd(int fd);

    // Returns vector of {fd, events} pairs
    std::vector<std::pair<int, uint32_t>> wait(int timeout_ms = -1);

private:
    int epoll_fd_;
};

} // namespace cacheforge

#endif // CACHEFORGE_EVENT_LOOP_H
