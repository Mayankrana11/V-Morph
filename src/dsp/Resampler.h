#pragma once

#include <cstddef>
#include <vector>

namespace rtvcc {

// High-quality resampler using polyphase FIR filters
// Supports arbitrary rational resampling ratios
class Resampler {
public:
    Resampler();
    ~Resampler();

    // Configure resampler
    // input_rate: input sample rate
    // output_rate: output sample rate
    // channels: number of channels (1 or 2)
    // quality: 0=fast, 1=medium, 2=high (default 1)
    bool configure(int input_rate, int output_rate, int channels = 1, int quality = 1);

    // Process interleaved audio
    // input:  [in_frames * channels]
    // output: [out_frames * channels] (pre-allocated)
    // Returns number of output frames written
    size_t process(const float* input, size_t in_frames, float* output, size_t max_out_frames);

    // Process planar audio (per channel)
    size_t processPlanar(const float* const* input, size_t in_frames, float* const* output, size_t max_out_frames);

    // Get latency in input frames
    size_t getInputLatency() const;

    // Get latency in output frames
    size_t getOutputLatency() const;

    // Reset internal state
    void reset();

    // Check if configured
    bool isConfigured() const;

    // Get ratio
    double getRatio() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// High-pass filter (1st order, for DC removal)
class HighPassFilter {
public:
    HighPassFilter();
    ~HighPassFilter();

    // Set cutoff frequency in Hz
    void setCutoff(float frequency_hz, float sample_rate);

    // Process single channel in-place
    void process(float* data, size_t frames);

    // Process interleaved multi-channel
    void processInterleaved(float* data, size_t frames, size_t channels, size_t channel_stride = 1);

    // Reset filter state
    void reset();

private:
    float a0_, a1_, b1_;
    float x1_ = 0.0f, y1_ = 0.0f;
    bool initialized_ = false;
};

// Gain control with smooth ramping
class Gain {
public:
    Gain();
    ~Gain();

    // Set target gain in dB
    void setGainDb(float gain_db);
    float getGainDb() const;

    // Set ramp time in seconds (for smooth transitions)
    void setRampTime(float seconds);

    // Process in-place
    void process(float* data, size_t frames);
    void processInterleaved(float* data, size_t frames, size_t channels);

    // Reset to target gain immediately
    void reset();

private:
    float current_gain_ = 1.0f;
    float target_gain_ = 1.0f;
    float ramp_increment_ = 0.0f;
    float ramp_remaining_ = 0.0f;
};

// Peak limiter with lookahead
class Limiter {
public:
    Limiter();
    ~Limiter();

    // Configure limiter
    // threshold_db: threshold in dB (e.g., -1.0)
    // release_ms: release time in milliseconds
    // lookahead_ms: lookahead time in milliseconds (0-10)
    void configure(float threshold_db = -1.0f, float release_ms = 50.0f, float lookahead_ms = 1.0f);

    // Process in-place (interleaved)
    void process(float* data, size_t frames, size_t channels);

    // Get current gain reduction in dB
    float getGainReductionDb() const;

    // Reset state
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rtvcc