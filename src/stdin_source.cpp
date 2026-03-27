#include "stdin_source.hpp"
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <poll.h>

StdinSource::StdinSource() = default;
StdinSource::~StdinSource() = default;

void StdinSource::poll() {
    struct pollfd pfd{};
    pfd.fd     = STDIN_FILENO;
    pfd.events = POLLIN;

    // Non-blocking poll with 50ms timeout
    int ret = ::poll(&pfd, 1, 50);
    if (ret <= 0) return;
    if (!(pfd.revents & POLLIN)) {
        if (pfd.revents & (POLLHUP | POLLERR)) {
            std::unique_lock<std::mutex> lock(mutex_);
            eof_ = true;
        }
        return;
    }

    char buf[256];
    if (!fgets(buf, sizeof(buf), stdin)) {
        std::unique_lock<std::mutex> lock(mutex_);
        eof_ = true;
        return;
    }

    char* end;
    double val = std::strtod(buf, &end);
    if (end != buf) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push_back(val);
    }
}

bool StdinSource::is_eof() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return eof_;
}

bool StdinSource::ready() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return !queue_.empty();
}

double StdinSource::next() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) return 0.0;
    double val = queue_.front();
    queue_.pop_front();
    return val;
}
