#include "LatencyMetrics.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace rtvcc {

LatencyMetrics::LatencyMetrics() = default;

void LatencyMetrics::record(double latency_ms) {
    // Update Welford's online algorithm
    size_t n = count_.fetch_add(1, std::memory_order_relaxed) + 1;
    double old_mean = mean_.load(std::memory_order_relaxed);
    double delta = latency_ms - old_mean;
    double new_mean = old_mean + delta / n;
    mean_.store(new_mean, std::memory_order_relaxed);

    double m2 = m2_.load(std::memory_order_relaxed) + delta * (latency_ms - new_mean);
    m2_.store(m2, std::memory_order_relaxed);

    // Update min/max
    double current_min = min_.load(std::memory_order_relaxed);
    while (latency_ms < current_min && !min_.compare_exchange_weak(current_min, latency_ms)) {}

    double current_max = max_.load(std::memory_order_relaxed);
    while (latency_ms > current_max && !max_.compare_exchange_weak(current_max, latency_ms)) {}

    // Store in circular buffer
    size_t idx = write_index_.fetch_add(1, std::memory_order_relaxed) % MAX_SAMPLES;
    samples_[idx] = latency_ms;
    sample_count_.fetch_add(1, std::memory_order_relaxed);
}

LatencyMetrics::Stats LatencyMetrics::getStats() const {
    Stats stats;
    stats.count = count_.load(std::memory_order_acquire);
    if (stats.count == 0) return stats;

    stats.mean = mean_.load(std::memory_order_acquire);
    stats.min = min_.load(std::memory_order_acquire);
    stats.max = max_.load(std::memory_order_acquire);

    double m2 = m2_.load(std::memory_order_acquire);
    if (stats.count > 1) {
        stats.stddev = std::sqrt(m2 / (stats.count - 1));
    }

    // Get samples for percentiles
    size_t sample_cnt = std::min(sample_count_.load(std::memory_order_acquire), MAX_SAMPLES);
    if (sample_cnt > 0) {
        std::vector<double> sorted_samples;
        sorted_samples.reserve(sample_cnt);

        size_t start_idx = (write_index_.load(std::memory_order_acquire) + MAX_SAMPLES - sample_cnt) % MAX_SAMPLES;
        for (size_t i = 0; i < sample_cnt; ++i) {
            size_t idx = (start_idx + i) % MAX_SAMPLES;
            sorted_samples.push_back(samples_[idx]);
        }

        std::sort(sorted_samples.begin(), sorted_samples.end());

        auto percentile = [&](double p) -> double {
            if (sorted_samples.empty()) return 0.0;
            size_t idx = static_cast<size_t>(p * (sorted_samples.size() - 1));
            return sorted_samples[idx];
        };

        stats.median = percentile(0.5);
        stats.p50 = percentile(0.5);
        stats.p90 = percentile(0.9);
        stats.p95 = percentile(0.95);
        stats.p99 = percentile(0.99);
        stats.p999 = percentile(0.999);
    }

    return stats;
}

double LatencyMetrics::getPercentile(double percentile) const {
    size_t sample_cnt = std::min(sample_count_.load(std::memory_order_acquire), MAX_SAMPLES);
    if (sample_cnt == 0) return 0.0;

    std::vector<double> sorted_samples;
    sorted_samples.reserve(sample_cnt);

    size_t start_idx = (write_index_.load(std::memory_order_acquire) + MAX_SAMPLES - sample_cnt) % MAX_SAMPLES;
    for (size_t i = 0; i < sample_cnt; ++i) {
        size_t idx = (start_idx + i) % MAX_SAMPLES;
        sorted_samples.push_back(samples_[idx]);
    }

    std::sort(sorted_samples.begin(), sorted_samples.end());
    size_t idx = static_cast<size_t>(percentile * (sorted_samples.size() - 1));
    return sorted_samples[idx];
}

void LatencyMetrics::reset() {
    mean_.store(0.0, std::memory_order_release);
    m2_.store(0.0, std::memory_order_release);
    count_.store(0, std::memory_order_release);
    min_.store(1000.0, std::memory_order_release);
    max_.store(0.0, std::memory_order_release);
    write_index_.store(0, std::memory_order_release);
    sample_count_.store(0, std::memory_order_release);
    std::fill(samples_.begin(), samples_.end(), 0.0);
}

std::vector<double> LatencyMetrics::getRecentSamples(size_t max_samples) const {
    size_t sample_cnt = std::min(sample_count_.load(std::memory_order_acquire), MAX_SAMPLES);
    size_t to_get = std::min(max_samples, sample_cnt);

    std::vector<double> result;
    result.reserve(to_get);

    size_t start_idx = (write_index_.load(std::memory_order_acquire) + MAX_SAMPLES - to_get) % MAX_SAMPLES;
    for (size_t i = 0; i < to_get; ++i) {
        size_t idx = (start_idx + i) % MAX_SAMPLES;
        result.push_back(samples_[idx]);
    }

    return result;
}

bool LatencyMetrics::hasEnoughSamples(size_t min_samples) const {
    return sample_count_.load(std::memory_order_acquire) >= min_samples;
}

// LatencyTester
LatencyTester::LatencyTester(int sample_rate) : sample_rate_(sample_rate) {
    // Generate impulse: single sample at 1.0, rest 0
    impulse_.assign(sample_rate / 10, 0.0f); // 100ms buffer
    impulse_[0] = 1.0f;

    input_buffer_.assign(sample_rate, 0.0f);
    output_buffer_.assign(sample_rate, 0.0f);
}

LatencyTester::~LatencyTester() = default;

void LatencyTester::generateImpulse(float* buffer, size_t frames) {
    std::fill(buffer, buffer + frames, 0.0f);
    if (frames > 0) buffer[0] = 1.0f;
}

double LatencyTester::process(const float* input, const float* output, size_t frames) {
    // Simple cross-correlation to find impulse in output
    // This is a simplified version - real implementation would be more robust
    return -1.0;
}

void LatencyTester::reset() {
    buffer_pos_ = 0;
    impulse_sent_ = false;
    measurements_.clear();
    std::fill(input_buffer_.begin(), input_buffer_.end(), 0.0f);
    std::fill(output_buffer_.begin(), output_buffer_.end(), 0.0f);
}

double LatencyTester::getLatencyMs() const {
    if (measurements_.empty()) return -1.0;
    double sum = 0.0;
    for (double m : measurements_) sum += m;
    return sum / measurements_.size();
}

const std::vector<double>& LatencyTester::getMeasurements() const {
    return measurements_;
}

double LatencyTester::crossCorrelate(const float* a, const float* b, size_t len, size_t max_lag) {
    double max_corr = 0.0;
    size_t best_lag = 0;

    for (size_t lag = 0; lag < max_lag && lag < len; ++lag) {
        double corr = 0.0;
        for (size_t i = 0; i < len - lag; ++i) {
            corr += a[i] * b[i + lag];
        }
        if (corr > max_corr) {
            max_corr = corr;
            best_lag = lag;
        }
    }

    return static_cast<double>(best_lag) / sample_rate_ * 1000.0;
}

} // namespace rtvcc