#include <gtest/gtest.h>
#include "inference/InferenceEngine.h"
#include "inference/OnnxInferenceEngine.h"
#include <vector>
#include <string>
#include <filesystem>

using namespace rtvcc;

TEST(InferenceEngineTest, CreateOnnxEngine) {
    auto engine = createInferenceEngine("onnx");
    EXPECT_NE(engine, nullptr);
}

TEST(InferenceEngineTest, OnnxEngineNotInitialized) {
    auto engine = createInferenceEngine("onnx");
    EXPECT_FALSE(engine->isReady());
    EXPECT_EQ(engine->getLastError(), "");
}

TEST(InferenceEngineTest, OnnxEngineInvalidModel) {
    auto engine = createInferenceEngine("onnx");
    InferenceConfig config;
    config.model_path = "nonexistent_model.onnx";
    
    EXPECT_FALSE(engine->initialize(config));
    EXPECT_FALSE(engine->isReady());
    EXPECT_FALSE(engine->getLastError().empty());
}

TEST(InferenceEngineTest, TensorInfoElementCount) {
    TensorInfo info;
    info.shape = {1, 1, 320};
    info.type = "float32";
    
    EXPECT_EQ(info.element_count(), 320);
    EXPECT_EQ(info.byte_size(), 320 * 4);
}

TEST(InferenceEngineTest, TensorUtilsValidateShape) {
    std::vector<int64_t> actual = {1, 1, 320};
    std::vector<int64_t> expected = {1, 1, 320};
    EXPECT_TRUE(TensorUtils::validateShape(actual, expected));
    
    // Test with dynamic dimension (-1)
    expected = {1, 1, -1};
    EXPECT_TRUE(TensorUtils::validateShape(actual, expected));
    
    // Test mismatch
    expected = {1, 1, 160};
    EXPECT_FALSE(TensorUtils::validateShape(actual, expected));
}

TEST(InferenceEngineTest, TensorUtilsCalculateElements) {
    std::vector<int64_t> shape = {1, 1, 320};
    EXPECT_EQ(TensorUtils::calculateElements(shape), 320);
    
    shape = {2, 3, 4};
    EXPECT_EQ(TensorUtils::calculateElements(shape), 24);
}

TEST(InferenceEngineTest, TensorUtilsInterleaveDeinterleave) {
    const size_t frames = 100;
    const size_t channels = 2;
    
    std::vector<float> planar_data[2];
    planar_data[0].resize(frames);
    planar_data[1].resize(frames);
    
    for (size_t i = 0; i < frames; ++i) {
        planar_data[0][i] = static_cast<float>(i);
        planar_data[1][i] = static_cast<float>(i) * 2.0f;
    }
    
    std::vector<float> interleaved(frames * channels);
    TensorUtils::interleave(planar_data, interleaved.data(), frames, channels);
    
    std::vector<float> deinterleaved_data[2];
    deinterleaved_data[0].resize(frames);
    deinterleaved_data[1].resize(frames);
    TensorUtils::deinterleave(interleaved.data(), deinterleaved_data, frames, channels);
    
    for (size_t i = 0; i < frames; ++i) {
        EXPECT_FLOAT_EQ(deinterleaved_data[0][i], static_cast<float>(i));
        EXPECT_FLOAT_EQ(deinterleaved_data[1][i], static_cast<float>(i) * 2.0f);
    }
}

TEST(InferenceEngineTest, OnnxEngineWithTestModel) {
    // This test requires a test model to be present
    std::string model_path = "models/test_identity.onnx";
    if (!std::filesystem::exists(model_path)) {
        GTEST_SKIP() << "Test model not found at " << model_path;
    }
    
    auto engine = createInferenceEngine("onnx");
    InferenceConfig config;
    config.model_path = model_path;
    config.execution_provider = "CPU";
    
    EXPECT_TRUE(engine->initialize(config)) << engine->getLastError();
    EXPECT_TRUE(engine->isReady());
    
    // Check model manifest
    auto manifest = engine->getManifest();
    EXPECT_EQ(manifest.format, "onnx");
    EXPECT_GT(manifest.sample_rate, 0);
    
    // Check input/output infos
    auto input_infos = engine->getInputInfos();
    auto output_infos = engine->getOutputInfos();
    EXPECT_GT(input_infos.size(), 0);
    EXPECT_GT(output_infos.size(), 0);
    
    // Test warmup
    EXPECT_TRUE(engine->warmup());
    
    // Test inference with dummy data
    std::vector<float> input_data(320, 0.5f);
    std::vector<float> output_data(320, 0.0f);
    
    std::vector<const float*> inputs = {input_data.data()};
    std::vector<float*> outputs = {output_data.data()};
    
    auto result = engine->run(inputs, outputs);
    EXPECT_TRUE(result.success) << result.error;
    EXPECT_GT(result.inference_time_ms, 0.0);
    
    // For identity model, output should equal input
    for (size_t i = 0; i < output_data.size(); ++i) {
        EXPECT_NEAR(output_data[i], 0.5f, 0.001f);
    }
}