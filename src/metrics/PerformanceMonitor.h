#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <array>
#include <vector>

namespace rtvcc {

// Real-time safe performance counters
// Updated from audio callback and processing thread
// Read from UI thread (non-real-time)

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();

    // Audio callback metrics (updated from real-time thread)
    void onAudioCallbackStart();
    void onAudioCallbackEnd();
    void onAudioUnderrun();
    void onAudioOverrun();
    void onAudioCallbackMissedDeadline();

    // Processing thread metrics
    void onProcessingStart();
    void onProcessingEnd();
    void onInferenceStart();
    void onInferenceEnd();

    // Queue metrics
    void setInputQueueDepth(size_t depth);
    void setOutputQueueDepth(size_t depth);

    // Model metrics
    void setModelInferenceTimeMs(double ms);
    void setModelLoadTimeMs(double ms);

    // Get snapshot for UI (non-real-time thread)
    struct Snapshot {
        // Audio callback
        double callback_avg_ms = 0.0;
        double callback_max_ms = 0.0;
        double callback_min_ms = 0.0;
        uint64_t callback_count = 0;
        uint64_t underruns = 0;
        uint64_t overruns = 0;
        uint64_t deadline_misses = 0;

        // Processing
        double processing_avg_ms = 0.0;
        double processing_max_ms = 0.0;
        uint64_t processing_count = 0;

        // Inference
        double inference_avg_ms = 0.0;
        double inference_max_ms = 0.0;
        uint64_t inference_count = 0;

        // Queue
        size_t input_queue_depth = 0;
        size_t output_queue_depth = 0;

        // Model
        double model_load_time_ms = 0.0;

        // Derived
        double cpu_usage_percent = 0.0;
        double realtime_factor = 0.0;

        std::chrono::steady_clock::time_point timestamp;
    };

    Snapshot getSnapshot() const;

    // Reset all counters
    void reset();

    // Get formatted string for logging
    std::string toString() const;

private:
    // Ring buffers for averaging (lock-free, single writer)
    static constexpr size_t HISTORY_SIZE = 1000;

    struct alignas(64) CallbackStats {
        std::atomic<double> sum_ms{0.0};
        std::atomic<double> max_ms{0.0};
        std::atomic<double> min_ms{1000.0};
        std::atomic<uint64_t> count{0};
        std::atomic<uint64_t> underruns{0};
        std::atomic<uint64_t> overruns{0};
        std::atomic<uint64_t> deadline_misses{0};
    };

    struct alignas(64) ProcessingStats {
        std::atomic<double> sum_ms{0.0};
        std::atomic<double> max_ms{0.0};
        std::atomic<uint64_t> count{0};
    };

    struct alignas(64) InferenceStats {
        std::atomic<double> sum_ms{0.0};
        std::atomic<double> max_ms{0.0};
        std::atomic<uint64_t> count{0};
    };

    alignas(64) CallbackStats callback_stats_;
    alignas(64) ProcessingStats processing_stats_;
    alignas(64) InferenceStats inference_stats_;

    alignas(64) std::atomic<size_t> input_queue_depth_{0};
    alignas(64) std::atomic<size_t> output_queue_depth_{0};
    alignas(64) std::atomic<double> model_load_time_ms_{0.0};

    // History for moving averages
    std::array<double, HISTORY_SIZE> callback_history_;
    std::array<double, HISTORY_SIZE> processing_history_;
    std::atomic<size_t> history_index_{0};
};

} // namespace rtvcc