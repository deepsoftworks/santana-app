#pragma once
#include "config.hpp"
#include "ring_buffer.hpp"
#include "stdin_source.hpp"
#include <thread>
#include <atomic>
#include <chrono>

class Renderer {
public:
    Renderer(Config cfg, int input_fd);
    ~Renderer();

    // Blocks until the user quits (q or Esc)
    void run();

private:
    void reader_thread();
    void compute_range(double& y_min, double& y_max) const;

    Config             cfg_;
    int                input_fd_ = -1;
    RingBuffer<double> buffer_;
    RingBuffer<double> buffer2_;    // two-graph mode, graph 2
    StdinSource        source_;
    std::thread        reader_;
    std::atomic<bool>  running_{true};
    std::atomic<bool>  rate_mode_;      // togglable at runtime with 'r'
    std::atomic<bool>  rate_reset_{false};
    std::atomic<bool>  eof_seen_{false};
};
