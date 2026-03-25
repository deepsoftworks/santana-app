#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <limits>
#include <optional>

template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : capacity_(capacity), data_(capacity), head_(0), size_(0) {}

    void push(T value) {
        std::unique_lock lock(mutex_);
        data_[head_] = value;
        head_ = (head_ + 1) % capacity_;
        if (size_ < capacity_) ++size_;
        update_stats(value);
        cv_.notify_one();
    }

    // Returns a snapshot of [oldest .. newest]
    std::vector<T> snapshot() const {
        std::unique_lock lock(mutex_);
        std::vector<T> out;
        out.reserve(size_);
        size_t start = (head_ + capacity_ - size_) % capacity_;
        for (size_t i = 0; i < size_; ++i)
            out.push_back(data_[(start + i) % capacity_]);
        return out;
    }

    size_t size() const {
        std::unique_lock lock(mutex_);
        return size_;
    }

    struct Stats {
        T    current = T{};
        T    min_val = T{};
        T    max_val = T{};
        double mean  = 0.0;
        size_t count = 0;
    };

    Stats stats() const {
        std::unique_lock lock(mutex_);
        return stats_;
    }

    void resize(size_t new_cap) {
        std::unique_lock lock(mutex_);
        auto snap = snapshot_locked();
        capacity_ = new_cap;
        data_.assign(new_cap, T{});
        head_ = 0;
        size_ = 0;
        stats_ = {};
        sum_ = 0.0;
        for (auto& v : snap) {
            if (size_ >= new_cap) break;
            data_[head_] = v;
            head_ = (head_ + 1) % capacity_;
            ++size_;
            update_stats_locked(v);
        }
    }

private:
    std::vector<T> snapshot_locked() const {
        std::vector<T> out;
        out.reserve(size_);
        size_t start = (head_ + capacity_ - size_) % capacity_;
        for (size_t i = 0; i < size_; ++i)
            out.push_back(data_[(start + i) % capacity_]);
        return out;
    }

    void update_stats(T value) {
        update_stats_locked(value);
    }

    void update_stats_locked(T value) {
        stats_.current = value;
        stats_.count = size_;
        if (size_ == 1) {
            stats_.min_val = value;
            stats_.max_val = value;
            sum_ = static_cast<double>(value);
        } else {
            if (value < stats_.min_val) stats_.min_val = value;
            if (value > stats_.max_val) stats_.max_val = value;
            sum_ += static_cast<double>(value);
        }
        stats_.mean = (stats_.count > 0) ? sum_ / static_cast<double>(stats_.count) : 0.0;
    }

    size_t          capacity_;
    std::vector<T>  data_;
    size_t          head_;
    size_t          size_;
    Stats           stats_;
    double          sum_ = 0.0;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
