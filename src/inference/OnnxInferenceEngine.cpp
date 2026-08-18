#include "InferenceEngine.h"
#include <memory>

namespace rtvcc {

class OnnxInferenceEngine : public IInferenceEngine {
public:
    OnnxInferenceEngine() = default;
    ~OnnxInferenceEngine() override = default;

    bool initialize(const InferenceConfig& config) override {
        // TODO: Implement ONNX Runtime initialization
        last_error_ = "ONNX Runtime not implemented yet";
        return false;
    }

    ModelManifest getManifest() const override {
        return manifest_;
    }

    std::vector<TensorInfo> getInputInfos() const override {
        return input_infos_;
    }

    std::vector<TensorInfo> getOutputInfos() const override {
        return output_infos_;
    }

    InferenceResult run(const std::vector<const float*>& inputs,
                        std::vector<float*>& outputs) override {
        InferenceResult result;
        result.success = false;
        result.error = "Not implemented";
        return result;
    }

    InferenceResult runNamed(const std::vector<std::pair<std::string, const float*>>& inputs,
                             std::vector<std::pair<std::string, float*>>& outputs) override {
        InferenceResult result;
        result.success = false;
        result.error = "Not implemented";
        return result;
    }

    bool warmup() override {
        return false;
    }

    void reset() override {}

    bool isReady() const override {
        return false;
    }

    std::string getLastError() const override {
        return last_error_;
    }

private:
    ModelManifest manifest_;
    std::vector<TensorInfo> input_infos_;
    std::vector<TensorInfo> output_infos_;
    std::string last_error_;
};

} // namespace rtvcc