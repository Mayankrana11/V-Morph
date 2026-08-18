#pragma once

#include "VoiceConverter.h"
#include "../dsp/HighPassFilter.h"
#include "../dsp/Gain.h"
#include "../dsp/Limiter.h"

namespace rtvcc {

// DSP-based voice effects converter
// Applies gain, high-pass filter, limiter, and other DSP effects
class DSPVoiceConverter : public IVoiceConverter {
public:
    DSPVoiceConverter();
    ~DSPVoiceConverter() override;

    bool initialize(const VoiceConverterConfig& config) override;

    std::string getName() const override { return "DSP Effects"; }
    std::string getVersion() const override { return "1.0.0"; }

    int getInputSampleRate() const override { return sample_rate_; }
    int getOutputSampleRate() const override { return sample_rate_; }

    size_t getAlgorithmicLatencySamples() const override { return highpass_.getLatency ? highpass_.getLatency() : 0; }
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

    // DSP-specific controls
    void setInputGainDb(float db);
    void setOutputGainDb(float db);
    void setHighpassCutoff(float hz);
    void setLimiterThresholdDb(float db);
    void setLimiterReleaseMs(float ms);
    void enableHighpass(bool enable);
    void enableLimiter(bool enable);

    VoiceConverterConfig getConfig() const override { return config_; }

private:
    VoiceConverterConfig config_;
    bool initialized_ = false;
    int sample_rate_ = 48000;
    float mix_ = 1.0f;
    std::string last_error_;

    HighPassFilter highpass_;
    Gain input_gain_;
    Gain output_gain_;
    Limiter limiter_;

    bool highpass_enabled_ = true;
    bool limiter_enabled_ = true;

    // Temporary buffers for processing
    std::vector<float> temp_buffer_;
};

} // namespace rtvcc