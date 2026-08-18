#include "PassthroughConverter.h"
#include <cstring>
#include <algorithm>

namespace rtvcc {

PassthroughConverter::PassthroughConverter() = default;
PassthroughConverter::~PassthroughConverter() = default;

bool PassthroughConverter::initialize(const VoiceConverterConfig& config) {
    config_ = config;
    input_sample_rate_ = config.sample_rate > 0 ? config.sample_rate : 48000;
    output_sample_rate_ = config.sample_rate > 0 ? config.sample_rate : 48000;
    initialized_ = true;
    last_error_.clear();
    return true;
}

IVoiceConverter::ProcessResult PassthroughConverter::process(const float* input, size_t frames, float* output) {
    ProcessResult result;
    if (!initialized_) {
        result.status = ProcessResult::Status::NotInitialized;
        result.error = "Converter not initialized";
        return result;
    }

    if (!input || !output) {
        result.status = ProcessResult::Status::Error;
        result.error = "Null pointer";
        return result;
    }

    // Simple copy with mix control
    if (mix_ >= 1.0f) {
        std::copy(input, input + frames, output);
    } else if (mix_ <= 0.0f) {
        std::fill(output, output + frames, 0.0f);
    } else {
        for (size_t i = 0; i < frames; ++i) {
            output[i] = input[i] * mix_;
        }
    }

    result.status = ProcessResult::Status::Success;
    result.input_consumed = frames;
    result.output_produced = frames;
    return result;
}

IVoiceConverter::ProcessResult PassthroughConverter::processPlanar(const float* const* input, size_t frames, float* const* output) {
    ProcessResult result;
    if (!initialized_) {
        result.status = ProcessResult::Status::NotInitialized;
        result.error = "Converter not initialized";
        return result;
    }

    if (!input || !output || !input[0] || !output[0]) {
        result.status = ProcessResult::Status::Error;
        result.error = "Null pointer";
        return result;
    }

    if (mix_ >= 1.0f) {
        std::copy(input[0], input[0] + frames, output[0]);
    } else if (mix_ <= 0.0f) {
        std::fill(output[0], output[0] + frames, 0.0f);
    } else {
        for (size_t i = 0; i < frames; ++i) {
            output[0][i] = input[0][i] * mix_;
        }
    }

    result.status = ProcessResult::Status::Success;
    result.input_consumed = frames;
    result.output_produced = frames;
    return result;
}

void PassthroughConverter::reset() {
    // Nothing to reset for passthrough
}

IVoiceConverter::ProcessResult PassthroughConverter::flush(float* output, size_t max_frames) {
    ProcessResult result;
    result.status = ProcessResult::Status::Success;
    result.input_consumed = 0;
    result.output_produced = 0;
    if (output && max_frames > 0) {
        std::fill(output, output + max_frames, 0.0f);
    }
    return result;
}

} // namespace rtvcc