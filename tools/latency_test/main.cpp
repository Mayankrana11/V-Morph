#include "metrics/LatencyMetrics.h"
#include "audio/AudioEngine.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cmath>
#include <nlohmann/json.hpp>

using namespace rtvcc;

class LatencyTestApp : public IAudioCallback {
public:
    LatencyTestApp(int sample_rate, int buffer_frames, int duration_sec)
        : sample_rate_(sample_rate)
        , buffer_frames_(buffer_frames)
        , duration_sec_(duration_sec)
        , tester_(sample_rate)
        , running_(false) {}

    bool run() {
        // Create audio engine
        AudioStreamConfig config;
        config.format.sample_rate = sample_rate_;
        config.format.channels = 1;
        config.format.bits_per_sample = 32;
        config.buffer_frames = buffer_frames_;
        config.exclusive_mode = true;
        config.low_latency = true;

        audio_engine_ = IAudioEngine::create(AudioBackend::WASAPI);
        if (!audio_engine_->initialize(config, this)) {
            std::cerr << "Failed to initialize audio engine: " << audio_engine_->getLastError() << std::endl;
            return false;
        }

        if (!audio_engine_->start()) {
            std::cerr << "Failed to start audio engine" << std::endl;
            return false;
        }

        running_ = true;
        std::cout << "Latency test running for " << duration_sec_ << " seconds..." << std::endl;
        std::cout << "Make sure microphone and output are connected (loopback)" << std::endl;

        // Wait for test duration
        std::this_thread::sleep_for(std::chrono::seconds(duration_sec_));

        running_ = false;
        audio_engine_->stop();

        // Print results
        printResults();

        // Save to JSON
        saveResults("latency_results.json");

        return true;
    }

private:
    void onAudioProcess(const float* input, float* output, size_t frames, size_t channels) override {
        if (!running_) {
            std::fill(output, output + frames, 0.0f);
            return;
        }

        // Generate impulse periodically
        static size_t frame_counter = 0;
        static const size_t impulse_interval = sample_rate_ * 2;  // Every 2 seconds

        if (frame_counter % impulse_interval == 0) {
            tester_.generateImpulse(const_cast<float*>(input), frames);
        }
        frame_counter += frames;

        // Measure latency
        double latency = tester_.process(input, output, frames);
        if (latency > 0) {
            latencies_.push_back(latency);
            std::cout << "Latency: " << latency << " ms" << std::endl;
        }

        // Pass through for monitoring
        std::copy(input, input + frames, output);
    }

    void onStreamStart() override {
        std::cout << "Stream started" << std::endl;
    }

    void onStreamStop() override {
        std::cout << "Stream stopped" << std::endl;
    }

    void onError(const std::string& error) override {
        std::cerr << "Audio error: " << error << std::endl;
    }

    void printResults() {
        if (latencies_.empty()) {
            std::cout << "No latency measurements captured." << std::endl;
            return;
        }

        LatencyMetrics metrics;
        for (double l : latencies_) {
            metrics.record(l);
        }

        auto stats = metrics.getStats();

        std::cout << "\n=== Latency Test Results ===" << std::endl;
        std::cout << "Sample Rate: " << sample_rate_ << " Hz" << std::endl;
        std::cout << "Buffer Frames: " << buffer_frames_ << " (" << (buffer_frames_ * 1000.0 / sample_rate_) << " ms)" << std::endl;
        std::cout << "Measurements: " << stats.count << std::endl;
        std::cout << "Mean: " << stats.mean << " ms" << std::endl;
        std::cout << "Median: " << stats.median << " ms" << std::endl;
        std::cout << "Min: " << stats.min << " ms" << std::endl;
        std::cout << "Max: " << stats.max << " ms" << std::endl;
        std::cout << "StdDev: " << stats.stddev << " ms" << std::endl;
        std::cout << "P50: " << stats.p50 << " ms" << std::endl;
        std::cout << "P90: " << stats.p90 << " ms" << std::endl;
        std::cout << "P95: " << stats.p95 << " ms" << std::endl;
        std::cout << "P99: " << stats.p99 << " ms" << std::endl;
        std::cout << "P99.9: " << stats.p999 << " ms" << std::endl;
    }

    void saveResults(const std::string& filename) {
        nlohmann::json j;
        j["sample_rate"] = sample_rate_;
        j["buffer_frames"] = buffer_frames_;
        j["duration_sec"] = duration_sec_;
        j["measurements"] = latencies_;

        LatencyMetrics metrics;
        for (double l : latencies_) metrics.record(l);
        auto stats = metrics.getStats();

        j["stats"]["count"] = stats.count;
        j["stats"]["mean"] = stats.mean;
        j["stats"]["median"] = stats.median;
        j["stats"]["min"] = stats.min;
        j["stats"]["max"] = stats.max;
        j["stats"]["stddev"] = stats.stddev;
        j["stats"]["p50"] = stats.p50;
        j["stats"]["p90"] = stats.p90;
        j["stats"]["p95"] = stats.p95;
        j["stats"]["p99"] = stats.p99;
        j["stats"]["p999"] = stats.p999;

        std::ofstream file(filename);
        file << j.dump(4);
        std::cout << "Results saved to " << filename << std::endl;
    }

    int sample_rate_;
    int buffer_frames_;
    int duration_sec_;
    LatencyTester tester_;
    std::unique_ptr<IAudioEngine> audio_engine_;
    std::vector<double> latencies_;
    std::atomic<bool> running_;
};

int main(int argc, char* argv[]) {
    int sample_rate = 48000;
    int buffer_frames = 128;
    int duration = 30;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sample-rate" && i + 1 < argc) sample_rate = std::stoi(argv[++i]);
        else if (arg == "--buffer-frames" && i + 1 < argc) buffer_frames = std::stoi(argv[++i]);
        else if (arg == "--duration" && i + 1 < argc) duration = std::stoi(argv[++i]);
        else if (arg == "--help") {
            std::cout << "Usage: latency_test [--sample-rate N] [--buffer-frames N] [--duration N]" << std::endl;
            return 0;
        }
    }

    LatencyTestApp app(sample_rate, buffer_frames, duration);
    return app.run() ? 0 : 1;
}