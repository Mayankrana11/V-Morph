#pragma once

#include "AudioDevice.h"
#include <memory>

namespace rtvcc {

class AudioEngine : public IAudioEngine {
public:
    AudioEngine();
    ~AudioEngine() override;

    bool initialize(const AudioStreamConfig& config, IAudioCallback* callback) override;
    bool start() override;
    bool stop() override;
    bool isRunning() const override;
    AudioState getState() const override;
    AudioFormat getFormat() const override;
    AudioMetrics getMetrics() const override;
    std::string getLastError() const override;
    void setCallback(IAudioCallback* callback) override;

    void refreshDevices() override;
    bool handleDeviceChange(const std::string& device_id, bool is_input) override;

    // Audio backend factory
    static std::unique_ptr<IAudioEngine> create(AudioBackend backend = AudioBackend::Default);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rtvcc