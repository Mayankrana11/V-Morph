#pragma once

#include <cstddef>
#include <string>
#include <memory>

namespace rtvcc {

// Voice converter processing result
struct ProcessResult {
    enum class Status {
        Success,
        NeedMoreInput,
        OutputFull,
        Error,
        NotInitialized
    };

    Status status = Status::Success;
    size_t input_consumed = 0;
    size_t output_produced = 0;
    std::string error;
    double processing_time_ms = 0.0;
};

// Voice converter configuration
struct VoiceConverterConfig {
    std::string model_path;
    std::string target_voice;  // Speaker ID or profile name
    float pitch_shift = 0.0f;  // Semitones
    float formant_shift = 1.0f;
    float mix = 1.0f;          // Dry/wet mix (0=dry, 1=wet)
    float output_gain_db = 0.0f;
    int chunk_size_ms = 20;    // Processing chunk size
    std::string execution_provider = "CPU";
};

// Abstract voice converter interface
// The audio engine knows only this interface, not model specifics
class IVoiceConverter {
public:
    virtual ~IVoiceConverter() = default;

    // Initialize the converter with configuration
    // Must be called before process()
    virtual bool initialize(const VoiceConverterConfig& config) = 0;

    // Get converter name/identifier
    virtual std::string getName() const = 0;

    // Get converter version
    virtual std::string getVersion() const = 0;

    // Get input sample rate requirement
    virtual int getInputSampleRate() const = 0;

    // Get output sample rate
    virtual int getOutputSampleRate() const = 0;

    // Get algorithmic latency in samples (at input sample rate)
    virtual size_t getAlgorithmicLatencySamples() const = 0;

    // Get algorithmic latency in milliseconds
    virtual double getAlgorithmicLatencyMs() const = 0;

    // Process audio chunk
    // input:  [frames * channels] at input sample rate
    // output: [frames * channels] at output sample rate
    // frames: number of frames to process
    // Returns result with status and frames consumed/produced
    virtual ProcessResult process(const float* input, size_t frames, float* output) = 0;

    // Process with separate input/output buffers (planar)
    virtual ProcessResult processPlanar(const float* const* input, size_t frames, float* const* output) = 0;

    // Reset streaming state (call when stream restarts, model changes, etc.)
    virtual void reset() = 0;

    // Flush remaining output (for end of stream)
    virtual ProcessResult flush(float* output, size_t max_frames) = 0;

    // Check if initialized and ready
    virtual bool isReady() const = 0;

    // Get last error message
    virtual std::string getLastError() const = 0;

    // Set target voice/profile (if supported)
    virtual bool setTargetVoice(const std::string& voice_id) = 0;

    // Set pitch shift in semitones (if supported)
    virtual bool setPitchShift(float semitones) = 0;

    // Set formant shift factor (if supported)
    virtual bool setFormantShift(float factor) = 0;

    // Set dry/wet mix (0.0 = dry, 1.0 = wet)
    virtual bool setMix(float mix) = 0;

    // Get current configuration
    virtual VoiceConverterConfig getConfig() const = 0;
};

// Factory for creating voice converters
std::unique_ptr<IVoiceConverter> createVoiceConverter(const std::string& type);

} // namespace rtvcc