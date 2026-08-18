#include "Resampler.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace rtvcc {

// =====================================================================
// HighPassFilter
// =====================================================================

HighPassFilter::HighPassFilter() = default;
HighPassFilter::~HighPassFilter() = default;

void HighPassFilter::setCutoff(float frequency_hz, float sample_rate) {
    if (frequency_hz <= 0 || sample_rate <= 0) return;

    float rc = 1.0f / (2.0f * 3.14159265359f * frequency_hz);
    float dt = 1.0f / sample_rate;
    float alpha = rc / (rc + dt);

    // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    a0_ = alpha;
    a1_ = -alpha;
    b1_ = alpha;
    initialized_ = true;
}

void HighPassFilter::process(float* data, size_t frames) {
    if (!initialized_) return;

    for (size_t i = 0; i < frames; ++i) {
        float x = data[i];
        float y = a0_ * (x - x1_) + b1_ * y1_;
        data[i] = y;
        x1_ = x;
        y1_ = y;
    }
}

void HighPassFilter::processInterleaved(float* data, size_t frames, size_t channels, size_t channel_stride) {
    if (!initialized_) return;

    for (size_t ch = 0; ch < channels; ++ch) {
        float x1 = 0.0f, y1 = 0.0f;
        for (size_t i = 0; i < frames; ++i) {
            float x = data[i * channel_stride + ch];
            float y = a0_ * (x - x1) + b1_ * y1;
            data[i * channel_stride + ch] = y;
            x1 = x;
            y1 = y;
        }
    }
}

void HighPassFilter::reset() {
    x1_ = 0.0f;
    y1_ = 0.0f;
}

// =====================================================================
// Gain
// =====================================================================

Gain::Gain() = default;
Gain::~Gain() = default;

void Gain::setGainDb(float gain_db) {
    target_gain_ = std::pow(10.0f, gain_db / 20.0f);
    if (ramp_remaining_ <= 0.0f) {
        current_gain_ = target_gain_;
    }
}

float Gain::getGainDb() const {
    return 20.0f * std::log10(target_gain_);
}

void Gain::setRampTime(float seconds) {
    if (seconds <= 0.0f) {
        ramp_increment_ = 0.0f;
        current_gain_ = target_gain_;
    } else {
        // Calculate per-sample increment for smooth ramping at 48kHz
        ramp_increment_ = (target_gain_ - current_gain_) / (seconds * 48000.0f);
    }
}

void Gain::process(float* data, size_t frames) {
    if (ramp_increment_ == 0.0f) {
        // No ramping, apply constant gain
        if (current_gain_ != 1.0f) {
            for (size_t i = 0; i < frames; ++i) {
                data[i] *= current_gain_;
            }
        }
    } else {
        // Ramping
        for (size_t i = 0; i < frames; ++i) {
            data[i] *= current_gain_;
            current_gain_ += ramp_increment_;
            if ((ramp_increment_ > 0 && current_gain_ >= target_gain_) ||
                (ramp_increment_ < 0 && current_gain_ <= target_gain_)) {
                current_gain_ = target_gain_;
                ramp_increment_ = 0.0f;
            }
        }
    }
}

void Gain::processInterleaved(float* data, size_t frames, size_t channels) {
    if (ramp_increment_ == 0.0f) {
        if (current_gain_ != 1.0f) {
            for (size_t i = 0; i < frames * channels; ++i) {
                data[i] *= current_gain_;
            }
        }
    } else {
        for (size_t i = 0; i < frames * channels; ++i) {
            data[i] *= current_gain_;
            current_gain_ += ramp_increment_;
            if ((ramp_increment_ > 0 && current_gain_ >= target_gain_) ||
                (ramp_increment_ < 0 && current_gain_ <= target_gain_)) {
                current_gain_ = target_gain_;
                ramp_increment_ = 0.0f;
            }
        }
    }
}

void Gain::reset() {
    current_gain_ = target_gain_;
    ramp_increment_ = 0.0f;
}

// =====================================================================
// Limiter
// =====================================================================

class Limiter::Impl {
public:
    Impl() = default;

    float threshold_ = 0.891f;      // -1 dB
    float release_coeff_ = 0.999f;  // ~50ms at 48kHz
    float lookahead_samples_ = 48;  // 1ms at 48kHz
    float envelope_ = 1.0f;
    float gain_ = 1.0f;
    std::vector<float> delay_line_;
    size_t delay_write_ = 0;
    int channels_ = 1;
};

Limiter::Limiter() : pimpl_(std::make_unique<Impl>()) {}
Limiter::~Limiter() = default;

void Limiter::configure(float threshold_db, float release_ms, float lookahead_ms) {
    auto& impl = *pimpl_;
    impl.threshold_ = std::pow(10.0f, threshold_db / 20.0f);

    // Release coefficient: e^(-1/(release_time * sample_rate))
    float release_time = release_ms / 1000.0f;
    impl.release_coeff_ = std::exp(-1.0f / (release_time * 48000.0f));

    // Lookahead delay line
    impl.lookahead_samples_ = lookahead_ms / 1000.0f * 48000.0f;
    impl.delay_line_.assign(static_cast<size_t>(impl.lookahead_samples_) * impl.channels_, 0.0f);
    impl.delay_write_ = 0;
    impl.envelope_ = 1.0f;
    impl.gain_ = 1.0f;
}

void Limiter::process(float* data, size_t frames, size_t channels) {
    auto& impl = *pimpl_;
    impl.channels_ = static_cast<int>(channels);

    // Resize delay line if channels changed
    size_t delay_size = static_cast<size_t>(impl.lookahead_samples_) * channels;
    if (impl.delay_line_.size() != delay_size) {
        impl.delay_line_.assign(delay_size, 0.0f);
        impl.delay_write_ = 0;
    }

    for (size_t i = 0; i < frames; ++i) {
        // Find peak across channels for this frame
        float peak = 0.0f;
        for (size_t ch = 0; ch < channels; ++ch) {
            float sample = std::abs(data[i * channels + ch]);
            peak = std::max(peak, sample);
        }

        // Update envelope (peak detector with release)
        if (peak > impl.envelope_) {
            impl.envelope_ = peak;  // Attack: instant
        } else {
            impl.envelope_ = impl.envelope_ * impl.release_coeff_ + peak * (1.0f - impl.release_coeff_);
        }

        // Calculate required gain reduction
        float target_gain = 1.0f;
        if (impl.envelope_ > impl.threshold_) {
            target_gain = impl.threshold_ / impl.envelope_;
        }

        // Smooth gain changes
        if (target_gain < impl.gain_) {
            impl.gain_ = target_gain;  // Fast attack
        } else {
            impl.gain_ = impl.gain_ * 0.999f + target_gain * 0.001f;  // Slow release
        }

        // Write to delay line (lookahead)
        for (size_t ch = 0; ch < channels; ++ch) {
            impl.delay_line_[impl.delay_write_ * channels + ch] = data[i * channels + ch];
        }
        impl.delay_write_ = (impl.delay_write_ + 1) % impl.lookahead_samples_;

        // Read from delay line and apply gain
        size_t read_pos = impl.delay_write_;
        for (size_t ch = 0; ch < channels; ++ch) {
            data[i * channels + ch] = impl.delay_line_[read_pos * channels + ch] * impl.gain_;
        }
    }
}

float Limiter::getGainReductionDb() const {
    auto& impl = *pimpl_;
    if (impl.gain_ >= 1.0f) return 0.0f;
    return 20.0f * std::log10(impl.gain_);
}

void Limiter::reset() {
    auto& impl = *pimpl_;
    impl.envelope_ = 1.0f;
    impl.gain_ = 1.0f;
    std::fill(impl.delay_line_.begin(), impl.delay_line_.end(), 0.0f);
    impl.delay_write_ = 0;
}

// =====================================================================
// Resampler (simplified - use a proper library like soxr or libresample in production)
// =====================================================================

class Resampler::Impl {
public:
    int input_rate_ = 48000;
    int output_rate_ = 48000;
    int channels_ = 1;
    double ratio_ = 1.0;
    size_t input_latency_ = 0;
    size_t output_latency_ = 0;

    // Simple linear interpolation state
    double phase_ = 0.0;
    double phase_increment_ = 1.0;
    std::vector<float> prev_input_;  // Per channel
};

Resampler::Resampler() : pimpl_(std::make_unique<Impl>()) {}
Resampler::~Resampler() = default;

bool Resampler::configure(int input_rate, int output_rate, int channels, int quality) {
    auto& impl = *pimpl_;

    if (input_rate <= 0 || output_rate <= 0 || channels <= 0 || channels > 2) {
        return false;
    }

    impl.input_rate_ = input_rate;
    impl.output_rate_ = output_rate;
    impl.channels_ = channels;
    impl.ratio_ = static_cast<double>(output_rate) / input_rate;
    impl.phase_increment_ = 1.0 / impl.ratio_;
    impl.prev_input_.assign(channels, 0.0f);

    // Latency estimation (for linear interpolation, ~1 sample)
    impl.input_latency_ = 1;
    impl.output_latency_ = 1;

    return true;
}

size_t Resampler::process(const float* input, size_t in_frames, float* output, size_t max_out_frames) {
    auto& impl = *pimpl_;

    if (impl.ratio_ == 1.0) {
        // Pass through
        size_t frames = std::min(in_frames, max_out_frames);
        std::copy(input, input + frames * impl.channels_, output);
        return frames;
    }

    size_t out_frames = 0;
    size_t in_idx = 0;
    double phase = impl.phase_;

    while (out_frames < max_out_frames) {
        // Calculate input index
        size_t in_idx_floor = static_cast<size_t>(phase);
        double frac = phase - in_idx_floor;

        if (in_idx_floor + 1 >= in_frames) {
            // Need more input
            break;
        }

        // Linear interpolation per channel
        for (int ch = 0; ch < impl.channels_; ++ch) {
            float x0 = (in_idx_floor == 0) ? impl.prev_input_[ch] : input[(in_idx_floor - 1) * impl.channels_ + ch];
            float x1 = input[in_idx_floor * impl.channels_ + ch];
            output[out_frames * impl.channels_ + ch] = x0 + frac * (x1 - x0);
        }

        phase += impl.phase_increment_;
        out_frames++;
    }

    // Save last input samples for next call
    if (in_frames > 0) {
        for (int ch = 0; ch < impl.channels_; ++ch) {
            impl.prev_input_[ch] = input[(in_frames - 1) * impl.channels_ + ch];
        }
    }

    impl.phase_ = phase - static_cast<size_t>(phase);
    return out_frames;
}

size_t Resampler::processPlanar(const float* const* input, size_t in_frames, float* const* output, size_t max_out_frames) {
    // Simplified: interleave, process, deinterleave
    // In production, implement proper planar processing
    return 0;
}

size_t Resampler::getInputLatency() const {
    return pimpl_->input_latency_;
}

size_t Resampler::getOutputLatency() const {
    return pimpl_->output_latency_;
}

void Resampler::reset() {
    pimpl_->phase_ = 0.0;
    std::fill(pimpl_->prev_input_.begin(), pimpl_->prev_input_.end(), 0.0f);
}

bool Resampler::isConfigured() const {
    return pimpl_->input_rate_ > 0 && pimpl_->output_rate_ > 0;
}

double Resampler::getRatio() const {
    return pimpl_->ratio_;
}

} // namespace rtvcc