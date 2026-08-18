#include "InferenceEngine.h"
#include "OnnxInferenceEngine.h"
#include <algorithm>

namespace rtvcc {

size_t TensorInfo::element_count() const {
    size_t count = 1;
    for (auto dim : shape) count *= static_cast<size_t>(dim);
    return count;
}

size_t TensorInfo::byte_size() const {
    size_t bytes = element_count();
    if (type == "float32" || type == "float") bytes *= 4;
    else if (type == "int64" || type == "int32") bytes *= 4;
    else if (type == "int8" || type == "uint8") bytes *= 1;
    return bytes;
}

InferenceConfig::InferenceConfig() = default;
InferenceConfig::~InferenceConfig() = default;

ModelManifest::ModelManifest() = default;
ModelManifest::~ModelManifest() = default;

InferenceResult::InferenceResult() = default;
InferenceResult::~InferenceResult() = default;

IInferenceEngine::~IInferenceEngine() = default;

std::unique_ptr<IInferenceEngine> createInferenceEngine(const std::string& type) {
    if (type == "onnx") {
        return std::make_unique<OnnxInferenceEngine>();
    }
    return nullptr;
}

namespace TensorUtils {

bool validateShape(const std::vector<int64_t>& actual, const std::vector<int64_t>& expected) {
    if (actual.size() != expected.size()) return false;
    for (size_t i = 0; i < actual.size(); ++i) {
        if (expected[i] >= 0 && actual[i] != expected[i]) return false;
    }
    return true;
}

size_t calculateElements(const std::vector<int64_t>& shape) {
    size_t count = 1;
    for (auto dim : shape) count *= static_cast<size_t>(dim);
    return count;
}

void copyTensor(const float* src, float* dst, size_t count) {
    std::copy(src, src + count, dst);
}

void interleave(const float* const* planar, float* interleaved, size_t frames, size_t channels) {
    for (size_t i = 0; i < frames; ++i) {
        for (size_t ch = 0; ch < channels; ++ch) {
            interleaved[i * channels + ch] = planar[ch][i];
        }
    }
}

void deinterleave(const float* interleaved, float* const* planar, size_t frames, size_t channels) {
    for (size_t i = 0; i < frames; ++i) {
        for (size_t ch = 0; ch < channels; ++ch) {
            planar[ch][i] = interleaved[i * channels + ch];
        }
    }
}

} // namespace TensorUtils

} // namespace rtvcc