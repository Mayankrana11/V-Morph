#include "AppConfig.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace rtvcc {

bool AppConfig::loadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        // Audio
        input_device_id = j.value("input_device_id", "");
        output_device_id = j.value("output_device_id", "");
        sample_rate = j.value("sample_rate", 48000);
        buffer_frames = j.value("buffer_frames", 128);
        exclusive_mode = j.value("exclusive_mode", true);

        // Virtual audio (support both flat and nested)
        auto virtual_audio = j.value("virtual_audio", nlohmann::json::object());
        use_virtual_output = virtual_audio.value("use_virtual_output", j.value("use_virtual_output", false));
        virtual_output_device_id = virtual_audio.value("virtual_output_device_id", j.value("virtual_output_device_id", ""));

        // Voice converter
        converter_type = j.value("converter_type", "passthrough");
        model_path = j.value("model_path", "");
        target_voice = j.value("target_voice", "");
        pitch_shift = j.value("pitch_shift", 0.0f);
        formant_shift = j.value("formant_shift", 1.0f);
        mix = j.value("mix", 1.0f);
        output_gain_db = j.value("output_gain_db", 0.0f);
        chunk_size_ms = j.value("chunk_size_ms", 20);

        // DSP
        input_gain_db = j.value("input_gain_db", 0.0f);
        highpass_cutoff_hz = j.value("highpass_cutoff_hz", 80.0f);
        limiter_threshold_db = j.value("limiter_threshold_db", -1.0f);
        limiter_release_ms = j.value("limiter_release_ms", 50.0f);
        enable_highpass = j.value("enable_highpass", true);
        enable_limiter = j.value("enable_limiter", true);

        // Performance
        execution_provider = j.value("execution_provider", "CPU");
        inference_threads = j.value("inference_threads", 1);

        // Runtime
        auto_start = j.value("auto_start", false);
        bypass = j.value("bypass", false);
        show_performance = j.value("show_performance", true);

        // Paths
        config_dir = j.value("config_dir", "");
        models_dir = j.value("models_dir", "");
        logs_dir = j.value("logs_dir", "");

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool AppConfig::saveToFile(const std::string& path) const {
    try {
        nlohmann::json j;

        j["input_device_id"] = input_device_id;
        j["output_device_id"] = output_device_id;
        j["sample_rate"] = sample_rate;
        j["buffer_frames"] = buffer_frames;
        j["exclusive_mode"] = exclusive_mode;

        // Virtual audio (nested)
        j["virtual_audio"]["use_virtual_output"] = use_virtual_output;
        j["virtual_audio"]["virtual_output_device_id"] = virtual_output_device_id;

        j["converter_type"] = converter_type;
        j["model_path"] = model_path;
        j["target_voice"] = target_voice;
        j["pitch_shift"] = pitch_shift;
        j["formant_shift"] = formant_shift;
        j["mix"] = mix;
        j["output_gain_db"] = output_gain_db;
        j["chunk_size_ms"] = chunk_size_ms;

        j["input_gain_db"] = input_gain_db;
        j["highpass_cutoff_hz"] = highpass_cutoff_hz;
        j["limiter_threshold_db"] = limiter_threshold_db;
        j["limiter_release_ms"] = limiter_release_ms;
        j["enable_highpass"] = enable_highpass;
        j["enable_limiter"] = enable_limiter;

        j["execution_provider"] = execution_provider;
        j["inference_threads"] = inference_threads;

        j["auto_start"] = auto_start;
        j["bypass"] = bypass;
        j["show_performance"] = show_performance;

        j["config_dir"] = config_dir;
        j["models_dir"] = models_dir;
        j["logs_dir"] = logs_dir;

        std::ofstream file(path);
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace rtvcc