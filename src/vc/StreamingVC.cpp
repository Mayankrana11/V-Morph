#include "VoiceConverter.h"

namespace rtvcc {

// StreamingVC - placeholder for streaming voice conversion model integration
class StreamingVC::Impl {
public:
    Impl() = default;
    ~Impl() = default;
};

StreamingVC::StreamingVC() : pimpl_(std::make_unique<Impl>()) {}
StreamingVC::~StreamingVC() = default;

bool StreamingVC::initialize(const VoiceConverterConfig& config) { return false; }
void StreamingVC::reset() {}
IVoiceConverter::ProcessResult StreamingVC::process(const float*, size_t, float*) {
    return {IVoiceConverter::ProcessResult::Status::NotInitialized, 0, 0, "Not implemented"};
}
IVoiceConverter::ProcessResult StreamingVC::processPlanar(const float* const*, size_t, float* const*) {
    return {IVoiceConverter::ProcessResult::Status::NotInitialized, 0, 0, "Not implemented"};
}
IVoiceConverter::ProcessResult StreamingVC::flush(float*, size_t) {
    return {IVoiceConverter::ProcessResult::Status::Success, 0, 0};
}
bool StreamingVC::isReady() const { return false; }
std::string StreamingVC::getLastError() const { return "Not implemented"; }
bool StreamingVC::setTargetVoice(const std::string&) { return false; }
bool StreamingVC::setPitchShift(float) { return false; }
bool StreamingVC::setFormantShift(float) { return false; }
bool StreamingVC::setMix(float) { return false; }
VoiceConverterConfig StreamingVC::getConfig() const { return {}; }
std::string StreamingVC::getName() const { return "StreamingVC"; }
std::string StreamingVC::getVersion() const { return "0.0.0"; }
int StreamingVC::getInputSampleRate() const { return 16000; }
int StreamingVC::getOutputSampleRate() const { return 16000; }
size_t StreamingVC::getAlgorithmicLatencySamples() const { return 0; }
double StreamingVC::getAlgorithmicLatencyMs() const { return 0.0; }

} // namespace rtvcc