#include <gtest/gtest.h>
#include "inference/InferenceEngine.h"
#include "vc/OnnxVoiceConverter.h"
#include "metrics/PerformanceMonitor.h"
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace rtvcc;

class PerformanceRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        model_path_ = "models/test_identity.onnx";
        if (!std::filesystem::exists(model_path_)) {
            GTEST_SKIP() << "Test model not found";
        }
    }
    
    std::string model_path_;
    
    struct PerfBaseline {
        double max_inference_ms = 5.0;      // Max acceptable inference time
        double max_processing_ms = 2.0;     // Max acceptable processing time
        double max_callback_ms = 1.0;       // Max acceptable callback time
        double max_rtf = 0.5;               // Max real-time factor
    };
    
    PerfBaseline baseline_;
    
    void saveBaseline(const std::string& path, const PerfBaseline& baseline) {
        nlohmann::json j;
        j["max_inference_ms"] = baseline.max_inference_ms;
        j["max_processing_ms"] = baseline.max_processing_ms;
        j["max_callback_ms"] = baseline.max_callback_ms;
        j["max_rtf"] = baseline.max_rtf;
        std::ofstream file(path);
        file << j.dump(4);
    }
    
    bool loadBaseline(const std::string& path, PerfBaseline& baseline) {
        try {
            std::ifstream file(path);
            if (!file.is_open()) return false;
            nlohmann::json j;
            file >> j;
            baseline.max_inference_ms = j.value("max_inference_ms", 5.0);
            baseline.max_processing_ms = j.value("max_processing_ms", 2.0);
            baseline.max_callback_ms = j.value("max_callback_ms", 1.0);
            baseline.max_rtf = j.value("max_rtf", 0.5);
            return true;
        } catch (...) {
            return false;
        }
    }
};

TEST_F(PerformanceRegressionTest, InferenceEngineBaseline) {
    auto engine = createInferenceEngine("onnx");
    InferenceConfig config;
    config.model_path = model_path_;
    config.execution_provider = "CPU";
    
    EXPECT_TRUE(engine->initialize(config));
    EXPECT_TRUE(engine->warmup());
    
    // Prepare buffers
    auto input_infos = engine->getInputInfos();
    auto output_infos = engine->getOutputInfos();
    
    std::vector<std::vector<float>> input_buffers;
    std::vector<std::vector<float>> output_buffers;
    std::vector<const float*> inputs;
    std::vector<float*> outputs;
    
    for (const auto& info : input_infos) {
        size_t elements = info.element_count();
        std::vector<int64_t> shape = info.shape;
        for (auto& dim : shape) if (dim == -1) dim = 1;
        elements = 1;
        for (int64_t dim : shape) elements *= static_cast<size_t>(dim);
        input_buffers.emplace_back(elements, 0.5f);
        inputs.push_back(input_buffers.back().data());
    }
    
    for (const auto& info : output_infos) {
        size_t elements = info.element_count();
        std::vector<int64_t> shape = info.shape;
        for (auto& dim : shape) if (dim == -1) dim = 1;
        elements = 1;
        for (int64_t dim : shape) elements *= static_cast<size_t>(dim);
        output_buffers.emplace_back(elements, 0.0f);
        outputs.push_back(output_buffers.back().data());
    }
    
    // Run multiple iterations
    constexpr int iterations = 100;
    double total_ms = 0.0;
    double max_ms = 0.0;
    double min_ms = 1000.0;
    
    for (int i = 0; i < iterations; ++i) {
        auto result = engine->run(inputs, outputs);
        EXPECT_TRUE(result.success);
        total_ms += result.inference_time_ms;
        max_ms = std::max(max_ms, result.inference_time_ms);
        min_ms = std::min(min_ms, result.inference_time_ms);
    }
    
    double avg_ms = total_ms / iterations;
    
    std::cout << "Inference: avg=" << avg_ms << "ms, min=" << min_ms << "ms, max=" << max_ms << "ms\n";
    
    // Check against baseline
    EXPECT_LT(avg_ms, baseline_.max_inference_ms) << "Average inference time exceeds baseline";
    EXPECT_LT(max_ms, baseline_.max_inference_ms * 2) << "Max inference time exceeds 2x baseline";
}

TEST_F(PerformanceRegressionTest, OnnxConverterBaseline) {
    auto converter = createVoiceConverter("onnx");
    VoiceConverterConfig config;
    config.model_path = model_path_;
    config.sample_rate = 48000;
    config.chunk_size_ms = 20;
    config.execution_provider = "CPU";
    
    EXPECT_TRUE(converter->initialize(config));
    
    size_t frames = (48000 * 20) / 1000;  // 960 frames at 48kHz for 20ms
    std::vector<float> input(frames, 0.5f);
    std::vector<float> output(frames);
    
    // Warmup
    for (int i = 0; i < 10; ++i) {
        converter->process(input.data(), frames, output.data());
    }
    
    // Benchmark
    constexpr int iterations = 100;
    double total_ms = 0.0;
    double max_ms = 0.0;
    
    PerformanceMonitor monitor;
    
    for (int i = 0; i < iterations; ++i) {
        monitor.onProcessingStart();
        auto result = converter->process(input.data(), frames, output.data());
        monitor.onProcessingEnd();
        
        EXPECT_EQ(result.status, IVoiceConverter::ProcessResult::Status::Success);
        
        auto snap = monitor.getSnapshot();
        if (snap.processing_count > 0) {
            total_ms += snap.processing_avg_ms;
            max_ms = std::max(max_ms, snap.processing_avg_ms);
        }
    }
    
    double avg_ms = total_ms / iterations;
    double chunk_ms = 20.0;  // 20ms chunk
    double rtf = avg_ms / chunk_ms;
    
    std::cout << "Converter: avg=" << avg_ms << "ms, max=" << max_ms << "ms, RTF=" << rtf << "x\n";
    
    // Check against baseline
    EXPECT_LT(avg_ms, baseline_.max_processing_ms) << "Average processing time exceeds baseline";
    EXPECT_LT(rtf, baseline_.max_rtf) << "Real-time factor exceeds baseline";
}

TEST_F(PerformanceRegressionTest, CallbackOverheadBaseline) {
    PerformanceMonitor monitor;
    
    constexpr int iterations = 1000;
    double total_ms = 0.0;
    double max_ms = 0.0;
    
    for (int i = 0; i < iterations; ++i) {
        monitor.onAudioCallbackStart();
        
        // Simulate minimal callback work
        volatile int dummy = 0;
        for (int j = 0; j < 128; ++j) dummy += j;
        
        monitor.onAudioCallbackEnd();
    }
    
    auto snap = monitor.getSnapshot();
    double avg_ms = snap.callback_avg_ms;
    double max_ms_val = snap.callback_max_ms;
    
    std::cout << "Callback: avg=" << avg_ms << "ms, max=" << max_ms_val << "ms\n";
    
    // Callback should be very fast (< 0.5ms for 128 frames at 48kHz)
    EXPECT_LT(avg_ms, baseline_.max_callback_ms);
    EXPECT_LT(max_ms_val, baseline_.max_callback_ms * 3);
}

TEST_F(PerformanceRegressionTest, SaveBaseline) {
    std::string baseline_file = "performance_baseline.json";
    saveBaseline(baseline_file, baseline_);
    EXPECT_TRUE(std::filesystem::exists(baseline_file));
    
    PerfBaseline loaded;
    EXPECT_TRUE(loadBaseline(baseline_file, loaded));
    EXPECT_EQ(loaded.max_inference_ms, baseline_.max_inference_ms);
}

TEST_F(PerformanceRegressionTest, EndToEndLatency) {
    auto converter = createVoiceConverter("onnx");
    VoiceConverterConfig config;
    config.model_path = model_path_;
    config.sample_rate = 48000;
    config.chunk_size_ms = 20;
    config.execution_provider = "CPU";
    
    EXPECT_TRUE(converter->initialize(config));
    
    size_t frames = (48000 * 20) / 1000;
    std::vector<float> input(frames, 0.5f);
    std::vector<float> output(frames);
    
    // Measure end-to-end processing time
    auto start = std::chrono::high_resolution_clock::now();
    auto result = converter->process(input.data(), frames, output.data());
    auto end = std::chrono::high_resolution_clock::now();
    
    EXPECT_EQ(result.status, IVoiceConverter::ProcessResult::Status::Success);
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Total latency = algorithmic + processing + resampling
    // Should be well under chunk duration
    double chunk_ms = 20.0;
    EXPECT_LT(elapsed_ms, chunk_ms * 0.5) << "End-to-end processing too slow";
    
    std::cout << "End-to-end latency: " << elapsed_ms << "ms (chunk: " << chunk_ms << "ms)\n";
}