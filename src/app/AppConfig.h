#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace rtvcc {

struct AppConfig {
    // Audio settings
    std::string input_device_id;
    std::string output_device_id;
    int sample_rate = 48000;
    int buffer_frames = 128;
    bool exclusive_mode = true;

    // Voice converter settings
    std::string converter_type = "passthrough";
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

    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;
};

} // namespace rtvcc