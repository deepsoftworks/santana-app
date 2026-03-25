#pragma once
#include "data_source.hpp"
#include <string>
#include <deque>
#include <mutex>

class StdinSource : public DataSource {
public:
    StdinSource();
    ~StdinSource() override;

    double next() override;
    bool ready() const override;

    // Called from background thread to poll stdin
    void poll();

private:
    mutable std::mutex  mutex_;
    std::deque<double>  queue_;
    bool                eof_ = false;
};
