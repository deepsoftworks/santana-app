#pragma once
#include "config.hpp"
#include "parser.hpp"
#include "ring_buffer.hpp"
#include "stdin_source.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

class Renderer {
public:
    Renderer(Config cfg, int input_fd,
             std::string first_line = "",
             LineFormat fmt = LineFormat::Single);
    ~Renderer();

    // Blocks until the user quits (q or Esc)
    void run();

private:
    void reader_thread();
    void compute_range(double& y_min, double& y_max) const;

    Config             cfg_;
    int                input_fd_ = -1;
    std::vector<std::unique_ptr<RingBuffer<double> > > buffers_;
    StdinSource        source_;
    std::thread        reader_;
    std::atomic<bool>  running_{true};
    std::atomic<bool>  rate_mode_;
    std::atomic<bool>  rate_reset_{false};
    std::atomic<bool>  eof_seen_{false};
};
