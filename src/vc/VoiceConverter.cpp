#include "VoiceConverter.h"
#include "OnnxVoiceConverter.h"
#include <memory>

namespace rtvcc {

VoiceConverterConfig::VoiceConverterConfig() = default;
VoiceConverterConfig::~VoiceConverterConfig() = default;

ProcessResult::ProcessResult() = default;
ProcessResult::~ProcessResult() = default;

IVoiceConverter::~IVoiceConverter() = default;

std::unique_ptr<IVoiceConverter> createVoiceConverter(const std::string& type) {
    if (type == "passthrough") {
        return std::make_unique<PassthroughConverter>();
    } else if (type == "dsp") {
        return std::make_unique<DSPVoiceConverter>();
    } else if (type == "onnx" || type == "streaming") {
        return std::make_unique<OnnxVoiceConverter>();
    }
    return nullptr;
}

} // namespace rtvcc