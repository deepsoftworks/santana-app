#pragma once
#include "config.hpp"
#include "ring_buffer.hpp"
#include "stdin_source.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Renderer {
public:
    Renderer(Config cfg, int input_fd, std::string first_line = "");
    ~Renderer();

    // Blocks until the user quits (q or Esc)
    void run();

private:
    void reader_thread();

    struct StreamSlot {
        std::string label;
        std::unique_ptr<RingBuffer<double>> buffer;
        double prev_val = 0.0;
        std::chrono::steady_clock::time_point prev_time{};
        bool has_prev = false;
    };

    size_t ensure_stream_locked(const std::string& label);

    Config                       cfg_;
    int                          input_fd_ = -1;
    StdinSource                  source_;
    std::thread                  reader_;
    std::atomic<bool>            running_{true};
    std::atomic<bool>            rate_mode_;
    std::atomic<bool>            rate_reset_{false};
    std::atomic<bool>            eof_seen_{false};
    mutable std::mutex           streams_mutex_;
    std::vector<StreamSlot>      streams_;
    std::unordered_map<std::string, size_t> stream_index_;
};
