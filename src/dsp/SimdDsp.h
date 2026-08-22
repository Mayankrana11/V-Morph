#pragma once

#include <cstddef>
#include <cmath>
#include <immintrin.h>  // AVX2, FMA

namespace rtvcc {
namespace simd {

// Check for SIMD support at runtime
inline bool hasAVX2() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool osUsesAVX2 = (cpuInfo[2] & (1 << 27)) != 0;  // OSXSAVE
    bool cpuAVX2 = (cpuInfo[2] & (1 << 5)) != 0;      // AVX
    __cpuid(cpuInfo, 7);
    bool cpuAVX2_2 = (cpuInfo[1] & (1 << 5)) != 0;    // AVX2
    return osUsesAVX2 && cpuAVX2 && cpuAVX2_2;
}

inline bool hasFMA() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    return (cpuInfo[2] & (1 << 12)) != 0;  // FMA
}

// =====================================================================
// Vectorized Gain (AVX2)
// =====================================================================
inline void applyGainAVX2(float* data, size_t frames, float gain) {
    if (!hasAVX2()) {
        // Scalar fallback
        for (size_t i = 0; i < frames; ++i) data[i] *= gain;
        return;
    }
    
    const __m256 gain_vec = _mm256_set1_ps(gain);
    size_t i = 0;
    const size_t vec_frames = frames & ~7;  // Multiple of 8
    
    for (; i < vec_frames; i += 8) {
        __m256 vals = _mm256_loadu_ps(data + i);
        vals = _mm256_mul_ps(vals, gain_vec);
        _mm256_storeu_ps(data + i, vals);
    }
    
    // Remainder
    for (; i < frames; ++i) {
        data[i] *= gain;
    }
}

inline void applyGainRampedAVX2(float* data, size_t frames, float& current_gain, float target_gain, float ramp_increment) {
    if (!hasAVX2() || ramp_increment == 0.0f) {
        applyGainAVX2(data, frames, current_gain);
        current_gain = target_gain;
        return;
    }
    
    size_t i = 0;
    const size_t vec_frames = frames & ~7;
    
    for (; i < vec_frames; i += 8) {
        __m256 vals = _mm256_loadu_ps(data + i);
        
        // Create gain vector with ramp
        __m256 gains = _mm256_set_ps(
            current_gain + 7 * ramp_increment,
            current_gain + 6 * ramp_increment,
            current_gain + 5 * ramp_increment,
            current_gain + 4 * ramp_increment,
            current_gain + 3 * ramp_increment,
            current_gain + 2 * ramp_increment,
            current_gain + 1 * ramp_increment,
            current_gain
        );
        
        vals = _mm256_mul_ps(vals, gains);
        _mm256_storeu_ps(data + i, vals);
        
        current_gain += 8 * ramp_increment;
    }
    
    // Remainder
    for (; i < frames; ++i) {
        data[i] *= current_gain;
        current_gain += ramp_increment;
    }
    
    // Clamp to target
    if ((ramp_increment > 0 && current_gain >= target_gain) ||
        (ramp_increment < 0 && current_gain <= target_gain)) {
        current_gain = target_gain;
    }
}

// =====================================================================
// Vectorized High-Pass Filter (AVX2) - Transposed Direct Form II
// =====================================================================
// Note: IIR filters are inherently sequential, but we can process
// multiple channels in parallel
inline void highPassFilterChannelsAVX2(float** channels, size_t num_channels, size_t frames,
                                       float a0, float a1, float b1,
                                       float* states_x1, float* states_y1) {
    if (!hasAVX2() || num_channels < 4) {
        // Scalar fallback for non-AVX2 or few channels
        for (size_t ch = 0; ch < num_channels; ++ch) {
            float x1 = states_x1[ch];
            float y1 = states_y1[ch];
            float* data = channels[ch];
            for (size_t i = 0; i < frames; ++i) {
                float x = data[i];
                float y = a0 * (x - x1) + b1 * y1;
                data[i] = y;
                x1 = x;
                y1 = y;
            }
            states_x1[ch] = x1;
            states_y1[ch] = y1;
        }
        return;
    }
    
    // Process 4 channels at a time using AVX2
    size_t ch = 0;
    for (; ch + 3 < num_channels; ch += 4) {
        float x1_0 = states_x1[ch + 0], y1_0 = states_y1[ch + 0];
        float x1_1 = states_x1[ch + 1], y1_1 = states_y1[ch + 1];
        float x1_2 = states_x1[ch + 2], y1_2 = states_y1[ch + 2];
        float x1_3 = states_x1[ch + 3], y1_3 = states_y1[ch + 3];
        
        float* data0 = channels[ch + 0];
        float* data1 = channels[ch + 1];
        float* data2 = channels[ch + 2];
        float* data3 = channels[ch + 3];
        
        for (size_t i = 0; i < frames; ++i) {
            // Load 4 samples (one from each channel)
            __m256 x = _mm256_set_ps(data3[i], data2[i], data1[i], data0[i], 0, 0, 0, 0);
            
            // y = a0 * (x - x1) + b1 * y1
            // This is still sequential per channel, but we vectorize across channels
            // For true vectorization, we'd need a different filter structure
            float y0 = a0 * (data0[i] - x1_0) + b1 * y1_0;
            float y1 = a0 * (data1[i] - x1_1) + b1 * y1_1;
            float y2 = a0 * (data2[i] - x1_2) + b1 * y1_2;
            float y3 = a0 * (data3[i] - x1_3) + b1 * y1_3;
            
            data0[i] = y0; x1_0 = data0[i]; y1_0 = y0;
            data1[i] = y1; x1_1 = data1[i]; y1_1 = y1;
            data2[i] = y2; x1_2 = data2[i]; y1_2 = y2;
            data3[i] = y3; x1_3 = data3[i]; y1_3 = y3;
        }
        
        states_x1[ch + 0] = x1_0; states_y1[ch + 0] = y1_0;
        states_x1[ch + 1] = x1_1; states_y1[ch + 1] = y1_1;
        states_x1[ch + 2] = x1_2; states_y1[ch + 2] = y1_2;
        states_x1[ch + 3] = x1_3; states_y1[ch + 3] = y1_3;
    }
    
    // Remainder channels
    for (; ch < num_channels; ++ch) {
        float x1 = states_x1[ch];
        float y1 = states_y1[ch];
        float* data = channels[ch];
        for (size_t i = 0; i < frames; ++i) {
            float x = data[i];
            float y = a0 * (x - x1) + b1 * y1;
            data[i] = y;
            x1 = x;
            y1 = y;
        }
        states_x1[ch] = x1;
        states_y1[ch] = y1;
    }
}

// =====================================================================
// Vectorized Limiter (AVX2) - Peak detection across channels
// =====================================================================
inline void limiterProcessAVX2(float* data, size_t frames, size_t channels,
                               float threshold, float release_coeff,
                               float& envelope, float& gain) {
    if (!hasAVX2() || channels != 1) {
        // Scalar fallback
        for (size_t i = 0; i < frames; ++i) {
            float peak = std::abs(data[i]);
            if (peak > envelope) envelope = peak;
            else envelope = envelope * release_coeff + peak * (1.0f - release_coeff);
            
            float target_gain = 1.0f;
            if (envelope > threshold) target_gain = threshold / envelope;
            
            if (target_gain < gain) gain = target_gain;
            else gain = gain * 0.999f + target_gain * 0.001f;
            
            data[i] *= gain;
        }
        return;
    }
    
    // AVX2 optimized single-channel limiter
    const __m256 threshold_vec = _mm256_set1_ps(threshold);
    const __m256 release_vec = _mm256_set1_ps(release_coeff);
    const __m256 one_vec = _mm256_set1_ps(1.0f);
    const __m256 attack_fast_vec = _mm256_set1_ps(0.999f);
    const __m256 attack_slow_vec = _mm256_set1_ps(0.001f);
    
    // We'll process in chunks of 8
    size_t i = 0;
    float env = envelope;
    float g = gain;
    
    for (; i + 7 < frames; i += 8) {
        __m256 samples = _mm256_loadu_ps(data + i);
        __m256 abs_samples = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), samples);  // abs
        
        // Peak detection (horizontal max)
        __m256 max1 = _mm256_max_ps(abs_samples, _mm256_permute2f128_ps(abs_samples, abs_samples, 1));
        __m256 max2 = _mm256_max_ps(max1, _mm256_permute_ps(max1, 0b11100100));
        __m256 max3 = _mm256_max_ps(max2, _mm256_permute_ps(max2, 0b01011000));
        float peak = _mm256_cvtss_f32(max3);
        
        // Update envelope
        if (peak > env) env = peak;
        else env = env * release_coeff + peak * (1.0f - release_coeff);
        
        // Calculate target gain
        float target_gain = (env > threshold) ? threshold / env : 1.0f;
        
        // Smooth gain
        if (target_gain < g) g = target_gain;
        else g = g * 0.999f + target_gain * 0.001f;
        
        // Apply gain
        __m256 gain_vec = _mm256_set1_ps(g);
        samples = _mm256_mul_ps(samples, gain_vec);
        _mm256_storeu_ps(data + i, samples);
    }
    
    // Remainder
    for (; i < frames; ++i) {
        float peak = std::abs(data[i]);
        if (peak > env) env = peak;
        else env = env * release_coeff + peak * (1.0f - release_coeff);
        
        float target_gain = (env > threshold) ? threshold / env : 1.0f;
        if (target_gain < g) g = target_gain;
        else g = g * 0.999f + target_gain * 0.001f;
        
        data[i] *= g;
    }
    
    envelope = env;
    gain = g;
}

// =====================================================================
// Vectorized Copy/Mix (AVX2)
// =====================================================================
inline void mixBuffersAVX2(const float* dry, const float* wet, float* output, 
                           size_t frames, float mix) {
    if (!hasAVX2()) {
        if (mix >= 1.0f) {
            std::copy(wet, wet + frames, output);
        } else if (mix <= 0.0f) {
            std::copy(dry, dry + frames, output);
        } else {
            for (size_t i = 0; i < frames; ++i) {
                output[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
            }
        }
        return;
    }
    
    const __m256 mix_vec = _mm256_set1_ps(mix);
    const __m256 one_minus_mix = _mm256_set1_ps(1.0f - mix);
    
    size_t i = 0;
    const size_t vec_frames = frames & ~7;
    
    for (; i < vec_frames; i += 8) {
        __m256 dry_vec = _mm256_loadu_ps(dry + i);
        __m256 wet_vec = _mm256_loadu_ps(wet + i);
        
        // output = dry * (1-mix) + wet * mix
        __m256 out = _mm256_fmadd_ps(dry_vec, one_minus_mix, _mm256_mul_ps(wet_vec, mix_vec));
        _mm256_storeu_ps(output + i, out);
    }
    
    for (; i < frames; ++i) {
        output[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
    }
}

inline void copyBufferAVX2(const float* src, float* dst, size_t frames) {
    if (!hasAVX2()) {
        std::copy(src, src + frames, dst);
        return;
    }
    
    size_t i = 0;
    const size_t vec_frames = frames & ~7;
    
    for (; i < vec_frames; i += 8) {
        __m256 vals = _mm256_loadu_ps(src + i);
        _mm256_storeu_ps(dst + i, vals);
    }
    
    for (; i < frames; ++i) {
        dst[i] = src[i];
    }
}

inline void fillBufferAVX2(float* dst, size_t frames, float value) {
    if (!hasAVX2()) {
        std::fill(dst, dst + frames, value);
        return;
    }
    
    const __m256 val_vec = _mm256_set1_ps(value);
    size_t i = 0;
    const size_t vec_frames = frames & ~7;
    
    for (; i < vec_frames; i += 8) {
        _mm256_storeu_ps(dst + i, val_vec);
    }
    
    for (; i < frames; ++i) {
        dst[i] = value;
    }
}

// =====================================================================
// Vectorized RMS/Peak Analysis (AVX2)
// =====================================================================
inline float computeRMSAVX2(const float* data, size_t frames) {
    if (!hasAVX2()) {
        double sum = 0.0;
        for (size_t i = 0; i < frames; ++i) sum += double(data[i]) * data[i];
        return float(std::sqrt(sum / frames));
    }
    
    __m256 sum_vec = _mm256_setzero_ps();
    size_t i = 0;
    const size_t vec_frames = frames & ~7;
    
    for (; i < vec_frames; i += 8) {
        __m256 vals = _mm256_loadu_ps(data + i);
        __m256 sq = _mm256_mul_ps(vals, vals);
        sum_vec = _mm256_add_ps(sum_vec, sq);
    }
    
    // Horizontal sum
    __m256 sum1 = _mm256_hadd_ps(sum_vec, sum_vec);
    __m256 sum2 = _mm256_hadd_ps(sum1, sum1);
    float sum = _mm256_cvtss_f32(sum2) + _mm256_cvtss_f32(_mm256_permute2f128_ps(sum2, sum2, 1));
    
    // Remainder
    for (; i < frames; ++i) {
        sum += data[i] * data[i];
    }
    
    return std::sqrt(sum / frames);
}

inline float computePeakAVX2(const float* data, size_t frames) {
    if (!hasAVX2()) {
        float peak = 0.0f;
        for (size_t i = 0; i < frames; ++i) {
            peak = std::max(peak, std::abs(data[i]));
        }
        return peak;
    }
    
    __m256 peak_vec = _mm256_setzero_ps();
    size_t i = 0;
    const size_t vec_frames = frames & ~7;
    
    for (; i < vec_frames; i += 8) {
        __m256 vals = _mm256_loadu_ps(data + i);
        __m256 abs_vals = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), vals);  // abs
        peak_vec = _mm256_max_ps(peak_vec, abs_vals);
    }
    
    // Horizontal max
    __m256 max1 = _mm256_max_ps(peak_vec, _mm256_permute2f128_ps(peak_vec, peak_vec, 1));
    __m256 max2 = _mm256_max_ps(max1, _mm256_permute_ps(max1, 0b11100100));
    __m256 max3 = _mm256_max_ps(max2, _mm256_permute_ps(max2, 0b01011000));
    float peak = _mm256_cvtss_f32(max3);
    
    // Remainder
    for (; i < frames; ++i) {
        peak = std::max(peak, std::abs(data[i]));
    }
    
    return peak;
}

} // namespace simd
} // namespace rtvcc