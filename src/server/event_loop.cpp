#include "server/event_loop.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>

namespace cacheforge {

namespace {
    constexpr int MAX_EVENTS = 64;
}

EventLoop::EventLoop() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        throw std::runtime_error("Failed to create epoll instance");
    }
}

EventLoop::~EventLoop() {
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
}

void EventLoop::addFd(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw std::runtime_error("Failed to add fd to epoll");
    }
}

void EventLoop::modifyFd(int fd, uint32_t events) {
    struct epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        throw std::runtime_error("Failed to modify fd in epoll");
    }
}

void EventLoop::removeFd(int fd) {
    // EPOLL_CTL_DEL ignores the event parameter, but some older kernels require non-null
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

std::vector<std::pair<int, uint32_t>> EventLoop::wait(int timeout_ms) {
    struct epoll_event events[MAX_EVENTS];

    int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout_ms);

    std::vector<std::pair<int, uint32_t>> result;
    result.reserve(static_cast<size_t>(n > 0 ? n : 0));

    for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;
        uint32_t ev = events[i].events;
        result.emplace_back(fd, ev);
    }

    return result;
}

} // namespace cacheforge
