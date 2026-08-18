#include <gtest/gtest.h>
#include "vc/PassthroughConverter.h"
#include "vc/DSPVoiceConverter.h"

using namespace rtvcc;

TEST(VoiceConverterTest, PassthroughIdentity) {
    PassthroughConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    ASSERT_TRUE(converter.initialize(config));

    std::vector<float> input(100);
    for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<float>(i) * 0.01f;
    std::vector<float> output(100);

    auto result = converter.process(input.data(), input.size(), output.data());
    EXPECT_EQ(result.status, IVoiceConverter::ProcessResult::Status::Success);
    EXPECT_EQ(result.input_consumed, 100);
    EXPECT_EQ(result.output_produced, 100);

    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_FLOAT_EQ(output[i], input[i]);
    }
}

TEST(VoiceConverterTest, PassthroughMixControl) {
    PassthroughConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    ASSERT_TRUE(converter.initialize(config));
    converter.setMix(0.5f);

    std::vector<float> input(100, 1.0f);
    std::vector<float> output(100);

    auto result = converter.process(input.data(), input.size(), output.data());
    for (float s : output) {
        EXPECT_FLOAT_EQ(s, 0.5f);
    }
}

TEST(VoiceConverterTest, DSPVoiceConverterBasic) {
    DSPVoiceConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    config.input_gain_db = 0.0f;
    config.output_gain_db = 0.0f;
    config.limiter_threshold_db = -1.0f;
    ASSERT_TRUE(converter.initialize(config));

    std::vector<float> input(1000, 0.5f);
    std::vector<float> output(1000);

    auto result = converter.process(input.data(), input.size(), output.data());
    EXPECT_EQ(result.status, IVoiceConverter::ProcessResult::Status::Success);
}

TEST(VoiceConverterTest, DSPVoiceConverterHighpassRemovesDC) {
    DSPVoiceConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    config.enable_highpass = true;
    config.highpass_cutoff_hz = 80.0f;
    config.enable_limiter = false;
    ASSERT_TRUE(converter.initialize(config));

    std::vector<float> input(5000, 1.0f);
    std::vector<float> output(5000);

    converter.process(input.data(), input.size(), output.data());

    float sum = 0.0f;
    for (size_t i = 1000; i < output.size(); ++i) {
        sum += std::abs(output[i]);
    }
    EXPECT_LT(sum / 4000.0f, 0.01f);
}

TEST(VoiceConverterTest, DSPVoiceConverterGainControl) {
    DSPVoiceConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    config.enable_highpass = false;
    config.enable_limiter = false;
    ASSERT_TRUE(converter.initialize(config));

    converter.setInputGainDb(6.0f);  // 2x

    std::vector<float> input(100, 0.5f);
    std::vector<float> output(100);

    converter.process(input.data(), input.size(), output.data());

    for (float s : output) {
        EXPECT_NEAR(s, 1.0f, 0.01f);
    }
}

TEST(VoiceConverterTest, ConverterReset) {
    PassthroughConverter converter;
    VoiceConverterConfig config;
    config.sample_rate = 48000;
    ASSERT_TRUE(converter.initialize(config));

    std::vector<float> input(10, 1.0f);
    std::vector<float> output(10);

    converter.process(input.data(), input.size(), output.data());
    converter.reset();
    converter.process(input.data(), input.size(), output.data());

    // Should work after reset
    for (float s : output) {
        EXPECT_FLOAT_EQ(s, 1.0f);
    }
}