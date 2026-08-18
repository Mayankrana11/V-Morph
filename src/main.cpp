#include "app/Application.h"
#include "audio/AudioEngine.h"
#include "vc/PassthroughConverter.h"
#include "vc/DSPVoiceConverter.h"
#include "vc/VoiceConverter.h"
#include "threading/RealtimeThread.h"
#include "metrics/PerformanceMonitor.h"
#include "metrics/LatencyMetrics.h"
#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <signal.h>

namespace rtvcc {

struct Application::Impl {
    AppConfig config;
    std::unique_ptr<IAudioEngine> audio_engine;
    std::unique_ptr<IVoiceConverter> voice_converter;
    std::unique_ptr<PerformanceMonitor> perf_monitor;
    std::unique_ptr<LatencyMetrics> latency_metrics;

    // Audio processing thread
    std::unique_ptr<RealtimeThread> processing_thread;
    std::atomic<bool> processing_running{false};

    // Ring buffers between audio callback and processing thread
    std::unique_ptr<AudioRingBuffer> input_queue;
    std::unique_ptr<AudioRingBuffer> output_queue;

    // Resamplers if needed
    std::unique_ptr<Resampler> input_resampler;
    std::unique_ptr<Resampler> output_resampler;

    // State
    std::atomic<bool> bypass{false};
    std::atomic<bool> should_stop{false};

    // Temporary buffers
    std::vector<float> input_buffer;
    std::vector<float> output_buffer;
    std::vector<float> converter_input;
    std::vector<float> converter_output;

    Impl() : perf_monitor(std::make_unique<PerformanceMonitor>()),
             latency_metrics(std::make_unique<LatencyMetrics>()) {}
};

Application::Application() : pimpl_(std::make_unique<Impl>()) {}
Application::~Application() {
    stopAudio();
}

bool Application::initialize(const AppConfig& config) {
    pimpl_->config = config;

    // Create audio engine
    pimpl_->audio_engine = IAudioEngine::create(AudioBackend::WASAPI);
    if (!pimpl_->audio_engine) {
        std::cerr << "Failed to create audio engine\n";
        return false;
    }

    // Create ring buffers (1 second at 48kHz = 48000 frames)
    size_t queue_capacity = config.sample_rate * 2;
    pimpl_->input_queue = std::make_unique<AudioRingBuffer>(queue_capacity);
    pimpl_->output_queue = std::make_unique<AudioRingBuffer>(queue_capacity);

    // Allocate buffers
    size_t max_frames = config.buffer_frames * 4;
    pimpl_->input_buffer.resize(max_frames * 2);
    pimpl_->output_buffer.resize(max_frames * 2);
    pimpl_->converter_input.resize(max_frames);
    pimpl_->converter_output.resize(max_frames);

    // Load default converter
    if (!loadConverter(config.converter_type, config.model_path)) {
        std::cerr << "Failed to load converter: " << config.converter_type << "\n";
        return false;
    }

    // Configure audio stream
    AudioStreamConfig stream_config;
    stream_config.format.sample_rate = config.sample_rate;
    stream_config.format.channels = 1;
    stream_config.format.bits_per_sample = 32;
    stream_config.buffer_frames = config.buffer_frames;
    stream_config.input_device_id = config.input_device_id;
    stream_config.output_device_id = config.output_device_id;
    stream_config.exclusive_mode = config.exclusive_mode;
    stream_config.low_latency = true;

    // Initialize audio engine with our callback
    if (!pimpl_->audio_engine->initialize(stream_config, this)) {
        std::cerr << "Failed to initialize audio engine\n";
        return false;
    }

    return true;
}

bool Application::initializeFromFile(const std::string& config_path) {
    // TODO: Load from JSON
    return initialize(pimpl_->config);
}

bool Application::saveConfig(const std::string& config_path) const {
    // TODO: Save to JSON
    return true;
}

int Application::run(int argc, char* argv[]) {
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list-devices") {
            auto devices = pimpl_->audio_engine->enumerateDevices();
            for (const auto& dev : devices) {
                std::cout << (dev.is_input ? "[IN] " : "[OUT] ") << dev.name << " (" << dev.id << ")\n";
            }
            return 0;
        } else if (arg == "--diagnostics") {
            printDiagnostics();
            return 0;
        } else if (arg == "--benchmark") {
            // Run benchmark
            return 0;
        } else if (arg == "--latency-test") {
            // Run latency test
            return 0;
        } else if (arg == "--help") {
            std::cout << "Usage: rtvc [options]\n";
            std::cout << "  --list-devices     List audio devices\n";
            std::cout << "  --diagnostics      Print diagnostics\n";
            std::cout << "  --benchmark        Run benchmark\n";
            std::cout << "  --latency-test     Run latency test\n";
            return 0;
        }
    }

    // Start audio
    if (!startAudio()) {
        std::cerr << "Failed to start audio\n";
        return 1;
    }

    if (pimpl_->config.auto_start) {
        // Run until interrupted
        std::cout << "Running... Press Ctrl+C to stop\n";
        std::signal(SIGINT, [](int) { /* handled below */ });

        while (!pimpl_->should_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (pimpl_->config.show_performance) {
                auto snapshot = pimpl_->perf_monitor->getSnapshot();
                std::cout << "\rCallback: " << snapshot.callback_avg_ms << "ms "
                          << "Proc: " << snapshot.processing_avg_ms << "ms "
                          << "Infer: " << snapshot.inference_avg_ms << "ms "
                          << "Underruns: " << snapshot.underruns << " "
                          << "Queue: " << snapshot.input_queue_depth << "/" << snapshot.output_queue_depth
                          << std::flush;
            }
        }
    }

    stopAudio();
    return 0;
}

int Application::runGui() {
    // TODO: Implement GUI mode
    std::cerr << "GUI mode not yet implemented\n";
    return 1;
}

bool Application::startAudio() {
    if (pimpl_->audio_engine->isRunning()) return true;

    // Start processing thread
    pimpl_->processing_running.store(true);
    pimpl_->processing_thread = std::make_unique<RealtimeThread>(
        [this](std::atomic<bool>& stop_flag) { processingThreadFunc(stop_flag); },
        ThreadPriority::High,
        "RTVC_Processing"
    );

    // Start audio engine
    return pimpl_->audio_engine->start();
}

bool Application::stopAudio() {
    pimpl_->should_stop.store(true);
    pimpl_->processing_running.store(false);

    if (pimpl_->processing_thread) {
        pimpl_->processing_thread->stop();
        pimpl_->processing_thread->join();
        pimpl_->processing_thread.reset();
    }

    return pimpl_->audio_engine->stop();
}

bool Application::isAudioRunning() const {
    return pimpl_->audio_engine && pimpl_->audio_engine->isRunning();
}

void Application::setBypass(bool bypass) {
    pimpl_->bypass.store(bypass);
}

bool Application::getBypass() const {
    return pimpl_->bypass.load();
}

bool Application::loadConverter(const std::string& type, const std::string& model_path) {
    if (type == "passthrough") {
        pimpl_->voice_converter = std::make_unique<PassthroughConverter>();
    } else if (type == "dsp") {
        pimpl_->voice_converter = std::make_unique<DSPVoiceConverter>();
    } else {
        std::cerr << "Unknown converter type: " << type << "\n";
        return false;
    }

    VoiceConverterConfig vc_config;
    vc_config.model_path = model_path;
    vc_config.sample_rate = pimpl_->config.sample_rate;
    vc_config.chunk_size_ms = pimpl_->config.chunk_size_ms;
    vc_config.execution_provider = pimpl_->config.execution_provider;
    vc_config.input_gain_db = pimpl_->config.input_gain_db;
    vc_config.output_gain_db = pimpl_->config.output_gain_db;
    vc_config.limiter_threshold_db = pimpl_->config.limiter_threshold_db;
    vc_config.limiter_release_ms = pimpl_->config.limiter_release_ms;

    if (!pimpl_->voice_converter->initialize(vc_config)) {
        std::cerr << "Failed to initialize converter: " << pimpl_->voice_converter->getLastError() << "\n";
        return false;
    }

    pimpl_->config.converter_type = type;
    pimpl_->config.model_path = model_path;
    return true;
}

bool Application::unloadConverter() {
    pimpl_->voice_converter.reset();
    return true;
}

std::string Application::getCurrentConverter() const {
    return pimpl_->voice_converter ? pimpl_->voice_converter->getName() : "None";
}

void Application::setConfig(const AppConfig& config) {
    pimpl_->config = config;
}

const PerformanceMonitor& Application::getPerformanceMonitor() const {
    return *pimpl_->perf_monitor;
}

void Application::printDiagnostics() const {
    std::cout << "=== RT Voice Changer Diagnostics ===\n";
    std::cout << "Audio Engine: " << (pimpl_->audio_engine ? "Initialized" : "Not initialized") << "\n";
    std::cout << "Audio State: " << static_cast<int>(pimpl_->audio_engine ? pimpl_->audio_engine->getState() : AudioState::Stopped) << "\n";
    std::cout << "Converter: " << getCurrentConverter() << "\n";
    std::cout << "Sample Rate: " << pimpl_->config.sample_rate << "\n";
    std::cout << "Buffer Frames: " << pimpl_->config.buffer_frames << "\n";
    std::cout << "Bypass: " << (pimpl_->bypass.load() ? "Yes" : "No") << "\n";

    if (pimpl_->audio_engine) {
        auto metrics = pimpl_->audio_engine->getMetrics();
        std::cout << "Input Latency: " << metrics.input_latency * 1000 << "ms\n";
        std::cout << "Output Latency: " << metrics.output_latency * 1000 << "ms\n";
        std::cout << "Processing Latency: " << metrics.processing_latency * 1000 << "ms\n";
        std::cout << "Underruns: " << metrics.underruns << "\n";
        std::cout << "Overruns: " << metrics.overruns << "\n";
    }

    auto snapshot = pimpl_->perf_monitor->getSnapshot();
    std::cout << "Callback Avg: " << snapshot.callback_avg_ms << "ms\n";
    std::cout << "Processing Avg: " << snapshot.processing_avg_ms << "ms\n";
    std::cout << "Inference Avg: " << snapshot.inference_avg_ms << "ms\n";
}

// IAudioCallback implementation
void Application::onAudioProcess(const float* input, float* output, size_t frames, size_t channels) {
    pimpl_->perf_monitor->onAudioCallbackStart();

    // Push input to queue
    if (input && channels == 1) {
        size_t pushed = pimpl_->input_queue->push(
            reinterpret_cast<const AudioFrame*>(input), frames);
        if (pushed < frames) {
            pimpl_->perf_monitor->onAudioOverrun();
        }
        pimpl_->perf_monitor->setInputQueueDepth(pimpl_->input_queue->size());
    }

    // Pop output from queue
    if (output && channels == 1) {
        size_t popped = pimpl_->output_queue->pop(
            reinterpret_cast<AudioFrame*>(output), frames);
        if (popped < frames) {
            // Underrun - fill with silence
            std::fill(output + popped, output + frames, 0.0f);
            pimpl_->perf_monitor->onAudioUnderrun();
        }
        pimpl_->perf_monitor->setOutputQueueDepth(pimpl_->output_queue->size());
    }

    pimpl_->perf_monitor->onAudioCallbackEnd();
}

void Application::onStreamStart() {
    pimpl_->processing_running.store(true);
}

void Application::onStreamStop() {
    pimpl_->processing_running.store(false);
}

void Application::onError(const std::string& error) {
    std::cerr << "Audio error: " << error << "\n";
}

// Processing thread function
void Application::processingThreadFunc(std::atomic<bool>& stop_flag) {
    const size_t chunk_frames = (pimpl_->config.sample_rate * pimpl_->config.chunk_size_ms) / 1000;

    while (!stop_flag.load(std::memory_order_acquire)) {
        // Wait for enough input data
        size_t available = pimpl_->input_queue->size();
        if (available < chunk_frames) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        pimpl_->perf_monitor->onProcessingStart();

        // Pop chunk from input queue
        size_t popped = pimpl_->input_queue->pop(
            reinterpret_cast<AudioFrame*>(pimpl_->input_buffer.data()), chunk_frames);

        if (popped > 0) {
            // Process through voice converter
            pimpl_->perf_monitor->onInferenceStart();

            if (!pimpl_->bypass.load() && pimpl_->voice_converter && pimpl_->voice_converter->isReady()) {
                auto result = pimpl_->voice_converter->process(
                    pimpl_->input_buffer.data(), popped, pimpl_->converter_output.data());

                if (result.status == IVoiceConverter::ProcessResult::Status::Success) {
                    // Push to output queue
                    size_t pushed = pimpl_->output_queue->push(
                        reinterpret_cast<const AudioFrame*>(pimpl_->converter_output.data()),
                        result.output_produced);
                    if (pushed < result.output_produced) {
                        pimpl_->perf_monitor->onAudioOverrun();
                    }
                }
            } else {
                // Bypass or no converter - passthrough
                size_t pushed = pimpl_->output_queue->push(
                    reinterpret_cast<const AudioFrame*>(pimpl_->input_buffer.data()), popped);
                if (pushed < popped) {
                    pimpl_->perf_monitor->onAudioOverrun();
                }
            }

            pimpl_->perf_monitor->onInferenceEnd();
        }

        pimpl_->perf_monitor->onProcessingEnd();
    }
}

} // namespace rtvcc

// Main entry point
int main(int argc, char* argv[]) {
    rtvcc::Application app;
    rtvcc::AppConfig config;

    // Set defaults
    config.sample_rate = 48000;
    config.buffer_frames = 128;
    config.converter_type = "passthrough";

    if (!app.initialize(config)) {
        return 1;
    }

    return app.run(argc, argv);
}