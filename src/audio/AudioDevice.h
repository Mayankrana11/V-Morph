#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace rtvcc {

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    bool is_input;
    bool is_default;
    std::vector<int> supported_sample_rates;
    std::vector<int> supported_channel_counts;
};

struct AudioFormat {
    int sample_rate = 48000;
    int channels = 1;
    int bits_per_sample = 32;  // 32-bit float
};

struct AudioStreamConfig {
    AudioFormat format;
    int buffer_frames = 128;  // frames per callback
    std::string input_device_id;
    std::string output_device_id;
    bool exclusive_mode = true;
    bool low_latency = true;
};

enum class AudioBackend {
    Default,
    WASAPI,
    ASIO,
    CoreAudio,
    ALSA,
    PulseAudio,
    RtAudio
};

enum class AudioState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Error
};

struct AudioMetrics {
    // Latency measurements (in seconds)
    double input_latency = 0.0;
    double output_latency = 0.0;
    double processing_latency = 0.0;
    double total_latency = 0.0;

    // Buffer health
    size_t input_queue_depth = 0;
    size_t output_queue_depth = 0;
    uint64_t underruns = 0;
    uint64_t overruns = 0;

    // Callback timing
    double callback_duration_ms = 0.0;
    double callback_deadline_ms = 0.0;
    uint64_t callback_count = 0;
    uint64_t callback_deadline_misses = 0;

    // Processing thread
    double processing_duration_ms = 0.0;
    uint64_t processing_count = 0;

    std::chrono::steady_clock::time_point last_update;
};

class IAudioCallback {
public:
    virtual ~IAudioCallback() = default;

    // Called on the real-time audio thread
    // input:  interleaved float samples [frames * channels]
    // output: interleaved float samples [frames * channels]
    // frames: number of frames to process
    // Must not allocate, lock, block, or call virtual functions
    virtual void onAudioProcess(
        const float* input,
        float* output,
        size_t frames,
        size_t channels
    ) = 0;

    // Called when stream starts/stops (not real-time)
    virtual void onStreamStart() {}
    virtual void onStreamStop() {}
    virtual void onError(const std::string& error) {}
};

class IAudioDeviceManager {
public:
    virtual ~IAudioDeviceManager() = default;

    virtual std::vector<AudioDeviceInfo> enumerateDevices() = 0;
    virtual std::optional<AudioDeviceInfo> getDefaultInputDevice() = 0;
    virtual std::optional<AudioDeviceInfo> getDefaultOutputDevice() = 0;
    virtual bool isDeviceAvailable(const std::string& device_id) = 0;
};

class IAudioEngine {
public:
    virtual ~IAudioEngine() = default;

    virtual bool initialize(const AudioStreamConfig& config, IAudioCallback* callback) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isRunning() const = 0;
    virtual AudioState getState() const = 0;
    virtual AudioFormat getFormat() const = 0;
    virtual AudioMetrics getMetrics() const = 0;
    virtual std::string getLastError() const = 0;
    virtual void setCallback(IAudioCallback* callback) = 0;

    // Device hot-plug handling
    virtual void refreshDevices() = 0;
    virtual bool handleDeviceChange(const std::string& device_id, bool is_input) = 0;
};

} // namespace rtvcc