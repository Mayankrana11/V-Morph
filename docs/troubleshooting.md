# Troubleshooting Guide

## Build Issues

### CMake Cannot Find Dependencies
```
CMake Error: Could not find ONNX Runtime
```
**Solution**: Ensure vcpkg is installed and integrated:
```powershell
vcpkg install onnxruntime:x64-windows
vcpkg integrate install
```

### MSVC Not Found
```
CMAKE_CXX_COMPILER not found
```
**Solution**: Install Visual Studio 2022 with "Desktop development with C++" workload.

### Ninja Not Found
```
CMAKE_MAKE_PROGRAM not found
```
**Solution**: `winget install Ninja-build.Ninja`

## Runtime Issues

### "No audio devices found"
1. Check Windows Sound settings - devices must be enabled
2. Run `--list-devices` to see detected devices
3. Verify microphone permissions: Settings → Privacy → Microphone

### "Failed to initialize audio stream"
- **Exclusive mode conflict**: Another app using device exclusively
  - Close other audio apps (Discord, browsers, games)
  - Or disable exclusive_mode in config
- **Format not supported**: Device doesn't support 48kHz/32-bit float
  - Try 44.1kHz or 16-bit

### "Model initialization failed"
- **File not found**: Check model_path in config
- **ONNX version mismatch**: Model exported with different opset
  - Re-export with compatible opset (≤18 for ORT 1.16+)
- **Unsupported operators**: Model uses ops not in ONNX Runtime
  - Check `docs/model-evaluation.md` for supported ops

### High CPU Usage
- **Buffer too small**: Increase `buffer_frames` to 256 or 512
- **Model too heavy**: Use smaller model or enable GPU
- **No GPU acceleration**: Install CUDA Toolkit and enable CUDA EP

### Audio Glitches (Clicks/Pops)
- **Underruns**: Check performance panel for underrun count
  - Increase buffer size
  - Set High Performance power plan
  - Disable CPU power saving in BIOS
- **Sample rate mismatch**: Ensure all devices at same rate
- **Buffer drift**: Restart stream periodically

### No Output in Discord
1. Verify VB-Cable installed and working
2. Check Discord Input Device = "CABLE Output (VB-Audio Virtual Cable)"
3. Check Windows Input Device = "CABLE Output (VB-Audio Virtual Cable)"
4. Test with Windows Voice Recorder first

### Model Produces Silence/Noise
- **Wrong input level**: Model expects specific range (check docs)
- **Sample rate mismatch**: Resampler not configured correctly
- **State not reset**: Call `reset()` after stream changes
- **Wrong tensor layout**: Check input/output shapes

### Latency Too High
- Reduce `buffer_frames` (64, 128)
- Enable exclusive_mode
- Check for audio enhancements in Windows Sound properties
- Use wired headset (Bluetooth adds 50-200ms)

## Performance Optimization

### CPU Inference Slow
1. Enable graph optimization: `config.graph_optimization = true`
2. Set intra_op_threads = physical cores
3. Try FP16 quantization
4. Use TensorRT EP if NVIDIA GPU available

### GPU Not Used
1. Install CUDA Toolkit 12.x
2. Set `execution_provider = "CUDA"` in config
3. Verify with `--diagnostics`

### Memory Growth
- Check for buffer leaks in processing thread
- Ensure ring buffers not growing unbounded
- Verify model tensors not reallocated per inference

## Debugging Commands

```bash
# List all audio devices
rtvc.exe --list-devices

# Full system diagnostics
rtvc.exe --diagnostics

# Run latency test
rtvc.exe --latency-test --duration 30

# Benchmark performance
rtvc.exe --benchmark --duration 60

# Run with debug logging
RTVC_LOG_LEVEL=DEBUG rtvc.exe

# Test specific device
rtvc.exe --input-device "{guid}" --output-device "{guid}"
```

## Log Analysis

Enable debug logging:
```json
{
    "runtime": {
        "log_level": "DEBUG"
    }
}
```

Key log messages:
- `Audio engine initialized` - Device opened successfully
- `Stream started` - Callback active
- `Model loaded: Xms` - Warmup complete
- `Underrun count: N` - Buffer starvation events
- `Processing time: Xms` - Per-chunk timing

## Common Error Codes

| Code | Meaning | Resolution |
|------|---------|------------|
| AUDCLNT_E_ALREADY_INITIALIZED | Stream already running | Stop before reconfiguring |
| AUDCLNT_E_DEVICE_INVALIDATED | Device unplugged | Handle device change |
| E_INVALIDARG | Bad format/config | Check sample rate/channels |
| ONNXRUNTIME_FAIL | Model error | Check model compatibility |
| QUEUE_UNDERRUN | Processing too slow | Increase buffer or optimize |

## Getting Help

1. Run `rtvc.exe --diagnostics` and include output
2. Check `logs/rtvc.log` for detailed trace
3. Run latency test and share results
4. Include:
   - OS version (Windows 10/11 build)
   - CPU/GPU model
   - Audio interface
   - Config file
   - Steps to reproduce