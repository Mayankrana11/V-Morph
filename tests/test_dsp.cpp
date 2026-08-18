#include <gtest/gtest.h>
#include "dsp/Resampler.h"
#include "dsp/HighPassFilter.h"
#include "dsp/Gain.h"
#include "dsp/Limiter.h"

using namespace rtvcc;

TEST(DSPTest, HighPassFilterDCRemoval) {
    HighPassFilter hp;
    hp.setCutoff(80.0f, 48000.0f);

    std::vector<float> signal(1000, 1.0f);
    hp.process(signal.data(), signal.size());

    float sum = 0.0f;
    for (size_t i = 100; i < signal.size(); ++i) {
        sum += std::abs(signal[i]);
    }
    EXPECT_LT(sum / 900.0f, 0.01f);
}

TEST(DSPTest, HighPassFilterPassesHighFreq) {
    HighPassFilter hp;
    hp.setCutoff(80.0f, 48000.0f);

    std::vector<float> signal(48000);
    for (size_t i = 0; i < signal.size(); ++i) {
        signal[i] = std::sin(2.0f * 3.14159f * 1000.0f * i / 48000.0f);
    }

    float rms_before = 0.0f;
    for (float s : signal) rms_before += s * s;
    rms_before = std::sqrt(rms_before / signal.size());

    hp.process(signal.data(), signal.size());

    float rms_after = 0.0f;
    for (float s : signal) rms_after += s * s;
    rms_after = std::sqrt(rms_after / signal.size());

    EXPECT_GT(rms_after / rms_before, 0.9f);
}

TEST(DSPTest, GainConstantGain) {
    Gain gain;
    gain.setGainDb(6.0f);
    gain.setRampTime(0.0f);

    std::vector<float> signal(100, 0.5f);
    gain.process(signal.data(), signal.size());

    for (float s : signal) {
        EXPECT_FLOAT_EQ(s, 1.0f);
    }
}

TEST(DSPTest, GainRamping) {
    Gain gain;
    gain.setGainDb(0.0f);
    gain.setRampTime(0.0f);
    gain.setGainDb(20.0f);
    gain.setRampTime(0.1f);

    std::vector<float> signal(4800, 1.0f);
    gain.process(signal.data(), signal.size());

    EXPECT_NEAR(signal[0], 1.0f, 0.1f);
    EXPECT_NEAR(signal[4799], 10.0f, 0.1f);
}

TEST(DSPTest, LimiterSoftLimiting) {
    Limiter limiter;
    limiter.configure(-6.0f, 50.0f, 1.0f);

    std::vector<float> signal(1000, 1.0f);
    limiter.process(signal.data(), signal.size(), 1);

    for (size_t i = 100; i < signal.size(); ++i) {
        EXPECT_NEAR(std::abs(signal[i]), 0.5f, 0.05f);
    }
}

TEST(DSPTest, LimiterNoGainReductionBelowThreshold) {
    Limiter limiter;
    limiter.configure(-6.0f, 50.0f, 1.0f);

    std::vector<float> signal(1000, 0.1f);
    limiter.process(signal.data(), signal.size(), 1);

    for (float s : signal) {
        EXPECT_NEAR(std::abs(s), 0.1f, 0.01f);
    }
}

TEST(DSPTest, ResamplerPassThrough) {
    Resampler resampler;
    EXPECT_TRUE(resampler.configure(48000, 48000, 1));

    std::vector<float> input(100);
    for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<float>(i);
    std::vector<float> output(100);

    size_t out_frames = resampler.process(input.data(), input.size(), output.data(), output.size());
    EXPECT_EQ(out_frames, 100);

    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_FLOAT_EQ(output[i], input[i]);
    }
}

TEST(DSPTest, ResamplerDownsample) {
    Resampler resampler;
    EXPECT_TRUE(resampler.configure(48000, 16000, 1));

    std::vector<float> input(48000);
    for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<float>(i);
    std::vector<float> output(16000);

    size_t out_frames = resampler.process(input.data(), input.size(), output.data(), output.size());
    EXPECT_EQ(out_frames, 16000);
}