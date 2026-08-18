#pragma once

#include <atomic>
#include <chrono>
#include <array>
#include <vector>
#include <algorithm>

namespace rtvcc {

// Latency measurement and statistics
class LatencyMetrics {
public:
    LatencyMetrics();
    ~LatencyMetrics() = default;

    // Record a latency measurement (in milliseconds)
    void record(double latency_ms);

    // Get statistics
    struct Stats {
        double mean = 0.0;
        double median = 0.0;
        double min = 0.0;
        double max = 0.0;
        double stddev = 0.0;
        double p50 = 0.0;
        double p90 = 0.0;
        double p95 = 0.0;
        double p99 = 0.0;
        double p999 = 0.0;
        size_t count = 0;
    };

    Stats getStats() const;

    // Get percentile
    double getPercentile(double percentile) const;

    // Reset
    void reset();

    // Get recent samples for plotting
    std::vector<double> getRecentSamples(size_t max_samples = 1000) const;

    // Check if we have enough samples
    bool hasEnoughSamples(size_t min_samples = 100) const;

private:
    static constexpr size_t MAX_SAMPLES = 10000;

    // Welford's online algorithm for mean/variance
    mutable std::atomic<double> mean_{0.0};
    mutable std::atomic<double> m2_{0.0};
    mutable std::atomic<size_t> count_{0};
    mutable std::atomic<double> min_{1000.0};
    mutable std::atomic<double> max_{0.0};

    // Circular buffer for percentiles
    std::array<double, MAX_SAMPLES> samples_;
    std::atomic<size_t> write_index_{0};
    std::atomic<size_t> sample_count_{0};
};

// End-to-end latency measurement using impulse response
class LatencyTester {
public:
    LatencyTester(int sample_rate = 48000);
    ~LatencyTester();

    // Generate test impulse
    void generateImpulse(float* buffer, size_t frames);

    // Process input and output to measure latency
    // Returns latency in milliseconds, or -1 if not detected
    double process(const float* input, const float* output, size_t frames);

    // Reset tester state
    void reset();

    // Get current measurement
    double getLatencyMs() const;

    // Get all measurements
    const std::vector<double>& getMeasurements() const;

private:
    int sample_rate_;
    std::vector<float> impulse_;
    std::vector<float> input_buffer_;
    std::vector<float> output_buffer_;
    size_t buffer_pos_ = 0;
    bool impulse_sent_ = false;
    std::vector<double> measurements_;

    // Cross-correlation for latency detection
    double crossCorrelate(const float* a, const float* b, size_t len, size_t max_lag);
};

} // namespace rtvcc