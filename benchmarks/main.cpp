#include "vc/PassthroughConverter.h"
#include "vc/DSPVoiceConverter.h"
#include "dsp/Resampler.h"
#include "metrics/PerformanceMonitor.h"
#include "metrics/LatencyMetrics.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <nlohmann/json.hpp>

using namespace rtvcc;

class BenchmarkRunner {
public:
    struct Results {
        std::string name;
        double mean_ms = 0.0;
        double median_ms = 0.0;
        double min_ms = 0.0;
        double max_ms = 0.0;
        double p95_ms = 0.0;
        double p99_ms = 0.0;
        double throughput_mpps = 0.0;  // Million samples per second
        size_t iterations = 0;
    };

    template <typename Func>
    Results runBenchmark(const std::string& name, Func&& func, size_t iterations = 10000, size_t warmup = 1000) {
        LatencyMetrics metrics;

        // Warmup
        for (size_t i = 0; i < warmup; ++i) {
            func();
        }

        // Benchmark
        for (size_t i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            func();
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            metrics.record(ms);
        }

        auto stats = metrics.getStats();
        Results results;
        results.name = name;
        results.mean_ms = stats.mean;
        results.median_ms = stats.median;
        results.min_ms = stats.min;
        results.max_ms = stats.max;
        results.p95_ms = stats.p95;
        results.p99_ms = stats.p99;
        results.iterations = iterations;

        return results;
    }

    void printResults(const Results& r) {
        std::cout << "\n=== " << r.name << " ===" << std::endl;
        std::cout << "  Iterations: " << r.iterations << std::endl;
        std::cout << "  Mean:   " << r.mean_ms << " ms" << std::endl;
        std::cout << "  Median: " << r.median_ms << " ms" << std::endl;
        std::cout << "  Min:    " << r.min_ms << " ms" << std::endl;
        std::cout << "  Max:    " << r.max_ms << " ms" << std::endl;
        std::cout << "  P95:    " << r.p95_ms << " ms" << std::endl;
        std::cout << "  P99:    " << r.p99_ms << " ms" << std::endl;
    }

    void saveResults(const std::vector<Results>& all_results, const std::string& filename) {
        nlohmann::json j;
        for (const auto& r : all_results) {
            j["benchmarks"].push_back({
                {"name", r.name},
                {"mean_ms", r.mean_ms},
                {"median_ms", r.median_ms},
                {"min_ms", r.min_ms},
                {"max_ms", r.max_ms},
                {"p95_ms", r.p95_ms},
                {"p99_ms", r.p99_ms},
                {"iterations", r.iterations}
            });
        }
        j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

        std::ofstream file(filename);
        file << j.dump(4);
        std::cout << "\nResults saved to " << filename << std::endl;
    }
};

void benchmarkPassthrough(BenchmarkRunner& runner) {
    PassthroughConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    converter.initialize(config);

    const size_t frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t frames : frame_sizes) {
        std::vector<float> input(frames, 0.5f);
        std::vector<float> output(frames);

        auto results = runner.runBenchmark("Passthrough_" + std::to_string(frames) + "frames", [&]() {
            converter.process(input.data(), frames, output.data());
        });

        runner.printResults(results);
    }
}

void benchmarkDSP(BenchmarkRunner& runner) {
    DSPVoiceConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    config.enable_highpass = true;
    config.enable_limiter = true;
    converter.initialize(config);

    const size_t frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t frames : frame_sizes) {
        std::vector<float> input(frames, 0.5f);
        std::vector<float> output(frames);

        auto results = runner.runBenchmark("DSP_" + std::to_string(frames) + "frames", [&]() {
            converter.process(input.data(), frames, output.data());
        });

        runner.printResults(results);
    }
}

void benchmarkResampler(BenchmarkRunner& runner) {
    const std::pair<int, int> rates[] = {
        {48000, 16000}, {48000, 24000}, {16000, 48000}, {24000, 48000}
    };

    for (auto [in_rate, out_rate] : rates) {
        Resampler resampler;
        resampler.configure(in_rate, out_rate, 1);

        double ratio = static_cast<double>(out_rate) / in_rate;
        size_t in_frames = 480;
        size_t out_frames = static_cast<size_t>(in_frames * ratio) + 10;

        std::vector<float> input(in_frames);
        std::vector<float> output(out_frames);

        // Fill with test signal
        for (size_t i = 0; i < input.size(); ++i) {
            input[i] = std::sin(2.0 * 3.14159 * 1000.0 * i / in_rate);
        }

        auto results = runner.runBenchmark("Resampler_" + std::to_string(in_rate) + "to" + std::to_string(out_rate), [&]() {
            resampler.process(input.data(), in_frames, output.data(), out_frames);
        });

        runner.printResults(results);
    }
}

void benchmarkRingBuffer(BenchmarkRunner& runner) {
    AudioRingBuffer buffer(65536);
    std::vector<AudioFrame> frames(1000);
    for (auto& f : frames) f.data[0] = 0.5f;

    // Producer benchmark
    auto prod_results = runner.runBenchmark("RingBuffer_Producer", [&]() {
        buffer.push(frames.data(), frames.size());
    });
    runner.printResults(prod_results);

    // Consumer benchmark (need data first)
    buffer.push(frames.data(), frames.size());
    std::vector<AudioFrame> out_frames(1000);

    auto cons_results = runner.runBenchmark("RingBuffer_Consumer", [&]() {
        buffer.pop(out_frames.data(), out_frames.size());
    });
    runner.printResults(cons_results);
}

int main(int argc, char* argv[]) {
    std::cout << "RT Voice Changer Benchmark" << std::endl;
    std::cout << "==========================" << std::endl;

    BenchmarkRunner runner;
    std::vector<BenchmarkRunner::Results> all_results;

    // Run benchmarks
    benchmarkPassthrough(runner);
    benchmarkDSP(runner);
    benchmarkResampler(runner);
    benchmarkRingBuffer(runner);

    // Save combined results
    runner.saveResults(all_results, "benchmark_results.json");

    std::cout << "\nAll benchmarks completed!" << std::endl;
    return 0;
}