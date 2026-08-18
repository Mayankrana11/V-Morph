#include "PerformanceMonitor.h"
#include <chrono>

namespace rtvcc {

PerformanceMonitor::PerformanceMonitor() = default;
PerformanceMonitor::~PerformanceMonitor() = default;

void PerformanceMonitor::onAudioCallbackStart() {
    callback_start_ = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::onAudioCallbackEnd() {
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - callback_start_).count();

    callback_stats_.sum_ms.fetch_add(ms, std::memory_order_relaxed);
    callback_stats_.count.fetch_add(1, std::memory_order_relaxed);

    double current_max = callback_stats_.max_ms.load(std::memory_order_relaxed);
    while (ms > current_max && !callback_stats_.max_ms.compare_exchange_weak(current_max, ms)) {}

    double current_min = callback_stats_.min_ms.load(std::memory_order_relaxed);
    while (ms < current_min && !callback_stats_.min_ms.compare_exchange_weak(current_min, ms)) {}

    // Store in history for moving average
    size_t idx = history_index_.fetch_add(1, std::memory_order_relaxed) % HISTORY_SIZE;
    callback_history_[idx] = ms;
}

void PerformanceMonitor::onAudioUnderrun() {
    callback_stats_.underruns.fetch_add(1, std::memory_order_relaxed);
}

void PerformanceMonitor::onAudioOverrun() {
    callback_stats_.overruns.fetch_add(1, std::memory_order_relaxed);
}

void PerformanceMonitor::onAudioCallbackMissedDeadline() {
    callback_stats_.deadline_misses.fetch_add(1, std::memory_order_relaxed);
}

void PerformanceMonitor::onProcessingStart() {
    processing_start_ = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::onProcessingEnd() {
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - processing_start_).count();

    processing_stats_.sum_ms.fetch_add(ms, std::memory_order_relaxed);
    processing_stats_.count.fetch_add(1, std::memory_order_relaxed);

    double current_max = processing_stats_.max_ms.load(std::memory_order_relaxed);
    while (ms > current_max && !processing_stats_.max_ms.compare_exchange_weak(current_max, ms)) {}

    size_t idx = history_index_.fetch_add(1, std::memory_order_relaxed) % HISTORY_SIZE;
    processing_history_[idx] = ms;
}

void PerformanceMonitor::onInferenceStart() {
    inference_start_ = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::onInferenceEnd() {
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - inference_start_).count();

    inference_stats_.sum_ms.fetch_add(ms, std::memory_order_relaxed);
    inference_stats_.count.fetch_add(1, std::memory_order_relaxed);

    double current_max = inference_stats_.max_ms.load(std::memory_order_relaxed);
    while (ms > current_max && !inference_stats_.max_ms.compare_exchange_weak(current_max, ms)) {}
}

void PerformanceMonitor::setInputQueueDepth(size_t depth) {
    input_queue_depth_.store(depth, std::memory_order_relaxed);
}

void PerformanceMonitor::setOutputQueueDepth(size_t depth) {
    output_queue_depth_.store(depth, std::memory_order_relaxed);
}

void PerformanceMonitor::setModelInferenceTimeMs(double ms) {
    // Could track separately if needed
}

void PerformanceMonitor::setModelLoadTimeMs(double ms) {
    model_load_time_ms_.store(ms, std::memory_order_relaxed);
}

PerformanceMonitor::Snapshot PerformanceMonitor::getSnapshot() const {
    Snapshot snap;

    uint64_t cb_count = callback_stats_.count.load(std::memory_order_acquire);
    if (cb_count > 0) {
        snap.callback_avg_ms = callback_stats_.sum_ms.load(std::memory_order_acquire) / cb_count;
        snap.callback_max_ms = callback_stats_.max_ms.load(std::memory_order_acquire);
        snap.callback_min_ms = callback_stats_.min_ms.load(std::memory_order_acquire);
    }
    snap.callback_count = cb_count;
    snap.underruns = callback_stats_.underruns.load(std::memory_order_acquire);
    snap.overruns = callback_stats_.overruns.load(std::memory_order_acquire);
    snap.deadline_misses = callback_stats_.deadline_misses.load(std::memory_order_acquire);

    uint64_t proc_count = processing_stats_.count.load(std::memory_order_acquire);
    if (proc_count > 0) {
        snap.processing_avg_ms = processing_stats_.sum_ms.load(std::memory_order_acquire) / proc_count;
        snap.processing_max_ms = processing_stats_.max_ms.load(std::memory_order_acquire);
    }
    snap.processing_count = proc_count;

    uint64_t inf_count = inference_stats_.count.load(std::memory_order_acquire);
    if (inf_count > 0) {
        snap.inference_avg_ms = inference_stats_.sum_ms.load(std::memory_order_acquire) / inf_count;
        snap.inference_max_ms = inference_stats_.max_ms.load(std::memory_order_acquire);
    }
    snap.inference_count = inf_count;

    snap.input_queue_depth = input_queue_depth_.load(std::memory_order_acquire);
    snap.output_queue_depth = output_queue_depth_.load(std::memory_order_acquire);
    snap.model_load_time_ms = model_load_time_ms_.load(std::memory_order_acquire);

    // Calculate CPU usage estimate
    if (cb_count > 0) {
        double callback_budget_ms = 1000.0 / 48000.0 * 128; // ~2.67ms for 128 frames at 48kHz
        snap.cpu_usage_percent = (snap.callback_avg_ms / callback_budget_ms) * 100.0;
    }

    snap.timestamp = std::chrono::steady_clock::now();
    return snap;
}

void PerformanceMonitor::reset() {
    callback_stats_.sum_ms.store(0.0, std::memory_order_release);
    callback_stats_.max_ms.store(0.0, std::memory_order_release);
    callback_stats_.min_ms.store(1000.0, std::memory_order_release);
    callback_stats_.count.store(0, std::memory_order_release);
    callback_stats_.underruns.store(0, std::memory_order_release);
    callback_stats_.overruns.store(0, std::memory_order_release);
    callback_stats_.deadline_misses.store(0, std::memory_order_release);

    processing_stats_.sum_ms.store(0.0, std::memory_order_release);
    processing_stats_.max_ms.store(0.0, std::memory_order_release);
    processing_stats_.count.store(0, std::memory_order_release);

    inference_stats_.sum_ms.store(0.0, std::memory_order_release);
    inference_stats_.max_ms.store(0.0, std::memory_order_release);
    inference_stats_.count.store(0, std::memory_order_release);

    input_queue_depth_.store(0, std::memory_order_release);
    output_queue_depth_.store(0, std::memory_order_release);
    model_load_time_ms_.store(0.0, std::memory_order_release);

    history_index_.store(0, std::memory_order_release);
    std::fill(callback_history_.begin(), callback_history_.end(), 0.0);
    std::fill(processing_history_.begin(), processing_history_.end(), 0.0);
}

std::string PerformanceMonitor::toString() const {
    auto snap = getSnapshot();
    char buf[512];
    snprintf(buf, sizeof(buf),
        "Callback: %.2f/%.2f/%.2fms (count=%llu, underruns=%llu, overruns=%llu, misses=%llu)\n"
        "Processing: %.2f/%.2fms (count=%llu)\n"
        "Inference: %.2f/%.2fms (count=%llu)\n"
        "Queue: in=%zu out=%zu\n"
        "CPU: %.1f%%",
        snap.callback_avg_ms, snap.callback_min_ms, snap.callback_max_ms,
        snap.callback_count, snap.underruns, snap.overruns, snap.deadline_misses,
        snap.processing_avg_ms, snap.processing_max_ms, snap.processing_count,
        snap.inference_avg_ms, snap.inference_max_ms, snap.inference_count,
        snap.input_queue_depth, snap.output_queue_depth,
        snap.cpu_usage_percent);
    return std::string(buf);
}

} // namespace rtvcc