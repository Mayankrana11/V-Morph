#include "WasapiDevice.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <ks.h>
#include <ksmedia.h>
#include <vector>
#include <string>
#include <memory>
#include <combaseapi.h>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>

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

static std::string WideToUtf8(LPCWSTR wstr) {
    if (!wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string str(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], len, nullptr, nullptr);
    if (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
}

static std::wstring Utf8ToWide(const std::string& str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], wlen);
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
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
            std::string id = WideToUtf8(id_str);
            CoTaskMemFree(id_str);

            IPropertyStore* props = nullptr;
            hr = device->OpenPropertyStore(STGM_READ, &props);
            std::string name = "Unknown";
            if (SUCCEEDED(hr)) {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
                if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                    name = WideToUtf8(pv.pwszVal);
                }
                PropVariantClear(&pv);
                props->Release();
            }

            IAudioClient* client = nullptr;
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(&client));
            if (SUCCEEDED(hr)) {
                WAVEFORMATEX* mix_format = nullptr;
                hr = client->GetMixFormat(&mix_format);
                if (SUCCEEDED(hr)) {
                    AudioDeviceInfo info;
                    info.id = id;
                    info.name = name;
                    info.supported_sample_rates.push_back(mix_format->nSamplesPerSec);
                    info.supported_channel_counts.push_back(mix_format->nChannels);
                    CoTaskMemFree(mix_format);

                    IMMEndpoint* endpoint = nullptr;
                    hr = device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast<void**>(&endpoint));
                    if (SUCCEEDED(hr)) {
                        EDataFlow data_flow;
                        hr = endpoint->GetDataFlow(&data_flow);
                        if (SUCCEEDED(hr)) {
                            info.is_input = (data_flow == eCapture);
                        }
                        endpoint->Release();
                    }

                    devices.push_back(info);
                }
                client->Release();
            }
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
        std::string id = WideToUtf8(id_str);
        CoTaskMemFree(id_str);

        IPropertyStore* props = nullptr;
        hr = device->OpenPropertyStore(STGM_READ, &props);
        std::string name = "Default Input";
        if (SUCCEEDED(hr)) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
            if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                name = WideToUtf8(pv.pwszVal);
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
        std::string id = WideToUtf8(id_str);
        CoTaskMemFree(id_str);

        IPropertyStore* props = nullptr;
        hr = device->OpenPropertyStore(STGM_READ, &props);
        std::string name = "Default Output";
        if (SUCCEEDED(hr)) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
            if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                name = WideToUtf8(pv.pwszVal);
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
    std::wstring wid = Utf8ToWide(device_id);
    IMMDevice* device = nullptr;
    HRESULT hr = pimpl_->enumerator_->GetDevice(wid.c_str(), &device);
    if (SUCCEEDED(hr)) {
        device->Release();
        return true;
    }
    return false;
}

// =====================================================================
// WasapiAudioEngine Implementation (Real Exclusive Mode)
// =====================================================================

class WasapiAudioEngine::Impl {
public:
    Impl() 
        : state_(AudioState::Stopped)
        , callback_(nullptr)
        , capture_client_(nullptr)
        , render_client_(nullptr)
        , capture_device_(nullptr)
        , render_device_(nullptr)
        , capture_audio_client_(nullptr)
        , render_audio_client_(nullptr)
        , capture_event_(nullptr)
        , render_event_(nullptr)
        , enumerator_(nullptr)
        , running_(false)
        , capture_thread_active_(false)
        , render_thread_active_(false)
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator),
                                      reinterpret_cast<void**>(&enumerator_));
        if (FAILED(hr)) {
            enumerator_ = nullptr;
        }
    }

    ~Impl() {
        stop();
        if (enumerator_) {
            enumerator_->Release();
        }
        CoUninitialize();
    }

    ~Impl() {
        stop();
    }

    AudioState state_;
    AudioFormat format_;
    AudioStreamConfig config_;
    IAudioCallback* callback_;
    std::string last_error_;
    AudioMetrics metrics_;

    // Capture (input) devices
    IMMDevice* capture_device_;
    IAudioClient* capture_audio_client_;
    IAudioCaptureClient* capture_client_;
    HANDLE capture_event_;

    // Render (output) devices
    IMMDevice* render_device_;
    IAudioClient* render_audio_client_;
    IAudioRenderClient* render_client_;
    HANDLE render_event_;

    // Threading
    std::thread capture_thread_;
    std::thread render_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> capture_thread_active_;
    std::atomic<bool> render_thread_active_;

    // Shared enumerator
    IMMDeviceEnumerator* enumerator_;

    // Buffers for format conversion
    std::vector<float> capture_buffer_;
    std::vector<float> render_buffer_;
    size_t buffer_frames_ = 0;
    int channels_ = 0;

    void setError(const std::string& err) {
        last_error_ = err;
    }

    bool initCaptureDevice() {
        if (config_.input_device_id.empty()) {
            auto default_in = getDefaultInputDevice();
            if (!default_in) {
                setError("No default input device available");
                return false;
            }
            config_.input_device_id = default_in->id;
        }

        std::wstring wid = Utf8ToWide(config_.input_device_id);
        HRESULT hr = enumerator_->GetDevice(wid.c_str(), &capture_device_);
        if (FAILED(hr)) {
            setError("Failed to get capture device: " + std::to_string(hr));
            return false;
        }

        hr = capture_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(&capture_audio_client_));
        if (FAILED(hr)) {
            setError("Failed to activate capture audio client: " + std::to_string(hr));
            return false;
        }

        return true;
    }

    bool initRenderDevice() {
        if (config_.output_device_id.empty()) {
            auto default_out = getDefaultOutputDevice();
            if (!default_out) {
                setError("No default output device available");
                return false;
            }
            config_.output_device_id = default_out->id;
        }

        std::wstring wid = Utf8ToWide(config_.output_device_id);
        HRESULT hr = enumerator_->GetDevice(wid.c_str(), &render_device_);
        if (FAILED(hr)) {
            setError("Failed to get render device: " + std::to_string(hr));
            return false;
        }

        hr = render_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void**>(&render_audio_client_));
        if (FAILED(hr)) {
            setError("Failed to activate render audio client: " + std::to_string(hr));
            return false;
        }

        return true;
    }

    std::optional<AudioDeviceInfo> getDefaultInputDevice() {
        if (!enumerator_) return std::nullopt;

        IMMDevice* device = nullptr;
        HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
        if (FAILED(hr)) return std::nullopt;

        LPWSTR id_str = nullptr;
        hr = device->GetId(&id_str);
        std::optional<AudioDeviceInfo> result = std::nullopt;

        if (SUCCEEDED(hr)) {
            std::string id = WideToUtf8(id_str);
            CoTaskMemFree(id_str);

            IPropertyStore* props = nullptr;
            hr = device->OpenPropertyStore(STGM_READ, &props);
            std::string name = "Default Input";
            if (SUCCEEDED(hr)) {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
                if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                    name = WideToUtf8(pv.pwszVal);
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

    std::optional<AudioDeviceInfo> getDefaultOutputDevice() {
        if (!enumerator_) return std::nullopt;

        IMMDevice* device = nullptr;
        HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr)) return std::nullopt;

        LPWSTR id_str = nullptr;
        hr = device->GetId(&id_str);
        std::optional<AudioDeviceInfo> result = std::nullopt;

        if (SUCCEEDED(hr)) {
            std::string id = WideToUtf8(id_str);
            CoTaskMemFree(id_str);

            IPropertyStore* props = nullptr;
            hr = device->OpenPropertyStore(STGM_READ, &props);
            std::string name = "Default Output";
            if (SUCCEEDED(hr)) {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
                if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                    name = WideToUtf8(pv.pwszVal);
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

    bool setupCaptureFormat() {
        WAVEFORMATEXTENSIBLE wfx = {};
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels = static_cast<WORD>(config_.format.channels);
        wfx.Format.nSamplesPerSec = config_.format.sample_rate;
        wfx.Format.wBitsPerSample = 32;
        wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
        wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.Samples.wValidBitsPerSample = 32;
        wfx.dwChannelMask = (config_.format.channels == 2) ? (KSAUDIO_SPEAKER_STEREO) : (KSAUDIO_SPEAKER_MONO);
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        UINT32 buffer_frames = config_.buffer_frames;
        HRESULT hr = capture_audio_client_->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            0, 0,
            reinterpret_cast<WAVEFORMATEX*>(&wfx),
            nullptr
        );

        if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
            UINT32 aligned_buffer;
            hr = capture_audio_client_->GetBufferSize(&aligned_buffer);
            if (SUCCEEDED(hr)) {
                buffer_frames = aligned_buffer;
                hr = capture_audio_client_->Initialize(
                    AUDCLNT_SHAREMODE_EXCLUSIVE,
                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                    0, 0,
                    reinterpret_cast<WAVEFORMATEX*>(&wfx),
                    nullptr
                );
            }
        }

        if (FAILED(hr)) {
            setError("Failed to initialize capture audio client: " + std::to_string(hr));
            return false;
        }

        hr = capture_audio_client_->GetBufferSize(&buffer_frames_);
        if (FAILED(hr)) {
            setError("Failed to get capture buffer size: " + std::to_string(hr));
            return false;
        }

        hr = capture_audio_client_->GetService(__uuidof(IAudioCaptureClient),
                                               reinterpret_cast<void**>(&capture_client_));
        if (FAILED(hr)) {
            setError("Failed to get capture client: " + std::to_string(hr));
            return false;
        }

        capture_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!capture_event_) {
            setError("Failed to create capture event");
            return false;
        }

        hr = capture_audio_client_->SetEventHandle(capture_event_);
        if (FAILED(hr)) {
            setError("Failed to set capture event handle: " + std::to_string(hr));
            return false;
        }

        channels_ = config_.format.channels;
        capture_buffer_.resize(buffer_frames_ * channels_);
        return true;
    }

    bool setupRenderFormat() {
        WAVEFORMATEXTENSIBLE wfx = {};
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels = static_cast<WORD>(config_.format.channels);
        wfx.Format.nSamplesPerSec = config_.format.sample_rate;
        wfx.Format.wBitsPerSample = 32;
        wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
        wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.Samples.wValidBitsPerSample = 32;
        wfx.dwChannelMask = (config_.format.channels == 2) ? (KSAUDIO_SPEAKER_STEREO) : (KSAUDIO_SPEAKER_MONO);
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        UINT32 buffer_frames = config_.buffer_frames;
        HRESULT hr = render_audio_client_->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            0, 0,
            reinterpret_cast<WAVEFORMATEX*>(&wfx),
            nullptr
        );

        if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
            UINT32 aligned_buffer;
            hr = render_audio_client_->GetBufferSize(&aligned_buffer);
            if (SUCCEEDED(hr)) {
                buffer_frames = aligned_buffer;
                hr = render_audio_client_->Initialize(
                    AUDCLNT_SHAREMODE_EXCLUSIVE,
                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                    0, 0,
                    reinterpret_cast<WAVEFORMATEX*>(&wfx),
                    nullptr
                );
            }
        }

        if (FAILED(hr)) {
            setError("Failed to initialize render audio client: " + std::to_string(hr));
            return false;
        }

        hr = render_audio_client_->GetBufferSize(&buffer_frames_);
        if (FAILED(hr)) {
            setError("Failed to get render buffer size: " + std::to_string(hr));
            return false;
        }

        hr = render_audio_client_->GetService(__uuidof(IAudioRenderClient),
                                              reinterpret_cast<void**>(&render_client_));
        if (FAILED(hr)) {
            setError("Failed to get render client: " + std::to_string(hr));
            return false;
        }

        render_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!render_event_) {
            setError("Failed to create render event");
            return false;
        }

        hr = render_audio_client_->SetEventHandle(render_event_);
        if (FAILED(hr)) {
            setError("Failed to set render event handle: " + std::to_string(hr));
            return false;
        }

        render_buffer_.resize(buffer_frames_ * channels_);
        return true;
    }

    void captureThreadFunc() {
        capture_thread_active_.store(true);
        HANDLE wait_handles[] = { capture_event_ };

        // Prime the capture buffer
        capture_audio_client_->Start();

        while (running_.load()) {
            DWORD wait_result = WaitForSingleObject(capture_event_, 100);
            if (wait_result != WAIT_OBJECT_0) {
                continue;
            }

            if (!running_.load()) break;

            UINT32 padding = 0;
            HRESULT hr = capture_audio_client_->GetCurrentPadding(&padding);
            if (FAILED(hr) || padding == 0) continue;

            UINT32 frames_to_read = padding;
            if (frames_to_read > buffer_frames_) frames_to_read = static_cast<UINT32>(buffer_frames_);

            BYTE* data_ptr = nullptr;
            UINT32 packet_length = 0;
            DWORD flags = 0;
            hr = capture_client_->GetBuffer(&data_ptr, &frames_to_read, &flags, nullptr, nullptr);
            if (FAILED(hr) || frames_to_read == 0) continue;

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                std::fill(capture_buffer_.begin(), capture_buffer_.begin() + frames_to_read * channels_, 0.0f);
            } else {
                const float* src = reinterpret_cast<const float*>(data_ptr);
                for (UINT32 i = 0; i < frames_to_read * channels_; ++i) {
                    capture_buffer_[i] = src[i];
                }
            }

            hr = capture_client_->ReleaseBuffer(frames_to_read);
            if (FAILED(hr)) continue;

            if (callback_ && frames_to_read > 0) {
                auto callback_start = std::chrono::high_resolution_clock::now();
                callback_->onAudioProcess(capture_buffer_.data(), render_buffer_.data(), frames_to_read, channels_);
                auto callback_end = std::chrono::high_resolution_clock::now();
                double callback_ms = std::chrono::duration<double, std::milli>(callback_end - callback_start).count();
                metrics_.callback_duration_ms = callback_ms;
                metrics_.callback_count++;
            }
        }

        capture_audio_client_->Stop();
        capture_thread_active_.store(false);
    }

    void renderThreadFunc() {
        render_thread_active_.store(true);
        HANDLE wait_handles[] = { render_event_ };

        // Prime the render buffer with silence
        BYTE* render_data = nullptr;
        UINT32 render_frames = 0;
        render_audio_client_->GetBuffer(buffer_frames_, &render_data);
        if (render_data) {
            std::fill(reinterpret_cast<float*>(render_data), 
                      reinterpret_cast<float*>(render_data) + buffer_frames_ * channels_, 0.0f);
            render_audio_client_->ReleaseBuffer(buffer_frames_, 0);
        }

        render_audio_client_->Start();

        while (running_.load()) {
            DWORD wait_result = WaitForSingleObject(render_event_, 100);
            if (wait_result != WAIT_OBJECT_0) {
                continue;
            }

            if (!running_.load()) break;

            UINT32 padding = 0;
            HRESULT hr = render_audio_client_->GetCurrentPadding(&padding);
            if (FAILED(hr)) continue;

            UINT32 frames_available = buffer_frames_ - padding;
            if (frames_available == 0) continue;

            UINT32 frames_to_write = std::min(frames_available, static_cast<UINT32>(buffer_frames_));

            hr = render_client_->GetBuffer(frames_to_write, &render_data);
            if (FAILED(hr) || !render_data) continue;

            float* dst = reinterpret_cast<float*>(render_data);
            for (UINT32 i = 0; i < frames_to_write * channels_; ++i) {
                dst[i] = render_buffer_[i];
            }

            hr = render_client_->ReleaseBuffer(frames_to_write, 0);
            if (FAILED(hr)) continue;
        }

        render_audio_client_->Stop();
        render_thread_active_.store(false);
    }

    void cleanup() {
        if (capture_client_) { capture_client_->Release(); capture_client_ = nullptr; }
        if (capture_audio_client_) { capture_audio_client_->Release(); capture_audio_client_ = nullptr; }
        if (capture_device_) { capture_device_->Release(); capture_device_ = nullptr; }
        if (capture_event_) { CloseHandle(capture_event_); capture_event_ = nullptr; }

        if (render_client_) { render_client_->Release(); render_client_ = nullptr; }
        if (render_audio_client_) { render_audio_client_->Release(); render_audio_client_ = nullptr; }
        if (render_device_) { render_device_->Release(); render_device_ = nullptr; }
        if (render_event_) { CloseHandle(render_event_); render_event_ = nullptr; }
    }
};

WasapiAudioEngine::WasapiAudioEngine() : pimpl_(std::make_unique<Impl>()) {}
WasapiAudioEngine::~WasapiAudioEngine() = default;

bool WasapiAudioEngine::initialize(const AudioStreamConfig& config, IAudioCallback* callback) {
    pimpl_->config_ = config;
    pimpl_->format_ = config.format;
    pimpl_->callback_ = callback;
    pimpl_->state_ = AudioState::Starting;
    pimpl_->last_error_.clear();

    if (!pimpl_->initCaptureDevice()) return false;
    if (!pimpl_->initRenderDevice()) return false;
    if (!pimpl_->setupCaptureFormat()) return false;
    if (!pimpl_->setupRenderFormat()) return false;

    pimpl_->state_ = AudioState::Stopped;
    return true;
}

bool WasapiAudioEngine::start() {
    if (pimpl_->running_.load()) return true;

    pimpl_->running_.store(true);
    pimpl_->state_ = AudioState::Running;

    if (pimpl_->callback_) {
        pimpl_->callback_->onStreamStart();
    }

    pimpl_->capture_thread_ = std::thread([this]() { pimpl_->captureThreadFunc(); });
    pimpl_->render_thread_ = std::thread([this]() { pimpl_->renderThreadFunc(); });

    // Set thread priorities
    ThreadUtils::setCurrentThreadPriority(ThreadPriority::RealTime);

    return true;
}

bool WasapiAudioEngine::stop() {
    pimpl_->running_.store(false);

    if (pimpl_->capture_thread_.joinable()) {
        pimpl_->capture_thread_.join();
    }
    if (pimpl_->render_thread_.joinable()) {
        pimpl_->render_thread_.join();
    }

    pimpl_->state_ = AudioState::Stopping;
    if (pimpl_->callback_) {
        pimpl_->callback_->onStreamStop();
    }
    pimpl_->state_ = AudioState::Stopped;

    pimpl_->cleanup();
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

// =====================================================================
// VirtualAudio Implementation
// =====================================================================

class VirtualAudio::Impl {
public:
    Impl() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator),
                                      reinterpret_cast<void**>(&enumerator_));
        if (FAILED(hr)) {
            enumerator_ = nullptr;
        }
    }
    ~Impl() {
        if (enumerator_) {
            enumerator_->Release();
        }
        CoUninitialize();
    }

    IMMDeviceEnumerator* enumerator_ = nullptr;
};

VirtualAudio::VirtualAudio() : pimpl_(std::make_unique<Impl>()) {}
VirtualAudio::~VirtualAudio() = default;

std::vector<AudioDeviceInfo> VirtualAudio::enumerateVirtualDevices() {
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
            std::string id = WideToUtf8(id_str);
            CoTaskMemFree(id_str);

            IPropertyStore* props = nullptr;
            hr = device->OpenPropertyStore(STGM_READ, &props);
            std::string name = "Unknown";
            if (SUCCEEDED(hr)) {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                hr = props->GetValue(PKEY_Device_FriendlyName, &pv);
                if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR) {
                    name = WideToUtf8(pv.pwszVal);
                }
                PropVariantClear(&pv);
                props->Release();
            }

            IMMEndpoint* endpoint = nullptr;
            hr = device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast<void**>(&endpoint));
            bool is_input = false;
            if (SUCCEEDED(hr)) {
                EDataFlow data_flow;
                hr = endpoint->GetDataFlow(&data_flow);
                if (SUCCEEDED(hr)) {
                    is_input = (data_flow == eCapture);
                }
                endpoint->Release();
            }

            std::string name_lower = name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            if (name_lower.find("cable") != std::string::npos ||
                name_lower.find("vb-audio") != std::string::npos ||
                name_lower.find("voicemeeter") != std::string::npos ||
                name_lower.find("virtual") != std::string::npos) {
                AudioDeviceInfo info;
                info.id = id;
                info.name = name;
                info.is_input = is_input;
                devices.push_back(info);
            }
        }
        device->Release();
    }

    collection->Release();
    return devices;
}

bool VirtualAudio::isVirtualDevice(const std::string& device_id) const {
    auto devices = enumerateVirtualDevices();
    return std::any_of(devices.begin(), devices.end(), 
                       [&device_id](const AudioDeviceInfo& d) { return d.id == device_id; });
}

std::optional<AudioDeviceInfo> VirtualAudio::getRecommendedVirtualOutput() {
    auto devices = enumerateVirtualDevices();
    for (const auto& dev : devices) {
        if (!dev.is_input) return dev;
    }
    return std::nullopt;
}

std::optional<AudioDeviceInfo> VirtualAudio::getRecommendedVirtualInput() {
    auto devices = enumerateVirtualDevices();
    for (const auto& dev : devices) {
        if (dev.is_input) return dev;
    }
    return std::nullopt;
}

bool VirtualAudio::installVBCable() {
    // Would require downloading and running installer with admin rights
    return false;
}

bool VirtualAudio::isVBCableInstalled() const {
    return !enumerateVirtualDevices().empty();
}

} // namespace rtvcc