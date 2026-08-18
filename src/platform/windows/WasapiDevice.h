#pragma once

#include "../../audio/AudioDevice.h"
#include <vector>
#include <string>
#include <memory>

namespace rtvcc {

// WASAPI device implementation
class WasapiDeviceManager : public IAudioDeviceManager {
public:
    WasapiDeviceManager();
    ~WasapiDeviceManager() override;

    std::vector<AudioDeviceInfo> enumerateDevices() override;
    std::optional<AudioDeviceInfo> getDefaultInputDevice() override;
    std::optional<AudioDeviceInfo> getDefaultOutputDevice() override;
    bool isDeviceAvailable(const std::string& device_id) override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// WASAPI audio engine implementation
class WasapiAudioEngine : public IAudioEngine {
public:
    WasapiAudioEngine();
    ~WasapiAudioEngine() override;

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

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Virtual audio device integration (VB-Cable, etc.)
class VirtualAudio {
public:
    VirtualAudio();
    ~VirtualAudio();

    // List available virtual audio devices
    std::vector<AudioDeviceInfo> enumerateVirtualDevices();

    // Check if a device is a virtual audio device
    bool isVirtualDevice(const std::string& device_id) const;

    // Get recommended virtual output device for routing
    std::optional<AudioDeviceInfo> getRecommendedVirtualOutput();

    // Get recommended virtual input device for capture
    std::optional<AudioDeviceInfo> getRecommendedVirtualInput();

    // Install VB-Cable if not present (requires admin)
    bool installVBCable();

    // Check if VB-Cable is installed
    bool isVBCableInstalled() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rtvcc