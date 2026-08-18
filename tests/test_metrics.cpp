#include <gtest/gtest.h>
#include "metrics/LatencyMetrics.h"
#include "metrics/PerformanceMonitor.h"

using namespace rtvcc;

TEST(MetricsTest, LatencyMetricsBasic) {
    LatencyMetrics metrics;
    metrics.record(10.0);
    metrics.record(12.0);
    metrics.record(8.0);

    auto stats = metrics.getStats();
    EXPECT_EQ(stats.count, 3);
    EXPECT_NEAR(stats.mean, 10.0, 0.1);
    EXPECT_NEAR(stats.min, 8.0, 0.1);
    EXPECT_NEAR(stats.max, 12.0, 0.1);
}

TEST(MetricsTest, LatencyMetricsPercentiles) {
    LatencyMetrics metrics;
    for (int i = 1; i <= 100; ++i) {
        metrics.record(static_cast<double>(i));
    }

    auto stats = metrics.getStats();
    EXPECT_NEAR(stats.p50, 50.0, 1.0);
    EXPECT_NEAR(stats.p90, 90.0, 1.0);
    EXPECT_NEAR(stats.p95, 95.0, 1.0);
    EXPECT_NEAR(stats.p99, 99.0, 1.0);
}

TEST(MetricsTest, LatencyMetricsReset) {
    LatencyMetrics metrics;
    metrics.record(10.0);
    metrics.record(20.0);
    metrics.reset();

    auto stats = metrics.getStats();
    EXPECT_EQ(stats.count, 0);
    EXPECT_EQ(stats.mean, 0.0);
}

TEST(MetricsTest, LatencyMetricsRecentSamples) {
    LatencyMetrics metrics;
    for (int i = 0; i < 100; ++i) {
        metrics.record(static_cast<double>(i));
    }

    auto samples = metrics.getRecentSamples(10);
    EXPECT_EQ(samples.size(), 10);
    // Should be last 10 samples (90-99)
    for (size_t i = 0; i < samples.size(); ++i) {
        EXPECT_NEAR(samples[i], 90.0 + i, 0.1);
    }
}

TEST(MetricsTest, PerformanceMonitorCallbackTiming) {
    PerformanceMonitor monitor;
    monitor.onAudioCallbackStart();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    monitor.onAudioCallbackEnd();

    auto snap = monitor.getSnapshot();
    EXPECT_GT(snap.callback_avg_ms, 0.05);
    EXPECT_LT(snap.callback_avg_ms, 1.0);
    EXPECT_EQ(snap.callback_count, 1);
}

TEST(MetricsTest, PerformanceMonitorUnderrunOverrun) {
    PerformanceMonitor monitor;
    monitor.onAudioUnderrun();
    monitor.onAudioUnderrun();
    monitor.onAudioOverrun();

    auto snap = monitor.getSnapshot();
    EXPECT_EQ(snap.underruns, 2);
    EXPECT_EQ(snap.overruns, 1);
}

TEST(MetricsTest, PerformanceMonitorProcessingTiming) {
    PerformanceMonitor monitor;
    monitor.onProcessingStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    monitor.onProcessingEnd();

    auto snap = monitor.getSnapshot();
    EXPECT_GT(snap.processing_avg_ms, 0.5);
    EXPECT_LT(snap.processing_avg_ms, 5.0);
    EXPECT_EQ(snap.processing_count, 1);
}

TEST(MetricsTest, PerformanceMonitorInferenceTiming) {
    PerformanceMonitor monitor;
    monitor.onInferenceStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    monitor.onInferenceEnd();

    auto snap = monitor.getSnapshot();
    EXPECT_GT(snap.inference_avg_ms, 1.0);
    EXPECT_LT(snap.inference_avg_ms, 10.0);
    EXPECT_EQ(snap.inference_count, 1);
}

TEST(MetricsTest, PerformanceMonitorQueueDepth) {
    PerformanceMonitor monitor;
    monitor.setInputQueueDepth(100);
    monitor.setOutputQueueDepth(50);

    auto snap = monitor.getSnapshot();
    EXPECT_EQ(snap.input_queue_depth, 100);
    EXPECT_EQ(snap.output_queue_depth, 50);
}

TEST(MetricsTest, PerformanceMonitorReset) {
    PerformanceMonitor monitor;
    monitor.onAudioCallbackStart();
    monitor.onAudioCallbackEnd();
    monitor.onAudioUnderrun();
    monitor.reset();

    auto snap = monitor.getSnapshot();
    EXPECT_EQ(snap.callback_count, 0);
    EXPECT_EQ(snap.underruns, 0);
}