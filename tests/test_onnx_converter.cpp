#include <gtest/gtest.h>
#include "vc/OnnxVoiceConverter.h"
#include "vc/VoiceConverter.h"
#include <vector>
#include <string>
#include <filesystem>

using namespace rtvcc;

TEST(OnnxVoiceConverterTest, CreateConverter) {
    auto converter = createVoiceConverter("onnx");
    EXPECT_NE(converter, nullptr);
    EXPECT_EQ(converter->getName(), "OnnxVoiceConverter");
}

TEST(OnnxVoiceConverterTest, CreateStreamingConverter) {
    auto converter = createVoiceConverter("streaming");
    EXPECT_NE(converter, nullptr);
    EXPECT_EQ(converter->getName(), "OnnxVoiceConverter");
}

TEST(OnnxVoiceConverterTest, InvalidModelPath) {
    auto converter = createVoiceConverter("onnx");
    VoiceConverterConfig config;
    config.model_path = "nonexistent.onnx";
    config.sample_rate = 48000;
    config.chunk_size_ms = 20;
    
    EXPECT_FALSE(converter->initialize(config));
    EXPECT_FALSE(converter->isReady());
    EXPECT_FALSE(converter->getLastError().empty());
}

TEST(OnnxVoiceConverterTest, OnnxConverterWithTestModel) {
    // This test requires a test model to be present
    std::string model_path = "models/test_identity.onnx";
    if (!std::filesystem::exists(model_path)) {
        GTEST_SKIP() << "Test model not found at " << model_path;
    }
    
    auto converter = createVoiceConverter("onnx");
    VoiceConverterConfig config;
    config.model_path = model_path;
    config.sample_rate = 48000;
    config.chunk_size_ms = 20;
    config.execution_provider = "CPU";
    
    EXPECT_TRUE(converter->initialize(config)) << converter->getLastError();
    EXPECT_TRUE(converter->isReady());
    
    // Check sample rates
    EXPECT_EQ(converter->getInputSampleRate(), 48000);
    EXPECT_EQ(converter->getOutputSampleRate(), 48000);
    EXPECT_GE(converter->getAlgorithmicLatencyMs(), 0.0);
    
    // Test process with simple signal
    const size_t frames = 128;
    std::vector<float> input(frames);
    std::vector<float> output(frames);
    
    // Generate test signal (sine wave)
    for (size_t i = 0; i < frames; ++i) {
        input[i] = 0.5f * std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);
    }
    
    auto result = converter->process(input.data(), frames, output.data());
    EXPECT_EQ(result.status, IVoiceConverter::ProcessResult::Status::Success);
    EXPECT_EQ(result.input_consumed, frames);
    EXPECT_EQ(result.output_produced, frames);
    
    // For identity model with resampling, output should be similar to input
    // (allowing for resampling artifacts)
    float input_rms = 0.0f, output_rms = 0.0f;
    for (size_t i = 0; i < frames; ++i) {
        input_rms += input[i] * input[i];
        output_rms += output[i] * output[i];
    }
    input_rms = std::sqrt(input_rms / frames);
    output_rms = std::sqrt(output_rms / frames);
    
    // RMS should be similar (within 10% for identity)
    EXPECT_NEAR(output_rms / input_rms, 1.0f, 0.1f);
}

TEST(OnnxVoiceConverterTest, ConverterReset) {
    std::string model_path = "models/test_identity.onnx";
    if (!std::filesystem::exists(model_path)) {
        GTEST_SKIP() << "Test model not found";
    }
    
    auto converter = createVoiceConverter("onnx");
    VoiceConverterConfig config;
    config.model_path = model_path;
    config.sample_rate = 48000;
    config.chunk_size_ms = 20;
    
    EXPECT_TRUE(converter->initialize(config));
    
    std::vector<float> input(128, 0.5f);
    std::vector<float> output(128);
    
    converter->process(input.data(), input.size(), output.data());
    converter->reset();
    converter->process(input.data(), input.size(), output.data());
    
    // Should work after reset
    for (float s : output) {
        EXPECT_NEAR(std::abs(s), 0.5f, 0.01f);
    }
}

TEST(OnnxVoiceConverterTest, ConverterMixControl) {
    std::string model_path = "models/test_identity.onnx";
    if (!std::filesystem::exists(model_path)) {
        GTEST_SKIP() << "Test model not found";
    }
    
    auto converter = createVoiceConverter("onnx");
    VoiceConverterConfig config;
    config.model_path = model_path;
    config.sample_rate = 48000;
    config.chunk_size_ms = 20;
    
    EXPECT_TRUE(converter->initialize(config));
    
    std::vector<float> input(128, 1.0f);
    std::vector<float> output(128);
    
    // Test mix = 0 (dry only)
    converter->setMix(0.0f);
    converter->process(input.data(), input.size(), output.data());
    for (float s : output) {
        EXPECT_FLOAT_EQ(s, 1.0f);
    }
    
    // Test mix = 0.5
    converter->setMix(0.5f);
    converter->process(input.data(), input.size(), output.data());
    for (float s : output) {
        // For identity model, 0.5 mix should give 0.5 * input + 0.5 * output = 1.0
        EXPECT_NEAR(s, 1.0f, 0.01f);
    }
}