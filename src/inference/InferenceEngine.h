#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

namespace rtvcc {

// Tensor descriptor for model I/O
struct TensorInfo {
    std::string name;
    std::vector<int64_t> shape;  // e.g., {1, 1, 16000} for [batch, channels, samples]
    std::string type;            // "float32", "int64", etc.
    size_t element_count() const;
    size_t byte_size() const;
};

// Inference engine configuration
struct InferenceConfig {
    std::string model_path;
    std::string execution_provider = "CPU";  // "CPU", "CUDA", "TensorRT", "DirectML"
    int intra_op_threads = 1;
    int inter_op_threads = 1;
    bool enable_profiling = false;
    bool graph_optimization = true;
    std::string cache_dir;
    
    // Optimization options
    enum class Precision {
        FP32,
        FP16,
        INT8
    };
    Precision precision = Precision::FP32;
    bool enable_quantization = false;
    std::string quantization_calibration_data;  // Path to calibration data for INT8
    
    // TensorRT specific
    size_t tensorrt_workspace_size = 1 << 30;  // 1GB default
    int tensorrt_min_timing_iterations = 2;
    int tensorrt_avg_timing_iterations = 1;
    
    // CPU specific
    bool enable_cpu_mem_arena = true;
    bool enable_cpu_mem_pattern = true;
    OrtExecutionMode execution_mode = OrtExecutionMode::ORT_SEQUENTIAL;
};

// Model metadata/manifest
struct ModelManifest {
    std::string name;
    std::string version;
    std::string format = "onnx";
    int sample_rate = 16000;
    int channels = 1;
    std::vector<int64_t> input_shape;
    std::vector<int64_t> output_shape;
    bool streaming = true;
    int lookahead_ms = 0;
    std::string license;
    std::string sha256;
    std::string runtime = "onnxruntime";
    std::string description;
};

// Inference result
struct InferenceResult {
    bool success = false;
    std::string error;
    double inference_time_ms = 0.0;
    double preprocess_time_ms = 0.0;
    double postprocess_time_ms = 0.0;
};

// Abstract inference engine interface
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;

    // Load and initialize model
    virtual bool initialize(const InferenceConfig& config) = 0;

    // Get model metadata
    virtual ModelManifest getManifest() const = 0;

    // Get input tensor info
    virtual std::vector<TensorInfo> getInputInfos() const = 0;

    // Get output tensor info
    virtual std::vector<TensorInfo> getOutputInfos() const = 0;

    // Run inference
    // inputs: map of tensor name -> float data
    // outputs: map of tensor name -> output buffer (pre-allocated)
    virtual InferenceResult run(const std::vector<const float*>& inputs,
                                std::vector<float*>& outputs) = 0;

    // Run inference with named tensors
    virtual InferenceResult runNamed(const std::vector<std::pair<std::string, const float*>>& inputs,
                                     std::vector<std::pair<std::string, float*>>& outputs) = 0;

    // Warm up the model (run dummy inference)
    virtual bool warmup() = 0;

    // Reset streaming state (for stateful models)
    virtual void reset() = 0;

    // Check if ready
    virtual bool isReady() const = 0;

    // Get last error
    virtual std::string getLastError() const = 0;
};

// Factory function
std::unique_ptr<IInferenceEngine> createInferenceEngine(const std::string& type = "onnx");

// Tensor utilities
namespace TensorUtils {
    // Validate tensor shape matches expected
    bool validateShape(const std::vector<int64_t>& actual, const std::vector<int64_t>& expected);

    // Calculate number of elements from shape
    size_t calculateElements(const std::vector<int64_t>& shape);

    // Copy data with layout conversion if needed
    void copyTensor(const float* src, float* dst, size_t count);

    // Interleave planar to interleaved
    void interleave(const float* const* planar, float* interleaved, size_t frames, size_t channels);

    // Deinterleave interleaved to planar
    void deinterleave(const float* interleaved, float* const* planar, size_t frames, size_t channels);
}

} // namespace rtvcc