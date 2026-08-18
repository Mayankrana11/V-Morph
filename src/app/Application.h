#pragma once

#include <string>
#include <memory>
#include "audio/AudioEngine.h"
#include "vc/VoiceConverter.h"
#include "metrics/PerformanceMonitor.h"

namespace rtvcc {

struct AppConfig {
    // Audio settings
    std::string input_device_id;
    std::string output_device_id;
    int sample_rate = 48000;
    int buffer_frames = 128;
    bool exclusive_mode = true;

    // Voice converter settings
    std::string converter_type = "passthrough";  // "passthrough", "dsp", "onnx"
    std::string model_path;
    std::string target_voice;
    float pitch_shift = 0.0f;
    float formant_shift = 1.0f;
    float mix = 1.0f;
    float output_gain_db = 0.0f;
    int chunk_size_ms = 20;

    // DSP settings
    float input_gain_db = 0.0f;
    float highpass_cutoff_hz = 80.0f;
    float limiter_threshold_db = -1.0f;
    float limiter_release_ms = 50.0f;
    bool enable_highpass = true;
    bool enable_limiter = true;

    // Performance
    std::string execution_provider = "CPU";
    int inference_threads = 1;

    // Runtime
    bool auto_start = false;
    bool bypass = false;
    bool show_performance = true;

    // Paths
    std::string config_dir;
    std::string models_dir;
    std::string logs_dir;
};

class Application {
public:
    Application();
    ~Application();

    // Initialize application
    bool initialize(const AppConfig& config);
    bool initializeFromFile(const std::string& config_path);
    bool saveConfig(const std::string& config_path) const;

    // Run main loop (CLI mode)
    int run(int argc, char* argv[]);

    // Run GUI mode
    int runGui();

    // Audio control
    bool startAudio();
    bool stopAudio();
    bool isAudioRunning() const;
    void setBypass(bool bypass);
    bool getBypass() const;

    // Voice converter control
    bool loadConverter(const std::string& type, const std::string& model_path = "");
    bool unloadConverter();
    std::string getCurrentConverter() const;

    // Configuration
    const AppConfig& getConfig() const { return config_; }
    void setConfig(const AppConfig& config);

    // Performance metrics
    const PerformanceMonitor& getPerformanceMonitor() const;

    // Diagnostics
    void printDiagnostics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    AppConfig config_;
};

} // namespace rtvcc