#include "AudioClock.h"
#include <chrono>

namespace rtvcc {

void AudioClock::start() {
    start_time_ = Clock::now();
    last_tick_ = start_time_;
    frame_count_.store(0, std::memory_order_release);
    running_.store(true, std::memory_order_release);
}

void AudioClock::stop() {
    running_.store(false, std::memory_order_release);
}

void AudioClock::tick(size_t frames, double sample_rate) {
    if (!running_.load(std::memory_order_acquire)) return;

    frame_count_.fetch_add(frames, std::memory_order_relaxed);
    last_tick_ = Clock::now();
}

double AudioClock::getTimeSeconds() const {
    if (!running_.load(std::memory_order_acquire)) return 0.0;
    auto now = Clock::now();
    return std::chrono::duration<double>(now - start_time_).count();
}

uint64_t AudioClock::getFramePosition() const {
    return frame_count_.load(std::memory_order_acquire);
}

double AudioClock::getTimeSinceLastTick() const {
    if (!running_.load(std::memory_order_acquire)) return 0.0;
    auto now = Clock::now();
    return std::chrono::duration<double>(now - last_tick_).count();
}

bool AudioClock::isRunning() const {
    return running_.load(std::memory_order_acquire);
}

void AudioClock::reset() {
    start_time_ = Clock::now();
    last_tick_ = start_time_;
    frame_count_.store(0, std::memory_order_release);
}

double AudioClock::framesToSeconds(uint64_t frames, double sample_rate) const {
    return static_cast<double>(frames) / sample_rate;
}

uint64_t AudioClock::secondsToFrames(double seconds, double sample_rate) const {
    return static_cast<uint64_t>(seconds * sample_rate);
}

// LatencyTimer
void LatencyTimer::start() {
    start_ = std::chrono::high_resolution_clock::now();
}

void LatencyTimer::stop() {
    end_ = std::chrono::high_resolution_clock::now();
}

double LatencyTimer::elapsedMs() const {
    return std::chrono::duration<double, std::milli>(end_ - start_).count();
}

double LatencyTimer::elapsedUs() const {
    return std::chrono::duration<double, std::micro>(end_ - start_).count();
}

template <typename Func>
double LatencyTimer::measure(Func&& func) {
    start();
    func();
    stop();
    return elapsedUs();
}

} // namespace rtvcc