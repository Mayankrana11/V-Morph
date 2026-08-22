#include <gtest/gtest.h>
#include "app/Application.h"
#include "audio/AudioEngine.h"
#include "vc/PassthroughConverter.h"
#include "vc/DSPVoiceConverter.h"
#include "vc/OnnxVoiceConverter.h"
#include "metrics/PerformanceMonitor.h"
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace rtvcc;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.sample_rate = 48000;
        config_.buffer_frames = 128;
        config_.converter_type = "passthrough";
        config_.exclusive_mode = false;  // Use shared mode for testing
        
        app_ = std::make_unique<Application>();
    }
    
    void TearDown() override {
        if (app_) {
            app_->stopAudio();
        }
    }
    
    AppConfig config_;
    std::unique_ptr<Application> app_;
};

TEST_F(IntegrationTest, ApplicationInitializePassthrough) {
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_TRUE(app_->isAudioRunning() == false);  // Not started yet
}

TEST_F(IntegrationTest, ApplicationStartStop) {
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_TRUE(app_->startAudio());
    EXPECT_TRUE(app_->isAudioRunning());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(app_->stopAudio());
    EXPECT_FALSE(app_->isAudioRunning());
}

TEST_F(IntegrationTest, ApplicationBypassMode) {
    config_.bypass = true;
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_TRUE(app_->startAudio());
    
    EXPECT_TRUE(app_->getBypass());
    app_->setBypass(false);
    EXPECT_FALSE(app_->getBypass());
    
    app_->stopAudio();
}

TEST_F(IntegrationTest, ConverterSwitching) {
    EXPECT_TRUE(app_->initialize(config_));
    
    // Switch to DSP
    EXPECT_TRUE(app_->loadConverter("dsp", ""));
    EXPECT_EQ(app_->getCurrentConverter(), "DSP Effects");
    
    // Switch back to passthrough
    EXPECT_TRUE(app_->loadConverter("passthrough", ""));
    EXPECT_EQ(app_->getCurrentConverter(), "Passthrough");
}

TEST_F(IntegrationTest, AudioPipelinePassthrough) {
    config_.converter_type = "passthrough";
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_TRUE(app_->startAudio());
    
    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto snapshot = app_->getPerformanceMonitor().getSnapshot();
    EXPECT_GT(snapshot.callback_count, 0);
    
    app_->stopAudio();
}

TEST_F(IntegrationTest, DSPConverterPipeline) {
    config_.converter_type = "dsp";
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_TRUE(app_->startAudio());
    
    // Test DSP controls
    app_->getConfig().input_gain_db = 6.0f;
    app_->getConfig().enable_highpass = true;
    app_->getConfig().enable_limiter = true;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto snapshot = app_->getPerformanceMonitor().getSnapshot();
    EXPECT_GT(snapshot.callback_count, 0);
    
    app_->stopAudio();
}

TEST_F(IntegrationTest, PerformanceMonitoring) {
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_TRUE(app_->startAudio());
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto snapshot = app_->getPerformanceMonitor().getSnapshot();
    EXPECT_GT(snapshot.callback_count, 0);
    EXPECT_GT(snapshot.processing_count, 0);
    EXPECT_EQ(snapshot.underruns, 0);
    EXPECT_EQ(snapshot.overruns, 0);
    
    app_->stopAudio();
}

TEST_F(IntegrationTest, DiagnosticsOutput) {
    EXPECT_TRUE(app_->initialize(config_));
    
    // Should not crash
    app_->printDiagnostics();
    
    auto devices = app_->initialize(config_) ? app_->getPerformanceMonitor().getSnapshot() : PerformanceMonitor::Snapshot{};
    EXPECT_GE(devices.callback_count, 0);
}

// Test with ONNX converter if model exists
TEST_F(IntegrationTest, OnnxConverterIfAvailable) {
    std::string model_path = "models/test_identity.onnx";
    if (!std::filesystem::exists(model_path)) {
        GTEST_SKIP() << "Test model not found";
    }
    
    config_.converter_type = "onnx";
    config_.model_path = model_path;
    config_.execution_provider = "CPU";
    
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_EQ(app_->getCurrentConverter(), "OnnxVoiceConverter");
    
    EXPECT_TRUE(app_->startAudio());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto snapshot = app_->getPerformanceMonitor().getSnapshot();
    EXPECT_GT(snapshot.inference_count, 0);
    EXPECT_GT(snapshot.inference_avg_ms, 0.0);
    
    app_->stopAudio();
}

// Stress test - run for longer duration
TEST_F(IntegrationTest, StressTestShort) {
    EXPECT_TRUE(app_->initialize(config_));
    EXPECT_TRUE(app_->startAudio());
    
    // Run for 2 seconds
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto snapshot = app_->getPerformanceMonitor().getSnapshot();
        // Should not have excessive underruns/overruns
        EXPECT_LT(snapshot.underruns, 100);
        EXPECT_LT(snapshot.overruns, 100);
    }
    
    app_->stopAudio();
    
    auto final_snapshot = app_->getPerformanceMonitor().getSnapshot();
    EXPECT_GT(final_snapshot.callback_count, 100);  // Should have processed many callbacks
}

// Test config save/load
TEST_F(IntegrationTest, ConfigPersistence) {
    config_.sample_rate = 48000;
    config_.buffer_frames = 256;
    config_.converter_type = "dsp";
    config_.input_gain_db = 3.0f;
    
    // Save
    std::string config_file = "test_config.json";
    EXPECT_TRUE(app_->saveConfig(config_file));
    
    // Load into new app
    auto app2 = std::make_unique<Application>();
    EXPECT_TRUE(app2->initializeFromFile(config_file));
    
    // Verify values
    const auto& loaded = app2->getConfig();
    EXPECT_EQ(loaded.sample_rate, 48000);
    EXPECT_EQ(loaded.buffer_frames, 256);
    EXPECT_EQ(loaded.converter_type, "dsp");
    EXPECT_FLOAT_EQ(loaded.input_gain_db, 3.0f);
    
    // Cleanup
    std::filesystem::remove(config_file);
}