#pragma once
#include "config.hpp"
#include "ring_buffer.hpp"
#include "stdin_source.hpp"
#include <thread>
#include <atomic>

class Renderer {
public:
    explicit Renderer(Config cfg);
    ~Renderer();

    // Blocks until the user quits (Ctrl+C or 'q')
    void run();

private:
    void reader_thread();
    void compute_range(double& y_min, double& y_max) const;

    Config             cfg_;
    RingBuffer<double> buffer_;
    StdinSource        source_;
    std::thread        reader_;
    std::atomic<bool>  running_{true};
};
