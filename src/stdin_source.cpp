#include "stdin_source.hpp"
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <poll.h>
#include <unistd.h>

StdinSource::StdinSource(int fd)
    : fd_(fd) {
    if (fd_ >= 0) {
        stream_ = ::fdopen(fd_, "r");
    }
}

StdinSource::~StdinSource() {
    if (stream_) {
        ::fclose(stream_);
        stream_ = nullptr;
    } else if (fd_ >= 0) {
        ::close(fd_);
    }
}

void StdinSource::poll() {
    if (fd_ < 0 || !stream_) {
        std::unique_lock<std::mutex> lock(mutex_);
        eof_ = true;
        return;
    }

    struct pollfd pfd{};
    pfd.fd     = fd_;
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
    if (!::fgets(buf, sizeof(buf), stream_)) {
        std::unique_lock<std::mutex> lock(mutex_);
        eof_ = true;
        return;
    }

    char* end;
    double val = std::strtod(buf, &end);
    if (end != buf) {
        std::unique_lock<std::mutex> lock(mutex_);
        // Keep bounded memory under sustained producer > consumer load.
        if (queue_.size() >= kMaxQueueSize) {
            queue_.pop_front();
        }
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
