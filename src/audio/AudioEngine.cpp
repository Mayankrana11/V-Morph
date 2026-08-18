#include "AudioEngine.h"
#include <memory>

namespace rtvcc {

class AudioEngine::Impl {
public:
    Impl() : state_(AudioState::Stopped) {}
    ~Impl() = default;

    AudioState state_;
    AudioFormat format_;
    AudioStreamConfig config_;
    IAudioCallback* callback_ = nullptr;
    std::string last_error_;
    AudioMetrics metrics_;
};

AudioEngine::AudioEngine() : pimpl_(std::make_unique<Impl>()) {}
AudioEngine::~AudioEngine() = default;

bool AudioEngine::initialize(const AudioStreamConfig& config, IAudioCallback* callback) {
    pimpl_->config_ = config;
    pimpl_->format_ = config.format;
    pimpl_->callback_ = callback;
    pimpl_->state_ = AudioState::Starting;
    pimpl_->last_error_.clear();
    return true;
}

bool AudioEngine::start() {
    pimpl_->state_ = AudioState::Running;
    if (pimpl_->callback_) {
        pimpl_->callback_->onStreamStart();
    }
    return true;
}

bool AudioEngine::stop() {
    pimpl_->state_ = AudioState::Stopping;
    if (pimpl_->callback_) {
        pimpl_->callback_->onStreamStop();
    }
    pimpl_->state_ = AudioState::Stopped;
    return true;
}

bool AudioEngine::isRunning() const {
    return pimpl_->state_ == AudioState::Running;
}

AudioState AudioEngine::getState() const {
    return pimpl_->state_;
}

AudioFormat AudioEngine::getFormat() const {
    return pimpl_->format_;
}

AudioMetrics AudioEngine::getMetrics() const {
    return pimpl_->metrics_;
}

std::string AudioEngine::getLastError() const {
    return pimpl_->last_error_;
}

void AudioEngine::setCallback(IAudioCallback* callback) {
    pimpl_->callback_ = callback;
}

void AudioEngine::refreshDevices() {
    // Platform-specific implementation
}

bool AudioEngine::handleDeviceChange(const std::string& device_id, bool is_input) {
    // Platform-specific implementation
    return false;
}

std::unique_ptr<IAudioEngine> AudioEngine::create(AudioBackend backend) {
    if (backend == AudioBackend::WASAPI || backend == AudioBackend::Default) {
#ifdef _WIN32
        return std::make_unique<WasapiAudioEngine>();
#else
        return std::make_unique<AudioEngine>();
#endif
    }
    return std::make_unique<AudioEngine>();
}

} // namespace rtvcc