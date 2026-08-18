#include "audio/AudioEngine.h"
#include "audio/AudioDevice.h"
#include "vc/PassthroughConverter.h"
#include "vc/DSPVoiceConverter.h"
#include "metrics/PerformanceMonitor.h"
#include <iostream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using namespace rtvcc;

void printSystemInfo() {
    std::cout << "=== System Information ===" << std::endl;

    // OS
    OSVERSIONINFOEXW osvi = { sizeof(osvi) };
    // Would use RtlGetVersion in real implementation
    std::cout << "OS: Windows 10/11" << std::endl;

    // CPU
    SYSTEM_INFO sysinfo;
    GetNativeSystemInfo(&sysinfo);
    std::cout << "CPU Cores: " << sysinfo.dwNumberOfProcessors << std::endl;
    std::cout << "Page Size: " << sysinfo.dwPageSize << " bytes" << std::endl;

    // Memory
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    std::cout << "Total RAM: " << (mem.ullTotalPhys / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "Available RAM: " << (mem.ullAvailPhys / (1024*1024*1024)) << " GB" << std::endl;

    // GPU (would query via DXGI/WMI in real implementation)
    std::cout << "GPU: Query via nvidia-smi or dxdiag" << std::endl;
}

void printAudioDevices(std::unique_ptr<IAudioDeviceManager>& manager) {
    std::cout << "\n=== Audio Devices ===" << std::endl;

    auto devices = manager->enumerateDevices();
    std::cout << "Found " << devices.size() << " devices:" << std::endl;

    for (const auto& dev : devices) {
        std::cout << "  " << (dev.is_input ? "[IN] " : "[OUT]")
                  << dev.name
                  << " (ID: " << dev.id << ")"
                  << (dev.is_default ? " [DEFAULT]" : "")
                  << std::endl;
    }

    auto default_in = manager->getDefaultInputDevice();
    auto default_out = manager->getDefaultOutputDevice();

    if (default_in) {
        std::cout << "Default Input: " << default_in->name << std::endl;
    }
    if (default_out) {
        std::cout << "Default Output: " << default_out->name << std::endl;
    }
}

void printAudioEngineCaps() {
    std::cout << "\n=== Audio Engine Capabilities ===" << std::endl;
    std::cout << "Backend: WASAPI (Windows)" << std::endl;
    std::cout << "Sample Rates: 44100, 48000, 96000" << std::endl;
    std::cout << "Formats: 32-bit float (primary), 16-bit int" << std::endl;
    std::cout << "Modes: Exclusive, Shared" << std::endl;
    std::cout << "Buffer Sizes: 64, 128, 256, 512 frames" << std::endl;
}

void printConverterInfo() {
    std::cout << "\n=== Voice Converters ===" << std::endl;

    std::vector<std::string> converters = {"passthrough", "dsp"};

    for (const auto& type : converters) {
        auto converter = createVoiceConverter(type);
        if (converter) {
            std::cout << "  " << converter->getName() << " v" << converter->getVersion() << std::endl;
            std::cout << "    Input Rate: " << converter->getInputSampleRate() << " Hz" << std::endl;
            std::cout << "    Output Rate: " << converter->getOutputSampleRate() << " Hz" << std::endl;
            std::cout << "    Algorithmic Latency: " << converter->getAlgorithmicLatencyMs() << " ms" << std::endl;
        }
    }

    std::cout << "  (ONNX Streaming VC - not yet implemented)" << std::endl;
}

void printRuntimeInfo() {
    std::cout << "\n=== Runtime Information ===" << std::endl;
    std::cout << "Build Type: " <<
#ifdef _DEBUG
        "Debug"
#else
        "Release"
#endif
        << std::endl;
    std::cout << "C++ Standard: C++20" << std::endl;
    std::cout << "Architecture: x64" << std::endl;

    // SIMD support
    std::cout << "SIMD: ";
#ifdef RTVC_HAVE_AVX2
    std::cout << "AVX2 ";
#endif
#ifdef RTVC_HAVE_FMA
    std::cout << "FMA ";
#endif
    std::cout << std::endl;

    // Dependencies
    std::cout << "ONNX Runtime: " <<
#ifdef RTVC_HAVE_ONNXRUNTIME
        "Available"
#else
        "Not linked"
#endif
        << std::endl;
    std::cout << "RtAudio: " <<
#ifdef RTVC_HAVE_RTAUDIO
        "Available"
#else
        "Not linked"
#endif
        << std::endl;
    std::cout << "Dear ImGui: " <<
#ifdef RTVC_HAVE_IMGUI
        "Available"
#else
        "Not linked"
#endif
        << std::endl;
}

void saveDiagnosticsJson() {
    nlohmann::json j;
    j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    j["version"] = "0.1.0";
    j["platform"] = "Windows";

    // System
    SYSTEM_INFO sysinfo;
    GetNativeSystemInfo(&sysinfo);
    j["system"]["cpu_cores"] = sysinfo.dwNumberOfProcessors;

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    j["system"]["total_ram_gb"] = mem.ullTotalPhys / (1024*1024*1024);
    j["system"]["avail_ram_gb"] = mem.ullAvailPhys / (1024*1024*1024);

    // Audio devices (would populate from manager)
    j["audio"]["devices"] = nlohmann::json::array();
    j["audio"]["backends"] = {"WASAPI"};

    // Converters
    j["converters"] = nlohmann::json::array();
    j["converters"].push_back({{"name", "passthrough"}, {"version", "1.0.0"}});
    j["converters"].push_back({{"name", "dsp"}, {"version", "1.0.0"}});

    // Runtime
    j["runtime"]["build_type"] = 
#ifdef _DEBUG
        "Debug"
#else
        "Release"
#endif
        ;
    j["runtime"]["cpp_standard"] = "C++20";
    j["runtime"]["arch"] = "x64";

    std::ofstream file("diagnostics.json");
    file << j.dump(4);
    std::cout << "\nDiagnostics saved to diagnostics.json" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "RT Voice Changer Diagnostics" << std::endl;
    std::cout << "=============================" << std::endl;

    printSystemInfo();

    // Audio devices
    auto manager = IAudioEngine::create(AudioBackend::WASAPI);
    if (manager) {
        // We need a device manager - for now just create the WASAPI one
        // In real implementation, expose device manager from engine
        printAudioDevices(manager);
    }

    printAudioEngineCaps();
    printConverterInfo();
    printRuntimeInfo();
    saveDiagnosticsJson();

    return 0;
}