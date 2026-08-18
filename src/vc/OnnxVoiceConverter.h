#pragma once

#include "VoiceConverter.h"
#include "inference/InferenceEngine.h"
#include "dsp/Resampler.h"
#include <memory>
#include <vector>
#include <string>

namespace rtvcc {

// ONNX-based streaming voice converter
// Implements the IVoiceConverter interface using ONNX Runtime
class OnnxVoiceConverter : public IVoiceConverter {
public:
    OnnxVoiceConverter();
    ~OnnxVoiceConverter() override;

    bool initialize(const VoiceConverterConfig& config) override;

    std::string getName() const override { return "OnnxVoiceConverter"; }
    std::string getVersion() const override { return "1.0.0"; }

    int getInputSampleRate() const override { return input_sample_rate_; }
    int getOutputSampleRate() const override { return output_sample_rate_; }

    size_t getAlgorithmicLatencySamples() const override { return algorithmic_latency_samples_; }
    double getAlgorithmicLatencyMs() const override { return algorithmic_latency_ms_; }

    ProcessResult process(const float* input, size_t frames, float* output) override;
    ProcessResult processPlanar(const float* const* input, size_t frames, float* const* output) override;

    void reset() override;
    ProcessResult flush(float* output, size_t max_frames) override;

    bool isReady() const override { return initialized_ && inference_engine_ && inference_engine_->isReady(); }
    std::string getLastError() const override { return last_error_; }

    bool setTargetVoice(const std::string& voice_id) override;
    bool setPitchShift(float semitones) override;
    bool setFormantShift(float factor) override;
    bool setMix(float mix) override { mix_ = mix; return true; }

    VoiceConverterConfig getConfig() const override { return config_; }

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    VoiceConverterConfig config_;
    bool initialized_ = false;
    int input_sample_rate_ = 48000;
    int output_sample_rate_ = 48000;
    int model_sample_rate_ = 16000;
    size_t algorithmic_latency_samples_ = 0;
    double algorithmic_latency_ms_ = 0.0;
    float mix_ = 1.0f;
    std::string last_error_;

    // Resamplers for sample rate conversion
    std::unique_ptr<Resampler> input_resampler_;
    std::unique_ptr<Resampler> output_resampler_;

    // Inference engine
    std::unique_ptr<IInferenceEngine> inference_engine_;

    // Buffers
    std::vector<float> resampled_input_;
    std::vector<float> resampled_output_;
    std::vector<float> model_input_;
    std::vector<float> model_output_;
    std::vector<const float*> inference_inputs_;
    std::vector<float*> inference_outputs_;

    // Streaming state
    size_t chunk_frames_ = 0;
    bool first_chunk_ = true;
};

} // namespace rtvcc