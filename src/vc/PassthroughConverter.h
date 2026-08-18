#pragma once

#include "VoiceConverter.h"

namespace rtvcc {

// Passthrough converter - passes audio through unchanged
// Used for testing the audio pipeline without any processing
class PassthroughConverter : public IVoiceConverter {
public:
    PassthroughConverter();
    ~PassthroughConverter() override;

    bool initialize(const VoiceConverterConfig& config) override;

    std::string getName() const override { return "Passthrough"; }
    std::string getVersion() const override { return "1.0.0"; }

    int getInputSampleRate() const override { return config_.input_sample_rate; }
    int getOutputSampleRate() const override { return config_.output_sample_rate; }

    size_t getAlgorithmicLatencySamples() const override { return 0; }
    double getAlgorithmicLatencyMs() const override { return 0.0; }

    ProcessResult process(const float* input, size_t frames, float* output) override;
    ProcessResult processPlanar(const float* const* input, size_t frames, float* const* output) override;

    void reset() override;
    ProcessResult flush(float* output, size_t max_frames) override;

    bool isReady() const override { return initialized_; }
    std::string getLastError() const override { return last_error_; }

    bool setTargetVoice(const std::string& voice_id) override { return false; }
    bool setPitchShift(float semitones) override { return false; }
    bool setFormantShift(float factor) override { return false; }
    bool setMix(float mix) override { mix_ = mix; return true; }

    VoiceConverterConfig getConfig() const override { return config_; }

private:
    VoiceConverterConfig config_;
    bool initialized_ = false;
    float mix_ = 1.0f;
    std::string last_error_;

    // For passthrough, we need to track input/output sample rates
    int input_sample_rate_ = 48000;
    int output_sample_rate_ = 48000;
};

} // namespace rtvcc