#include "stdin_source.hpp"
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <poll.h>
#include <unistd.h>

StdinSource::StdinSource(int fd, std::string first_line, LineFormat fmt)
    : fd_(fd), fmt_(fmt) {
    if (fd_ >= 0) {
        stream_ = ::fdopen(fd_, "r");
    }
    // Pre-queue the first line already consumed by main for format detection
    if (!first_line.empty()) {
        auto vals = parse_line(first_line, fmt_);
        if (!vals.empty()) {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push_back(std::move(vals));
        }
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

    char buf[4096];
    if (!::fgets(buf, sizeof(buf), stream_)) {
        std::unique_lock<std::mutex> lock(mutex_);
        eof_ = true;
        return;
    }

    auto vals = parse_line(buf, fmt_);
    if (!vals.empty()) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= kMaxQueueSize) {
            queue_.pop_front();
        }
        queue_.push_back(std::move(vals));
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

std::vector<double> StdinSource::next_line() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) return {};
    auto row = std::move(queue_.front());
    queue_.pop_front();
    return row;
}

double StdinSource::next() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) return 0.0;
    double val = queue_.front().empty() ? 0.0 : queue_.front()[0];
    queue_.pop_front();
    return val;
}
