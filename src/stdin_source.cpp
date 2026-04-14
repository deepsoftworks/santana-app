#include "stdin_source.hpp"
#include <cstdio>
#include <poll.h>
#include <unistd.h>

StdinSource::StdinSource(int fd, std::string first_line)
    : fd_(fd)
{
    if (fd_ >= 0) {
        stream_ = ::fdopen(fd_, "r");
    }

    // Pre-queue the first line already consumed by main.
    if (!first_line.empty()) {
        auto row = parse_line(first_line);
        if (!row.empty()) {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push_back(std::move(row));
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

    auto row = parse_line(buf);
    if (!row.empty()) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= kMaxQueueSize) {
            queue_.pop_front();
        }
        queue_.push_back(std::move(row));
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

ParsedRow StdinSource::next_line() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) return {};
    auto row = std::move(queue_.front());
    queue_.pop_front();
    return row;
}

double StdinSource::next() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) return 0.0;
    double val = queue_.front().fields.empty() ? 0.0 : queue_.front().fields[0].value;
    queue_.pop_front();
    return val;
}
