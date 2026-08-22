#include "MainWindow.h"
#include "app/Application.h"
#include "platform/windows/WasapiDevice.h"
#include "vc/VoiceConverter.h"
#include "metrics/PerformanceMonitor.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>

namespace rtvcc {

struct MainWindow::Impl {
    Impl() : hwnd_(nullptr), device_(nullptr), context_(nullptr), swap_chain_(nullptr),
             render_target_(nullptr), running_(false), app_(nullptr) {}

    ~Impl() {
        shutdown();
    }

    HWND hwnd_;
    ID3D11Device* device_;
    ID3D11DeviceContext* context_;
    IDXGISwapChain* swap_chain_;
    ID3D11RenderTargetView* render_target_;
    bool running_;
    Application* app_;

    // UI State
    bool show_performance_ = true;
    bool show_settings_ = true;
    bool show_about_ = false;
    bool show_device_selector_ = false;
    int selected_input_device_ = -1;
    int selected_output_device_ = -1;
    std::vector<AudioDeviceInfo> input_devices_;
    std::vector<AudioDeviceInfo> output_devices_;
    std::vector<AudioDeviceInfo> virtual_output_devices_;
    VirtualAudio virtual_audio_;

    // Performance graph data
    static constexpr int GRAPH_HISTORY = 120;
    float cpu_history_[GRAPH_HISTORY] = {0};
    float latency_history_[GRAPH_HISTORY] = {0};
    int history_idx_ = 0;

    bool initD3D11(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &sd,
            &swap_chain_, &device_, &feature_level, &context_
        );
        if (FAILED(hr)) return false;

        createRenderTarget();
        return true;
    }

    void createRenderTarget() {
        ID3D11Texture2D* back_buffer = nullptr;
        swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_);
        back_buffer->Release();
    }

    void cleanupRenderTarget() {
        if (render_target_) { render_target_->Release(); render_target_ = nullptr; }
    }

    void shutdownD3D11() {
        cleanupRenderTarget();
        if (swap_chain_) { swap_chain_->Release(); swap_chain_ = nullptr; }
        if (context_) { context_->Release(); context_ = nullptr; }
        if (device_) { device_->Release(); device_ = nullptr; }
    }

    void refreshDevices() {
        WasapiDeviceManager manager;
        auto all_devices = manager.enumerateDevices();
        
        input_devices_.clear();
        output_devices_.clear();
        for (const auto& dev : all_devices) {
            if (dev.is_input) input_devices_.push_back(dev);
            else output_devices_.push_back(dev);
        }

        virtual_output_devices_ = virtual_audio_.enumerateVirtualDevices();
    }

    void updatePerformanceGraph() {
        if (app_) {
            auto snapshot = app_->getPerformanceMonitor().getSnapshot();
            cpu_history_[history_idx_] = static_cast<float>(snapshot.cpu_usage_percent);
            latency_history_[history_idx_] = static_cast<float>(snapshot.callback_avg_ms);
            history_idx_ = (history_idx_ + 1) % GRAPH_HISTORY;
        }
    }

    void renderDeviceSelector(const char* title, const std::vector<AudioDeviceInfo>& devices,
                              int& selected, const std::string& current_id,
                              bool is_input) {
        if (ImGui::BeginCombo(title, selected >= 0 && selected < (int)devices.size() ? 
                              devices[selected].name.c_str() : "Select device...")) {
            for (int i = 0; i < (int)devices.size(); ++i) {
                bool is_selected = (selected == i);
                ImGui::Selectable(devices[i].name.c_str(), &is_selected);
                if (is_selected) {
                    selected = i;
                    if (app_) {
                        if (is_input) {
                            app_->getConfig().input_device_id = devices[i].id;
                        } else {
                            app_->getConfig().output_device_id = devices[i].id;
                        }
                    }
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        
        if (ImGui::Button("Refresh")) {
            refreshDevices();
        }
    }
};

MainWindow::MainWindow() : pimpl_(std::make_unique<Impl>()) {}
MainWindow::~MainWindow() = default;

bool MainWindow::initialize() {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, DefWindowProcW, 0L, 0L,
                       GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                       L"V-Morph", nullptr };
    RegisterClassExW(&wc);

    pimpl_->hwnd_ = CreateWindowW(wc.lpszClassName, L"V-Morph - Real-time Voice Changer",
                                  WS_OVERLAPPEDWINDOW, 100, 100, 1000, 700,
                                  nullptr, nullptr, wc.hInstance, nullptr);

    if (!pimpl_->initD3D11(pimpl_->hwnd_)) {
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(pimpl_->hwnd_);
    ImGui_ImplDX11_Init(pimpl_->device_, pimpl_->context_);

    pimpl_->refreshDevices();
    pimpl_->running_ = true;

    ShowWindow(pimpl_->hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(pimpl_->hwnd_);

    return true;
}

void MainWindow::run() {
    MSG msg = {};
    while (pimpl_->running_ && msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // DockSpace
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        // Main Menu Bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Config")) {
                    if (pimpl_->app_) pimpl_->app_->saveConfig("config.json");
                }
                if (ImGui::MenuItem("Load Config")) {
                    if (pimpl_->app_) pimpl_->app_->initializeFromFile("config.json");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    pimpl_->running_ = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Performance", nullptr, &pimpl_->show_performance_);
                ImGui::MenuItem("Settings", nullptr, &pimpl_->show_settings_);
                ImGui::MenuItem("About", nullptr, &pimpl_->show_about_);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                ImGui::MenuItem("About V-Morph", nullptr, &pimpl_->show_about_);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Audio Panel
        if (pimpl_->show_settings_ && pimpl_->app_) {
            ImGui::Begin("Audio Settings", &pimpl_->show_settings_);
            
            ImGui::Text("Input Device");
            pimpl_->renderDeviceSelector("##InputDevice", pimpl_->input_devices_,
                                         pimpl_->selected_input_device_,
                                         pimpl_->app_->getConfig().input_device_id, true);

            ImGui::Text("Output Device");
            pimpl_->renderDeviceSelector("##OutputDevice", pimpl_->output_devices_,
                                         pimpl_->selected_output_device_,
                                         pimpl_->app_->getConfig().output_device_id, false);

            ImGui::Separator();
            ImGui::Text("Virtual Audio Routing (for Discord/Games)");
            ImGui::Checkbox("Use Virtual Output", &pimpl_->app_->getConfig().use_virtual_output);
            
            if (pimpl_->app_->getConfig().use_virtual_output) {
                ImGui::Indent();
                if (!pimpl_->virtual_output_devices_.empty()) {
                    int virt_idx = -1;
                    for (int i = 0; i < (int)pimpl_->virtual_output_devices_.size(); ++i) {
                        if (pimpl_->virtual_output_devices_[i].id == pimpl_->app_->getConfig().virtual_output_device_id) {
                            virt_idx = i;
                            break;
                        }
                    }
                    pimpl_->renderDeviceSelector("##VirtualOutput", pimpl_->virtual_output_devices_,
                                                 virt_idx,
                                                 pimpl_->app_->getConfig().virtual_output_device_id, false);
                    if (virt_idx >= 0) {
                        pimpl_->app_->getConfig().virtual_output_device_id = pimpl_->virtual_output_devices_[virt_idx].id;
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No virtual audio devices found (VB-Cable, Voicemeeter)");
                    ImGui::Text("Install VB-Cable from: https://vb-audio.com/Cable/");
                }
                ImGui::Unindent();
            }

            ImGui::Separator();
            ImGui::Text("Sample Rate");
            ImGui::Combo("##SampleRate", (int*)&pimpl_->app_->getConfig().sample_rate, "44100\048000\096000\0");
            ImGui::Text("Buffer Size");
            ImGui::Combo("##BufferSize", (int*)&pimpl_->app_->getConfig().buffer_frames, "64\0128\0256\0512\0");
            ImGui::Checkbox("Exclusive Mode", &pimpl_->app_->getConfig().exclusive_mode);

            ImGui::End();
        }

        // Voice Panel
        if (pimpl_->app_) {
            ImGui::Begin("Voice Conversion", &pimpl_->show_settings_);
            
            ImGui::Text("Converter: %s", pimpl_->app_->getCurrentConverter().c_str());
            ImGui::Separator();

            const char* converter_types[] = {"Passthrough", "DSP Effects", "ONNX Streaming"};
            int current_type = 0;
            std::string curr = pimpl_->app_->getCurrentConverter();
            if (curr == "Passthrough") current_type = 0;
            else if (curr == "DSP Effects") current_type = 1;
            else if (curr == "OnnxVoiceConverter") current_type = 2;

            if (ImGui::Combo("Type", &current_type, converter_types, 3)) {
                std::string new_type;
                switch (current_type) {
                    case 0: new_type = "passthrough"; break;
                    case 1: new_type = "dsp"; break;
                    case 2: new_type = "onnx"; break;
                }
                pimpl_->app_->loadConverter(new_type, pimpl_->app_->getConfig().model_path);
            }

            if (current_type == 2) { // ONNX
                ImGui::InputText("Model Path", &pimpl_->app_->getConfig().model_path);
                ImGui::InputText("Target Voice", &pimpl_->app_->getConfig().target_voice);
                ImGui::Combo("Execution Provider", (int*)&pimpl_->app_->getConfig().execution_provider, "CPU\0CUDA\0TensorRT\0DirectML\0");
            }

            ImGui::Separator();
            ImGui::SliderFloat("Pitch Shift", &pimpl_->app_->getConfig().pitch_shift, -12.0f, 12.0f, "%.1f st");
            ImGui::SliderFloat("Formant Shift", &pimpl_->app_->getConfig().formant_shift, 0.5f, 2.0f, "%.2f");
            ImGui::SliderFloat("Mix", &pimpl_->app_->getConfig().mix, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Output Gain", &pimpl_->app_->getConfig().output_gain_db, -20.0f, 20.0f, "%.1f dB");
            ImGui::SliderInt("Chunk Size (ms)", &pimpl_->app_->getConfig().chunk_size_ms, 5, 100, "%d ms");

            if (current_type == 1) { // DSP
                ImGui::Separator();
                ImGui::Text("DSP Effects");
                ImGui::SliderFloat("Input Gain", &pimpl_->app_->getConfig().input_gain_db, -20.0f, 20.0f, "%.1f dB");
                ImGui::SliderFloat("Highpass Cutoff", &pimpl_->app_->getConfig().highpass_cutoff_hz, 20.0f, 500.0f, "%.0f Hz");
                ImGui::SliderFloat("Limiter Threshold", &pimpl_->app_->getConfig().limiter_threshold_db, -20.0f, 0.0f, "%.1f dB");
                ImGui::SliderFloat("Limiter Release", &pimpl_->app_->getConfig().limiter_release_ms, 1.0f, 500.0f, "%.0f ms");
                ImGui::Checkbox("Enable Highpass", &pimpl_->app_->getConfig().enable_highpass);
                ImGui::Checkbox("Enable Limiter", &pimpl_->app_->getConfig().enable_limiter);
            }

            ImGui::End();
        }

        // Performance Panel
        if (pimpl_->show_performance_ && pimpl_->app_) {
            pimpl_->updatePerformanceGraph();
            auto snapshot = pimpl_->app_->getPerformanceMonitor().getSnapshot();

            ImGui::Begin("Performance", &pimpl_->show_performance_);
            
            ImGui::Text("Audio Callback: %.2f ms (avg) / %.2f ms (max)", 
                        snapshot.callback_avg_ms, snapshot.callback_max_ms);
            ImGui::Text("Processing: %.2f ms (avg) / %.2f ms (max)",
                        snapshot.processing_avg_ms, snapshot.processing_max_ms);
            ImGui::Text("Inference: %.2f ms (avg) / %.2f ms (max)",
                        snapshot.inference_avg_ms, snapshot.inference_max_ms);
            
            ImGui::Separator();
            ImGui::Text("Underruns: %llu  Overruns: %llu  Deadline Misses: %llu",
                        snapshot.underruns, snapshot.overruns, snapshot.deadline_misses);
            ImGui::Text("Queue Depth - In: %zu  Out: %zu", 
                        snapshot.input_queue_depth, snapshot.output_queue_depth);
            ImGui::Text("CPU Usage: %.1f%%", snapshot.cpu_usage_percent);
            ImGui::Text("Realtime Factor: %.2fx", snapshot.realtime_factor);

            ImGui::Separator();
            ImGui::PlotLines("CPU %", pimpl_->cpu_history_, pimpl_->GRAPH_HISTORY, 
                             pimpl_->history_idx_, nullptr, 0.0f, 100.0f, ImVec2(0, 80));
            ImGui::PlotLines("Latency (ms)", pimpl_->latency_history_, pimpl_->GRAPH_HISTORY,
                             pimpl_->history_idx_, nullptr, 0.0f, 50.0f, ImVec2(0, 80));

            if (ImGui::Button("Reset Counters")) {
                pimpl_->app_->getPerformanceMonitor().reset();
            }

            ImGui::End();
        }

        // About Dialog
        if (pimpl_->show_about_) {
            ImGui::OpenPopup("About V-Morph");
            ImGui::SetNextWindowSize(ImVec2(400, 300));
            if (ImGui::BeginPopupModal("About V-Morph", &pimpl_->show_about_, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("V-Morph - Real-time AI Voice Changer");
                ImGui::Text("Version 0.1.0");
                ImGui::Separator();
                ImGui::Text("Low-latency voice conversion for gaming, Discord, and VoIP");
                ImGui::Text("Built with C++20, ONNX Runtime, WASAPI, Dear ImGui");
                ImGui::Separator();
                ImGui::Text("Copyright (c) 2026 Mayank Rana");
                ImGui::Text("MIT License");
                ImGui::Separator();
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    pimpl_->show_about_ = false;
                }
                ImGui::EndPopup();
            }
        }

        // Rendering
        ImGui::Render();
        const float clear_color[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
        pimpl_->context_->OMSetRenderTargets(1, &pimpl_->render_target_, nullptr);
        pimpl_->context_->ClearRenderTargetView(pimpl_->render_target_, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        pimpl_->swap_chain_->Present(1, 0);

        // Small sleep to prevent 100% CPU when idle
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void MainWindow::shutdown() {
    if (!pimpl_->running_) return;
    
    pimpl_->running_ = false;
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    pimpl_->shutdownD3D11();
    
    if (pimpl_->hwnd_) {
        DestroyWindow(pimpl_->hwnd_);
        pimpl_->hwnd_ = nullptr;
    }
    
    UnregisterClassW(L"V-Morph", GetModuleHandle(nullptr));
}

void MainWindow::setAudioEngine(Application* app) {
    pimpl_->app_ = app;
}

} // namespace rtvcc