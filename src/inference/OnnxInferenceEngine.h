#pragma once

#include "InferenceEngine.h"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace rtvcc {

// ONNX Runtime inference engine implementation
class OnnxInferenceEngine : public IInferenceEngine {
public:
    OnnxInferenceEngine();
    ~OnnxInferenceEngine() override;

    bool initialize(const InferenceConfig& config) override;

    ModelManifest getManifest() const override;
    std::vector<TensorInfo> getInputInfos() const override;
    std::vector<TensorInfo> getOutputInfos() const override;

    InferenceResult run(const std::vector<const float*>& inputs,
                        std::vector<float*>& outputs) override;
    InferenceResult runNamed(const std::vector<std::pair<std::string, const float*>>& inputs,
                             std::vector<std::pair<std::string, float*>>& outputs) override;

    bool warmup() override;
    bool warmup(int iterations);
    void reset() override;

    // Export optimized model (for TensorRT engine cache)
    bool exportOptimizedModel(const std::string& output_path) const;

    bool isReady() const override;
    std::string getLastError() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rtvcc