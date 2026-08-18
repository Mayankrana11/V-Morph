#include "WasapiDevice.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <string>
#include <memory>
#include <combaseapi.h>

namespace rtvcc {

// =====================================================================
// WasapiDeviceManager Implementation
// =====================================================================

class WasapiDeviceManager::Impl {
public:
    Impl() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    ~Impl() {
        CoUninitialize();
    }

    IMMDeviceEnumerator* enumerator_ = nullptr;
};

WasapiDeviceManager::WasapiDeviceManager() : pimpl_(std::make_unique<Impl>()) {
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(&pimpl_->enumerator_));
    if (FAILED(hr)) {
        pimpl_->enumerator_ = nullptr;
    }
}

WasapiDeviceManager::~WasapiDeviceManager() {
    if (pimpl_->enumerator_) {
        pimpl_->enumerator_->Release();
    }
}

std::vector<AudioDeviceInfo> WasapiDeviceManager::enumerateDevices() {
    std::vector<AudioDeviceInfo> devices;
    if (!pimpl_->enumerator_) return devices;

    IMMDeviceCollection* collection = nullptr;
    HRESULT hr = pimpl_->enumerator_->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) return devices;

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        hr = collection->Item(i, &device);
        if (FAILED(hr)) continue;

        LPWSTR id_str = nullptr;
        hr = device->GetId(&id_str);
        if (SUCCEEDED(hr)) {
            // Convert to string
            int len = WideCharToMultiByte(CP_UTF8, 0, id_str, -1, nullptr, 0, nullptr, nullptr);
            std::string id(len, 0);
            WideCharToMultiByte(CP_UTF8, 0, id_str, -1, &id[0], len, nullptr, nullptr);
            CoTaskMemFree(id_str);

            // Get properties
            IPropertyStore* props = nullptr;
            hr = device->OpenPropertyStore(STGM_READ, &props);
            std::string name = "Unknown";
            if (SUCCEEDED(hr)) {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
                if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                    int nlen = WideCharToMultiByte(CP_UTF8, 0, pv.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                    name.resize(nlen);
                    WideCharToMultiByte(CP_UTF8, 0, pv.pwszVal, -1, &name[0], nlen, nullptr, nullptr);
                }
                PropVariantClear(&pv);
                props->Release();
            }

            // Check if input or output
            AudioDeviceInfo info;
            info.id = id;
            info.name = name;

            // Check data flow
            IAudioClient* client = nullptr;
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(&client));
            if (SUCCEEDED(hr)) {
                WAVEFORMATEX* mix_format = nullptr;
                hr = client->GetMixFormat(&mix_format);
                if (SUCCEEDED(hr)) {
                    info.supported_sample_rates.push_back(mix_format->nSamplesPerSec);
                    info.supported_channel_counts.push_back(mix_format->nChannels);
                    CoTaskMemFree(mix_format);
                }
                client->Release();
            }

            // Determine if input or output by checking endpoint type
            // This is simplified - in reality you'd check the data flow
            info.is_input = (name.find("Microphone") != std::string::npos ||
                            name.find("Input") != std::string::npos ||
                            name.find("Headset") != std::string::npos);

            devices.push_back(info);
        }
        device->Release();
    }

    collection->Release();
    return devices;
}

std::optional<AudioDeviceInfo> WasapiDeviceManager::getDefaultInputDevice() {
    if (!pimpl_->enumerator_) return std::nullopt;

    IMMDevice* device = nullptr;
    HRESULT hr = pimpl_->enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
    if (FAILED(hr)) return std::nullopt;

    LPWSTR id_str = nullptr;
    hr = device->GetId(&id_str);
    std::optional<AudioDeviceInfo> result = std::nullopt;

    if (SUCCEEDED(hr)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, id_str, -1, nullptr, 0, nullptr, nullptr);
        std::string id(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, id_str, -1, &id[0], len, nullptr, nullptr);
        CoTaskMemFree(id_str);

        IPropertyStore* props = nullptr;
        hr = device->OpenPropertyStore(STGM_READ, &props);
        std::string name = "Default Input";
        if (SUCCEEDED(hr)) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
            if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                int nlen = WideCharToMultiByte(CP_UTF8, 0, pv.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                name.resize(nlen);
                WideCharToMultiByte(CP_UTF8, 0, pv.pwszVal, -1, &name[0], nlen, nullptr, nullptr);
            }
            PropVariantClear(&pv);
            props->Release();
        }

        AudioDeviceInfo info;
        info.id = id;
        info.name = name;
        info.is_input = true;
        info.is_default = true;
        result = info;
    }

    device->Release();
    return result;
}

std::optional<AudioDeviceInfo> WasapiDeviceManager::getDefaultOutputDevice() {
    if (!pimpl_->enumerator_) return std::nullopt;

    IMMDevice* device = nullptr;
    HRESULT hr = pimpl_->enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) return std::nullopt;

    LPWSTR id_str = nullptr;
    hr = device->GetId(&id_str);
    std::optional<AudioDeviceInfo> result = std::nullopt;

    if (SUCCEEDED(hr)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, id_str, -1, nullptr, 0, nullptr, nullptr);
        std::string id(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, id_str, -1, &id[0], len, nullptr, nullptr);
        CoTaskMemFree(id_str);

        IPropertyStore* props = nullptr;
        hr = device->OpenPropertyStore(STGM_READ, &props);
        std::string name = "Default Output";
        if (SUCCEEDED(hr)) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
            if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                int nlen = WideCharToMultiByte(CP_UTF8, 0, pv.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                name.resize(nlen);
                WideCharToMultiByte(CP_UTF8, 0, pv.pwszVal, -1, &name[0], nlen, nullptr, nullptr);
            }
            PropVariantClear(&pv);
            props->Release();
        }

        AudioDeviceInfo info;
        info.id = id;
        info.name = name;
        info.is_input = false;
        info.is_default = true;
        result = info;
    }

    device->Release();
    return result;
}

bool WasapiDeviceManager::isDeviceAvailable(const std::string& device_id) {
    // Convert to wide string and check
    int wlen = MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, nullptr, 0);
    std::wstring wid(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, &wid[0], wlen);

    IMMDevice* device = nullptr;
    HRESULT hr = pimpl_->enumerator_->GetDevice(wid.c_str(), &device);
    if (SUCCEEDED(hr)) {
        device->Release();
        return true;
    }
    return false;
}

// =====================================================================
// WasapiAudioEngine Implementation
// =====================================================================

class WasapiAudioEngine::Impl {
public:
    Impl() : state_(AudioState::Stopped), callback_(nullptr), client_(nullptr), render_client_(nullptr), capture_client_(nullptr) {}
    ~Impl() {
        stop();
    }

    AudioState state_;
    AudioFormat format_;
    AudioStreamConfig config_;
    IAudioCallback* callback_;
    std::string last_error_;
    AudioMetrics metrics_;

    // WASAPI objects
    IAudioClient* client_;
    IAudioRenderClient* render_client_;
    IAudioCaptureClient* capture_client_;
    HANDLE event_handle_;

    // Audio thread
    std::thread audio_thread_;
    std::atomic<bool> running_{false};
};

WasapiAudioEngine::WasapiAudioEngine() : pimpl_(std::make_unique<Impl>()) {}
WasapiAudioEngine::~WasapiAudioEngine() = default;

bool WasapiAudioEngine::initialize(const AudioStreamConfig& config, IAudioCallback* callback) {
    pimpl_->config_ = config;
    pimpl_->format_ = config.format;
    pimpl_->callback_ = callback;
    pimpl_->state_ = AudioState::Starting;
    pimpl_->last_error_.clear();

    // TODO: Full WASAPI initialization
    // This is a stub - real implementation would:
    // 1. Get device by ID
    // 2. Activate IAudioClient
    // 3. Set format (shared or exclusive mode)
    // 4. Initialize client with buffer size
    // 5. Get render/capture clients
    // 6. Set event handle for callback

    return true;
}

bool WasapiAudioEngine::start() {
    if (pimpl_->running_.load()) return true;

    pimpl_->running_.store(true);
    pimpl_->state_ = AudioState::Running;

    if (pimpl_->callback_) {
        pimpl_->callback_->onStreamStart();
    }

    // Start audio thread
    pimpl_->audio_thread_ = std::thread([this]() { audioThreadFunc(); });

    return true;
}

bool WasapiAudioEngine::stop() {
    pimpl_->running_.store(false);

    if (pimpl_->audio_thread_.joinable()) {
        pimpl_->audio_thread_.join();
    }

    pimpl_->state_ = AudioState::Stopping;
    if (pimpl_->callback_) {
        pimpl_->callback_->onStreamStop();
    }
    pimpl_->state_ = AudioState::Stopped;

    return true;
}

bool WasapiAudioEngine::isRunning() const {
    return pimpl_->state_ == AudioState::Running;
}

AudioState WasapiAudioEngine::getState() const {
    return pimpl_->state_;
}

AudioFormat WasapiAudioEngine::getFormat() const {
    return pimpl_->format_;
}

AudioMetrics WasapiAudioEngine::getMetrics() const {
    return pimpl_->metrics_;
}

std::string WasapiAudioEngine::getLastError() const {
    return pimpl_->last_error_;
}

void WasapiAudioEngine::setCallback(IAudioCallback* callback) {
    pimpl_->callback_ = callback;
}

void WasapiAudioEngine::refreshDevices() {}
bool WasapiAudioEngine::handleDeviceChange(const std::string&, bool) { return false; }

void WasapiAudioEngine::audioThreadFunc() {
    // Simplified audio thread - real implementation would wait on event handle
    // and call the callback with actual audio data
    const size_t frames_per_callback = pimpl_->config_.buffer_frames;
    const size_t channels = pimpl_->config_.format.channels;

    std::vector<float> input(frames_per_callback * channels);
    std::vector<float> output(frames_per_callback * channels);

    while (pimpl_->running_.load()) {
        // Simulate audio callback timing
        std::this_thread::sleep_for(std::chrono::milliseconds(
            (frames_per_callback * 1000) / pimpl_->config_.format.sample_rate));

        if (pimpl_->callback_) {
            pimpl_->callback_->onAudioProcess(input.data(), output.data(), frames_per_callback, channels);
        }
    }
}

// =====================================================================
// VirtualAudio Implementation
// =====================================================================

class VirtualAudio::Impl {
public:
    Impl() = default;
    ~Impl() = default;
};

VirtualAudio::VirtualAudio() : pimpl_(std::make_unique<Impl>()) {}
VirtualAudio::~VirtualAudio() = default;

std::vector<AudioDeviceInfo> VirtualAudio::enumerateVirtualDevices() {
    std::vector<AudioDeviceInfo> devices;
    // Look for VB-Cable, Voicemeeter, etc.
    return devices;
}

bool VirtualAudio::isVirtualDevice(const std::string& device_id) const {
    // Check if device name contains virtual audio keywords
    return false;
}

std::optional<AudioDeviceInfo> VirtualAudio::getRecommendedVirtualOutput() {
    return std::nullopt;
}

std::optional<AudioDeviceInfo> VirtualAudio::getRecommendedVirtualInput() {
    return std::nullopt;
}

bool VirtualAudio::installVBCable() {
    // Would download and run VB-Cable installer
    return false;
}

bool VirtualAudio::isVBCableInstalled() const {
    return false;
}

} // namespace rtvcc