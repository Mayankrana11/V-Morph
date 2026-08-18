# Architecture Documentation

## Overview

RT Voice Changer follows a strict layered architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│  (Config, State Machine, CLI/GUI, Model Management)         │
└─────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────┐
│                       Audio Engine                           │
│  (Device Management, Stream Control, Callback Dispatch)     │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│  Input Queue  │    │ Processing    │    │ Output Queue  │
│  (SPSC Ring)  │───▶│   Thread      │───▶│  (SPSC Ring)  │
└───────────────┘    └───────────────┘    └───────────────┘
                            │
                   ┌────────┴────────┐
                   ▼                 ▼
            ┌─────────────┐   ┌─────────────┐
            │ Preprocess  │   │ Postprocess │
            │ (Resample,  │   │ (Resample,  │
            │  HPF, Gain) │   │  Limiter)   │
            └─────────────┘   └─────────────┘
                   │                 │
                   └────────┬────────┘
                            ▼
                   ┌─────────────────┐
                   │ Voice Converter │
                   │  (Interface)    │
                   └─────────────────┘
                            │
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
       ┌──────────┐  ┌──────────┐  ┌──────────┐
       │Passthrough│  │   DSP    │  │  ONNX    │
       │ Converter │  │ Effects  │  │  Model   │
       └──────────┘  └──────────┘  └──────────┘
```

## Real-Time Constraints

### Audio Callback (WASAPI)

**MUST NOT:**
- Allocate/free memory
- Lock mutexes
- Block on I/O
- Call virtual functions
- Log to disk
- Wait for other threads

**MUST:**
- Return within deadline (typically 1-3ms)
- Only read/write ring buffers
- Use pre-allocated memory

### Processing Thread

- Runs at high priority (MMCSS Pro Audio on Windows)
- Pulls from input queue, pushes to output queue
- Runs voice conversion model
- Can fall behind temporarily (queue absorbs)
- Must not block audio callback

## Threading Model

| Thread | Priority | Responsibility |
|--------|----------|----------------|
| Audio Callback | Real-time (MMCSS) | Device I/O, ring buffer push/pop |
| Processing | High | Voice conversion, DSP, resampling |
| UI | Normal | Configuration, display, model management |
| Model Loading | Background | ONNX session creation, warmup |

## Data Flow

### Sample Rate Conversion

```
Mic (48kHz) → Resampler → Model Rate (16/24kHz)
                              ↓
                        Voice Converter
                              ↓
Model Rate → Resampler → Output (48kHz)
```

### Buffer Sizes

| Buffer Frames | Duration @ 48kHz | Use Case |
|---------------|------------------|----------|
| 64 | 1.33ms | Ultra-low latency |
| 128 | 2.67ms | **Default** |
| 256 | 5.33ms | Stable |
| 512 | 10.67ms | High latency tolerance |

## Voice Converter Interface

```cpp
class IVoiceConverter {
    virtual bool initialize(VoiceConverterConfig) = 0;
    virtual ProcessResult process(const float* input, size_t frames, float* output) = 0;
    virtual void reset() = 0;
    virtual int getInputSampleRate() const = 0;
    virtual int getOutputSampleRate() const = 0;
    virtual size_t getAlgorithmicLatencySamples() const = 0;
};
```

Implementations:
- `PassthroughConverter` - Zero latency, identity transform
- `DSPVoiceConverter` - Gain, high-pass, limiter
- `OnnxStreamingConverter` - Neural voice conversion (planned)

## Memory Management

- **No heap allocations** in audio callback or processing thread hot path
- All buffers pre-allocated at startup
- Ring buffers use fixed-capacity arrays
- Model tensors pre-allocated during warmup

## Error Handling

- Errors captured via atomic flags/counters
- Audio callback never throws
- Processing thread converts exceptions to error state
- UI polls error state and displays actionable messages

## Extensibility Points

1. **Audio Backend** - Implement `IAudioEngine` for new platforms
2. **Voice Converter** - Implement `IVoiceConverter` for new models
3. **Inference Engine** - Implement `IInferenceEngine` for new runtimes
4. **DSP Effects** - Add new classes in `dsp/`
5. **UI Panels** - Add new panels in `ui/`