#include "DSPVoiceConverter.h"
#include <algorithm>

namespace rtvcc {

DSPVoiceConverter::DSPVoiceConverter() = default;
DSPVoiceConverter::~DSPVoiceConverter() = default;

bool DSPVoiceConverter::initialize(const VoiceConverterConfig& config) {
    config_ = config;
    sample_rate_ = config.sample_rate > 0 ? config.sample_rate : 48000;
    initialized_ = true;
    last_error_.clear();

    // Initialize DSP components
    highpass_.setCutoff(80.0f, static_cast<float>(sample_rate_));
    input_gain_.setGainDb(config.input_gain_db > 0 ? config.input_gain_db : 0.0f);
    output_gain_.setGainDb(config.output_gain_db > 0 ? config.output_gain_db : 0.0f);
    limiter_.configure(config.limiter_threshold_db > -100 ? config.limiter_threshold_db : -1.0f,
                       config.limiter_release_ms > 0 ? config.limiter_release_ms : 50.0f,
                       1.0f);

    // Allocate temp buffer
    temp_buffer_.resize(4096);

    return true;
}

IVoiceConverter::ProcessResult DSPVoiceConverter::process(const float* input, size_t frames, float* output) {
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

    // Ensure temp buffer is large enough
    if (temp_buffer_.size() < frames) {
        temp_buffer_.resize(frames);
    }

    // Copy input to temp buffer
    std::copy(input, input + frames, temp_buffer_.data());

    // Apply DSP chain
    if (highpass_enabled_) {
        highpass_.process(temp_buffer_.data(), frames);
    }

    input_gain_.process(temp_buffer_.data(), frames);

    if (limiter_enabled_) {
        limiter_.process(temp_buffer_.data(), frames, 1);
    }

    output_gain_.process(temp_buffer_.data(), frames);

    // Apply mix
    if (mix_ >= 1.0f) {
        std::copy(temp_buffer_.data(), temp_buffer_.data() + frames, output);
    } else if (mix_ <= 0.0f) {
        std::fill(output, output + frames, 0.0f);
    } else {
        for (size_t i = 0; i < frames; ++i) {
            output[i] = input[i] * (1.0f - mix_) + temp_buffer_[i] * mix_;
        }
    }

    result.status = ProcessResult::Status::Success;
    result.input_consumed = frames;
    result.output_produced = frames;
    return result;
}

IVoiceConverter::ProcessResult DSPVoiceConverter::processPlanar(const float* const* input, size_t frames, float* const* output) {
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

    if (temp_buffer_.size() < frames) {
        temp_buffer_.resize(frames);
    }

    std::copy(input[0], input[0] + frames, temp_buffer_.data());

    if (highpass_enabled_) {
        highpass_.process(temp_buffer_.data(), frames);
    }

    input_gain_.process(temp_buffer_.data(), frames);

    if (limiter_enabled_) {
        limiter_.process(temp_buffer_.data(), frames, 1);
    }

    output_gain_.process(temp_buffer_.data(), frames);

    if (mix_ >= 1.0f) {
        std::copy(temp_buffer_.data(), temp_buffer_.data() + frames, output[0]);
    } else if (mix_ <= 0.0f) {
        std::fill(output[0], output[0] + frames, 0.0f);
    } else {
        for (size_t i = 0; i < frames; ++i) {
            output[0][i] = input[0][i] * (1.0f - mix_) + temp_buffer_[i] * mix_;
        }
    }

    result.status = ProcessResult::Status::Success;
    result.input_consumed = frames;
    result.output_produced = frames;
    return result;
}

void DSPVoiceConverter::reset() {
    highpass_.reset();
    input_gain_.reset();
    output_gain_.reset();
    limiter_.reset();
}

IVoiceConverter::ProcessResult DSPVoiceConverter::flush(float* output, size_t max_frames) {
    ProcessResult result;
    result.status = ProcessResult::Status::Success;
    result.input_consumed = 0;
    result.output_produced = 0;
    if (output && max_frames > 0) {
        std::fill(output, output + max_frames, 0.0f);
    }
    return result;
}

void DSPVoiceConverter::setInputGainDb(float db) {
    input_gain_.setGainDb(db);
}

void DSPVoiceConverter::setOutputGainDb(float db) {
    output_gain_.setGainDb(db);
}

void DSPVoiceConverter::setHighpassCutoff(float hz) {
    highpass_.setCutoff(hz, static_cast<float>(sample_rate_));
}

void DSPVoiceConverter::setLimiterThresholdDb(float db) {
    limiter_.configure(db, 50.0f, 1.0f);
}

void DSPVoiceConverter::setLimiterReleaseMs(float ms) {
    limiter_.configure(-1.0f, ms, 1.0f);
}

void DSPVoiceConverter::enableHighpass(bool enable) {
    highpass_enabled_ = enable;
}

void DSPVoiceConverter::enableLimiter(bool enable) {
    limiter_enabled_ = enable;
}

} // namespace rtvcc