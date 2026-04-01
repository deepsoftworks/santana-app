#pragma once
#include "data_source.hpp"
#include "parser.hpp"
#include <cstdio>
#include <string>
#include <deque>
#include <mutex>
#include <vector>
#include <cstddef>

class StdinSource : public DataSource {
public:
    explicit StdinSource(int fd,
                         std::string first_line = "",
                         LineFormat fmt = LineFormat::Single);
    ~StdinSource() override;

    // Returns all values parsed from the next queued line.
    std::vector<double> next_line();

    bool ready() const override;
    bool is_eof() const;

    // Called from background thread to poll stdin
    void poll();

    // DataSource interface — returns first value of next line (legacy)
    double next() override;

private:
    static constexpr std::size_t kMaxQueueSize = 100000;

    int                              fd_     = -1;
    FILE*                            stream_ = nullptr;
    LineFormat                       fmt_    = LineFormat::Single;
    mutable std::mutex               mutex_;
    std::deque<std::vector<double>>  queue_;
    bool                             eof_    = false;
};
