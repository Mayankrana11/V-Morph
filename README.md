# V-Morph

Real-time AI Voice Changer for Windows - Low-latency voice conversion for gaming, Discord, and VoIP applications.

## Features

- **Ultra-low latency**: Target < 30ms end-to-end latency
- **Real-time processing**: Lock-free audio pipeline with dedicated processing thread
- **Model-agnostic architecture**: Easy to swap voice conversion models
- **Offline operation**: No internet required after model download
- **Virtual microphone support**: Route processed audio to Discord/games
- **Performance monitoring**: Built-in latency and CPU metrics

## Requirements

### Build Dependencies
- **Windows 10/11** (64-bit)
- **Visual Studio 2022** with Desktop Development with C++ workload
- **CMake** ≥ 3.25
- **Ninja** (recommended)
- **vcpkg** (for dependency management)

### Optional
- **CUDA Toolkit 12.x** (for GPU acceleration)
- **VB-Cable** (virtual audio driver for Discord integration)

## Quick Start

### 1. Install Dependencies

```powershell
# Install Visual Studio 2022 Community (if not installed)
# https://visualstudio.microsoft.com/downloads/

# Install CMake and Ninja
winget install Kitware.CMake Ninja-build.Ninja

# Install vcpkg
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install dependencies via vcpkg
.\vcpkg install onnxruntime:x64-windows
```

### 2. Build

```powershell
# Configure
cmake --preset windows-release

# Build
cmake --build --preset windows-release

# Run CLI
.\build\windows-release\bin\v-morph.exe --list-devices

# Run GUI
.\build\windows-release\bin\v-morph.exe --gui
```

### 3. Configure Virtual Audio (for Discord)

1. Install [VB-Cable](https://vb-audio.com/Cable/)
2. In Windows Sound Settings:
   - Set **CABLE Input (VB-Audio Virtual Cable)** as your default **Output** device
   - Set **CABLE Output (VB-Audio Virtual Cable)** as your **Input** device in Discord
3. In V-Morph:
   - Select your physical microphone as **Input**
   - Select **CABLE Input** as **Output**

## Usage

### Command Line

```bash
# List audio devices
v-morph.exe --list-devices

# Print diagnostics
v-morph.exe --diagnostics

# Run with custom config
v-morph.exe --config configs/development.json

# Run benchmark
v-morph.exe --benchmark

# Latency test
v-morph.exe --latency-test

# Run GUI
v-morph.exe --gui
```

### Configuration

Edit `configs/default.json` or create your own:

```json
{
    "audio": {
        "sample_rate": 48000,
        "buffer_frames": 128,
        "exclusive_mode": true
    },
    "virtual_audio": {
        "use_virtual_output": false,
        "virtual_output_device_id": ""
    },
    "voice": {
        "converter_type": "passthrough",
        "model_path": "models/your_model.onnx",
        "chunk_size_ms": 20
    },
    "dsp": {
        "input_gain_db": 0.0,
        "highpass_cutoff_hz": 80.0,
        "limiter_threshold_db": -1.0,
        "limiter_release_ms": 50.0,
        "enable_highpass": true,
        "enable_limiter": true
    },
    "performance": {
        "execution_provider": "CPU",
        "inference_threads": 1
    }
}
```

## Architecture

```
Microphone
    ↓
WASAPI Capture (Exclusive Mode)
    ↓
Lock-free Ring Buffer (SPSC)
    ↓
Processing Thread
    ↓
[Resample] → [Preprocessing] → [Voice Converter] → [Postprocessing] → [Resample]
    ↓
Lock-free Ring Buffer (SPSC)
    ↓
WASAPI Render (Exclusive Mode)
    ↓
Virtual Microphone / Speakers
```

### Core Components

| Component | Description |
|-----------|-------------|
| `AudioEngine` | WASAPI exclusive-mode audio I/O |
| `AudioRingBuffer` | Lock-free SPSC queue for audio frames |
| `VoiceConverter` | Abstract interface for voice conversion |
| `PassthroughConverter` | Zero-latency passthrough (baseline) |
| `DSPVoiceConverter` | Gain, high-pass, limiter effects |
| `OnnxInferenceEngine` | ONNX Runtime integration |
| `PerformanceMonitor` | Real-time metrics collection |

## Model Support

Currently implemented:
- **Passthrough** - Zero processing (testing baseline)
- **DSP Effects** - Gain, high-pass filter, limiter
- **ONNX Runtime Voice Converter** - Streaming ONNX model inference (NEW)

Planned:
- **LLVC** - Low-latency voice conversion
- **StreamVC** - Streaming voice conversion
- **RVC** - Retrieval-based voice conversion

## Performance Targets

| Metric | Target | Stretch |
|--------|--------|---------|
| End-to-end latency | ≤ 30ms | ≤ 20ms |
| Audio callback CPU | < 25% budget | < 15% |
| Processing thread | < 50% chunk time | < 25% |
| Underruns (1hr) | 0 | 0 |

## Project Structure

```
rt-voice-changer/
├── src/
│   ├── app/           # Application logic
│   ├── audio/         # Audio engine, devices, ring buffers
│   ├── dsp/           # Resampler, filters, gain, limiter
│   ├── inference/     # ONNX Runtime abstraction
│   ├── vc/            # Voice converter implementations
│   ├── threading/     # Real-time thread, lock-free queues
│   ├── metrics/       # Latency, performance monitoring
│   ├── ui/            # Dear ImGui UI (planned)
│   └── platform/      # Windows (WASAPI), Linux, macOS
├── models/            # Model files (not in git)
├── tests/             # Unit and integration tests
├── benchmarks/        # Performance benchmarks
├── tools/             # Model conversion, diagnostics
├── configs/           # Configuration files
└── docs/              # Documentation
```

## Development

### Build Scripts

```powershell
# Debug build with tests
.\scripts\build.ps1 -Config Debug

# Release build
.\scripts\build.ps1 -Config Release

# Run tests
.\scripts\test.ps1

# Run benchmarks
.\scripts\benchmark.ps1
```

### Adding a New Voice Converter

1. Implement `IVoiceConverter` interface in `src/vc/`
2. Register in `createVoiceConverter()` factory
3. Add configuration options to `AppConfig`
4. Update documentation

## Troubleshooting

### No Audio Input/Output
- Check Windows Privacy Settings → Microphone access
- Verify device IDs with `--list-devices`
- Try disabling exclusive mode

### High Latency
- Reduce `buffer_frames` (64, 128, 256)
- Enable exclusive mode
- Check CPU usage in performance panel

### Underruns/Glitches
- Increase `buffer_frames`
- Close other audio applications
- Check for CPU throttling

### Model Not Loading
- Verify model path in config
- Check ONNX Runtime version compatibility
- Validate model with `tools/diagnostics`

## License

MIT License - See LICENSE file for details.

Third-party licenses documented in `docs/licensing.md`.

## Contributing

1. Fork the repository
2. Create feature branch
3. Follow coding standards (C++20, clang-format)
4. Add tests for new functionality
5. Submit PR with description


### Future Enhancements
- Real voice conversion model integration (LLVC, StreamVC)
- Speaker embedding profiles
- Noise suppression and echo cancellation
- macOS/Linux audio backends
- MSI installer with code signing