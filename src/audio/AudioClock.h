#pragma once

#include <chrono>
#include <atomic>

namespace rtvcc {

class AudioClock {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    AudioClock() = default;

    // Start the clock
    void start() {
        start_time_ = Clock::now();
        last_tick_ = start_time_;
        frame_count_ = 0;
        running_.store(true, std::memory_order_release);
    }

    // Stop the clock
    void stop() {
        running_.store(false, std::memory_order_release);
    }

    // Call once per audio callback with frame count
    void tick(size_t frames, double sample_rate) {
        if (!running_.load(std::memory_order_acquire)) return;

        frame_count_ += frames;
        last_tick_ = Clock::now();
    }

    // Get current time in seconds
    double getTimeSeconds() const {
        if (!running_.load(std::memory_order_acquire)) return 0.0;
        auto now = Clock::now();
        return std::chrono::duration<double>(now - start_time_).count();
    }

    // Get current frame position
    uint64_t getFramePosition() const {
        return frame_count_.load(std::memory_order_acquire);
    }

    // Get time since last tick
    double getTimeSinceLastTick() const {
        if (!running_.load(std::memory_order_acquire)) return 0.0;
        auto now = Clock::now();
        return std::chrono::duration<double>(now - last_tick_).count();
    }

    // Check if clock is running
    bool isRunning() const {
        return running_.load(std::memory_order_acquire);
    }

    // Reset clock
    void reset() {
        start_time_ = Clock::now();
        last_tick_ = start_time_;
        frame_count_.store(0, std::memory_order_release);
    }

    // Convert frame count to time
    double framesToSeconds(uint64_t frames, double sample_rate) const {
        return static_cast<double>(frames) / sample_rate;
    }

    // Convert time to frame count
    uint64_t secondsToFrames(double seconds, double sample_rate) const {
        return static_cast<uint64_t>(seconds * sample_rate);
    }

private:
    TimePoint start_time_;
    TimePoint last_tick_;
    std::atomic<uint64_t> frame_count_{0};
    std::atomic<bool> running_{false};
};

// High-resolution timer for latency measurement
class LatencyTimer {
public:
    LatencyTimer() = default;

    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        end_ = std::chrono::high_resolution_clock::now();
    }

    double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(end_ - start_).count();
    }

    double elapsedUs() const {
        return std::chrono::duration<double, std::micro>(end_ - start_).count();
    }

    // Measure a function execution time
    template <typename Func>
    double measure(Func&& func) {
        start();
        func();
        stop();
        return elapsedUs();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point end_;
};

} // namespace rtvcc