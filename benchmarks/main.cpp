#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <string>
#include <filesystem>
#include "inference/InferenceEngine.h"
#include "vc/OnnxVoiceConverter.h"
#include "vc/VoiceConverter.h"
#include "metrics/PerformanceMonitor.h"

using namespace rtvcc;

void printUsage() {
    std::cout << "Usage: benchmark [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --model <path>        Path to ONNX model\n";
    std::cout << "  --iterations <N>      Number of inference iterations (default: 1000)\n";
    std::cout << "  --warmup <N>          Number of warmup iterations (default: 10)\n";
    std::cout << "  --chunk-size <ms>     Chunk size in ms (default: 20)\n";
    std::cout << "  --sample-rate <Hz>    Sample rate (default: 48000)\n";
    std::cout << "  --provider <name>     Execution provider: CPU, CUDA, TensorRT, DirectML (default: CPU)\n";
}

struct BenchmarkResult {
    double min_ms = 1000.0;
    double max_ms = 0.0;
    double sum_ms = 0.0;
    size_t count = 0;
    
    void add(double ms) {
        min_ms = std::min(min_ms, ms);
        max_ms = std::max(max_ms, ms);
        sum_ms += ms;
        count++;
    }
    
    double mean() const { return count > 0 ? sum_ms / count : 0.0; }
};

int main(int argc, char* argv[]) {
    std::string model_path = "models/test_identity.onnx";
    int iterations = 1000;
    int warmup = 10;
    int chunk_size_ms = 20;
    int sample_rate = 48000;
    std::string provider = "CPU";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) model_path = argv[++i];
        else if (arg == "--iterations" && i + 1 < argc) iterations = std::stoi(argv[++i]);
        else if (arg == "--warmup" && i + 1 < argc) warmup = std::stoi(argv[++i]);
        else if (arg == "--chunk-size" && i + 1 < argc) chunk_size_ms = std::stoi(argv[++i]);
        else if (arg == "--sample-rate" && i + 1 < argc) sample_rate = std::stoi(argv[++i]);
        else if (arg == "--provider" && i + 1 < argc) provider = argv[++i];
        else if (arg == "--help") { printUsage(); return 0; }
    }
    
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Model not found: " << model_path << "\n";
        std::cerr << "Run python tools/model_conversion/generate_test_models.py to generate test models\n";
        return 1;
    }
    
    std::cout << "=== RTVC Benchmark ===\n";
    std::cout << "Model: " << model_path << "\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Warmup: " << warmup << "\n";
    std::cout << "Chunk size: " << chunk_size_ms << "ms\n";
    std::cout << "Sample rate: " << sample_rate << "Hz\n";
    std::cout << "Provider: " << provider << "\n\n";
    
    // Benchmark InferenceEngine directly
    std::cout << "--- InferenceEngine Benchmark ---\n";
    auto engine = createInferenceEngine("onnx");
    InferenceConfig config;
    config.model_path = model_path;
    config.execution_provider = provider;
    
    if (!engine->initialize(config)) {
        std::cerr << "Failed to initialize engine: " << engine->getLastError() << "\n";
        return 1;
    }
    
    auto manifest = engine->getManifest();
    std::cout << "Model: " << manifest.name << " v" << manifest.version << "\n";
    std::cout << "Sample rate: " << manifest.sample_rate << "Hz\n";
    std::cout << "Inputs: " << engine->getInputInfos().size() << ", Outputs: " << engine->getOutputInfos().size() << "\n";
    
    // Prepare input/output buffers
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
    
    // Warmup
    std::cout << "Warming up...\n";
    for (int i = 0; i < warmup; ++i) {
        engine->run(inputs, outputs);
    }
    engine->reset();
    
    // Benchmark
    std::cout << "Running benchmark...\n";
    BenchmarkResult infer_result;
    BenchmarkResult total_result;
    
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = engine->run(inputs, outputs);
        auto end = std::chrono::high_resolution_clock::now();
        
        if (!result.success) {
            std::cerr << "Inference failed: " << result.error << "\n";
            return 1;
        }
        
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        infer_result.add(result.inference_time_ms);
        total_result.add(total_ms);
        
        if ((i + 1) % 100 == 0) {
            std::cout << "  " << (i + 1) << "/" << iterations << " completed\n";
        }
    }
    
    std::cout << "\n--- Results ---\n";
    std::cout << "Inference only:\n";
    std::cout << "  Min: " << infer_result.min_ms << "ms\n";
    std::cout << "  Max: " << infer_result.max_ms << "ms\n";
    std::cout << "  Mean: " << infer_result.mean() << "ms\n";
    std::cout << "Total (incl. overhead):\n";
    std::cout << "  Min: " << total_result.min_ms << "ms\n";
    std::cout << "  Max: " << total_result.max_ms << "ms\n";
    std::cout << "  Mean: " << total_result.mean() << "ms\n";
    
    double chunk_ms = static_cast<double>(chunk_size_ms);
    double rtf_infer = infer_result.mean() / chunk_ms;
    double rtf_total = total_result.mean() / chunk_ms;
    std::cout << "Real-time factor (inference): " << rtf_infer << "x\n";
    std::cout << "Real-time factor (total): " << rtf_total << "x\n";
    
    // Benchmark OnnxVoiceConverter (full pipeline with resampling)
    std::cout << "\n--- OnnxVoiceConverter Benchmark ---\n";
    auto converter = createVoiceConverter("onnx");
    VoiceConverterConfig vc_config;
    vc_config.model_path = model_path;
    vc_config.sample_rate = sample_rate;
    vc_config.chunk_size_ms = chunk_size_ms;
    vc_config.execution_provider = provider;
    
    if (!converter->initialize(vc_config)) {
        std::cerr << "Failed to initialize converter: " << converter->getLastError() << "\n";
        return 1;
    }
    
    size_t frames = (sample_rate * chunk_size_ms) / 1000;
    std::vector<float> input(frames, 0.5f);
    std::vector<float> output(frames);
    
    // Warmup
    for (int i = 0; i < warmup; ++i) {
        converter->process(input.data(), frames, output.data());
    }
    converter->reset();
    
    // Benchmark
    BenchmarkResult conv_result;
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = converter->process(input.data(), frames, output.data());
        auto end = std::chrono::high_resolution_clock::now();
        
        if (result.status != IVoiceConverter::ProcessResult::Status::Success) {
            std::cerr << "Converter failed: " << result.error << "\n";
            return 1;
        }
        
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        conv_result.add(ms);
    }
    
    std::cout << "Full converter pipeline:\n";
    std::cout << "  Min: " << conv_result.min_ms << "ms\n";
    std::cout << "  Max: " << conv_result.max_ms << "ms\n";
    std::cout << "  Mean: " << conv_result.mean() << "ms\n";
    double rtf_conv = conv_result.mean() / chunk_ms;
    std::cout << "Real-time factor: " << rtf_conv << "x\n";
    
    // Performance assessment
    std::cout << "\n--- Performance Assessment ---\n";
    if (rtf_conv < 0.25) {
        std::cout << "EXCELLENT: Well within real-time budget (< 25%)\n";
    } else if (rtf_conv < 0.5) {
        std::cout << "GOOD: Within real-time budget (< 50%)\n";
    } else if (rtf_conv < 1.0) {
        std::cout << "MARGINAL: Close to real-time limit (< 100%)\n";
    } else {
        std::cout << "POOR: Exceeds real-time budget (> 100%)\n";
    }
    
    double estimated_latency = conv_result.mean() + chunk_ms + 2.67; // chunk + processing + 128 frame callback
    std::cout << "Estimated end-to-end latency: " << estimated_latency << "ms\n";
    
    return 0;
}