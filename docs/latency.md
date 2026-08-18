# Latency Analysis and Measurement

## Latency Components

```
Total Latency = Capture + Input Buffer + Processing + Output Buffer + Render
```

| Component | Typical Range | Notes |
|-----------|---------------|-------|
| OS Capture | 0.5-2ms | WASAPI exclusive mode |
| Input Ring Buffer | 0-2ms | Depends on queue depth |
| Resample (48k→16k) | 0.5-1ms | Linear interpolation |
| Preprocessing | <0.1ms | HPF, gain |
| **Model Inference** | **5-20ms** | **Main variable** |
| Postprocessing | <0.1ms | Limiter, gain |
| Resample (16k→48k) | 0.5-1ms | Linear interpolation |
| Output Ring Buffer | 0-2ms | Depends on queue depth |
| OS Render | 0.5-2ms | WASAPI exclusive mode |
| **Total (Target)** | **≤30ms** | **Stretch: ≤20ms** |

## Buffer Size vs Latency

| Buffer Frames | Callback Interval | Theoretical Min Latency |
|---------------|-------------------|------------------------|
| 64 | 1.33ms | ~5ms |
| 128 | 2.67ms | ~8ms |
| 256 | 5.33ms | ~14ms |
| 512 | 10.67ms | ~25ms |

**Note**: Actual latency > theoretical due to:
- Double/triple buffering in driver
- Processing thread scheduling
- Model inference time

## Measurement Methodology

### Impulse Response Method

1. Generate unit impulse (1.0 followed by zeros)
2. Feed into microphone input (loopback or hardware)
3. Record output from virtual microphone
4. Cross-correlate input/output
5. Peak correlation = total latency

### Implementation

```cpp
// In tools/latency_test/
class LatencyTester {
    // Generate impulse
    void generateImpulse(float* buffer, size_t frames);

    // Process and measure
    double measure(const float* input, const float* output, size_t frames);
};
```

### Metrics Collected

- **Mean latency**: Average over N measurements
- **Median latency**: P50
- **P95/P99**: Tail latency
- **Jitter**: StdDev of latency
- **Min/Max**: Range

## Optimization Strategies

### 1. Reduce Buffer Size
- Trade-off: Lower latency vs higher underrun risk
- Minimum practical: 64 frames (1.33ms) on good hardware

### 2. Model Optimization
- Quantization (FP16/INT8)
- Graph optimization (fusion, constant folding)
- Operator selection (avoid slow ops)
- Distillation to smaller model

### 3. Processing Thread
- Dedicated CPU core (affinity)
- High priority (MMCSS Pro Audio)
- Lock-free queues (no mutex contention)
- Pre-allocated tensors

### 4. Resampling
- Use lower quality for monitoring, high for output
- Consider polyphase FIR with minimal taps
- Avoid double resampling

### 5. Pipeline Parallelism
```
Callback Thread          Processing Thread
    │                          │
    ├─ Push input ────────────▶│
    │                          ├─ Resample
    │                          ├─ Preprocess
    │                          ├─ **Inference** (bottleneck)
    │                          ├─ Postprocess
    │                          ├─ Resample
    │                          └─ Push output
    │◀──── Pop output ──────────┤
    │                          │
```

## Latency Budget Allocation (30ms target)

| Stage | Budget | Notes |
|-------|--------|-------|
| OS Audio | 4ms | 2ms capture + 2ms render |
| Ring Buffers | 4ms | 2ms input + 2ms output |
| Resampling | 2ms | 1ms each direction |
| Pre/Post DSP | 1ms | Very fast |
| **Model Inference** | **19ms** | **Primary budget** |
| Safety Margin | 5ms | Scheduling jitter |

## Profiling Tools

### Windows
- **Windows Performance Analyzer (WPA)** - ETW tracing
- **Visual Studio Profiler** - CPU sampling
- **NVIDIA Nsight** - GPU profiling (if CUDA)
- **Custom ETW Provider** - Audio callback timestamps

### Metrics to Track
```cpp
struct LatencyBreakdown {
    double capture_ms;
    double input_queue_ms;
    double resample_in_ms;
    double preprocess_ms;
    double inference_ms;
    double postprocess_ms;
    double resample_out_ms;
    double output_queue_ms;
    double render_ms;
    double total_ms;
};
```

## Regression Testing

### Automated Benchmark
```bash
# Run on every commit
rtvc.exe --benchmark --duration 60 --output results.json
```

### Thresholds
| Metric | Warning | Error |
|--------|---------|-------|
| Mean Latency | > 25ms | > 35ms |
| P99 Latency | > 40ms | > 50ms |
| Underruns/min | > 1 | > 5 |
| Callback CPU | > 50% | > 80% |

## Troubleshooting High Latency

### Symptom: Consistent high latency
- Check buffer size configuration
- Verify exclusive mode enabled
- Check for audio enhancements in Windows (disable)

### Symptom: Sporadic latency spikes
- CPU throttling (check power plan)
- Background processes
- Garbage collection (not applicable in C++)
- GPU context switching

### Symptom: Increasing latency over time
- Buffer leak (queue growing)
- Sample rate mismatch (drift)
- Memory leak

### Debug Commands
```bash
# Real-time diagnostics
rtvc.exe --diagnostics

# Latency test
rtvc.exe --latency-test --duration 30

# CPU profiling
# Use WPA with custom ETW provider
```