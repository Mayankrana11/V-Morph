#include "AudioDevice.h"

namespace rtvcc {

AudioDeviceInfo::AudioDeviceInfo() = default;
AudioDeviceInfo::~AudioDeviceInfo() = default;

AudioFormat::AudioFormat() = default;
AudioFormat::~AudioFormat() = default;

AudioStreamConfig::AudioStreamConfig() = default;
AudioStreamConfig::~AudioStreamConfig() = default;

IAudioCallback::~IAudioCallback() = default;
IAudioDeviceManager::~IAudioDeviceManager() = default;
IAudioEngine::~IAudioEngine() = default;

} // namespace rtvcc