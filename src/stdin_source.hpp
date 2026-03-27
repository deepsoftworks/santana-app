#pragma once
#include "data_source.hpp"
#include <cstdio>
#include <string>
#include <deque>
#include <mutex>

class StdinSource : public DataSource {
public:
    explicit StdinSource(int fd);
    ~StdinSource() override;

    double next() override;
    bool ready() const override;
    bool is_eof() const;

    // Called from background thread to poll stdin
    void poll();

private:
    int                 fd_ = -1;
    FILE*               stream_ = nullptr;
    mutable std::mutex  mutex_;
    std::deque<double>  queue_;
    bool                eof_ = false;
};
