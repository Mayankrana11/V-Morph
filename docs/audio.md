# Audio System Documentation

## WASAPI Exclusive Mode

### Why Exclusive Mode?
- Lowest latency (bypasses audio engine)
- Direct hardware access
- No system resampling
- Glitch-free with proper buffer sizing

### Configuration

```cpp
AudioStreamConfig config;
config.format.sample_rate = 48000;
config.format.channels = 1;
config.format.bits_per_sample = 32;  // Float
config.buffer_frames = 128;
config.exclusive_mode = true;
config.low_latency = true;
```

### Device Enumeration

```cpp
auto devices = audio_engine->enumerateDevices();
for (const auto& dev : devices) {
    std::cout << (dev.is_input ? "[IN] " : "[OUT] ") 
              << dev.name << " (" << dev.id << ")\n";
}
```

### Format Negotiation

WASAPI requires specific format matching:
- Query device mix format via `IAudioClient::GetMixFormat()`
- Request closest match via `IAudioClient::IsFormatSupported()`
- Initialize with `AUDCLNT_STREAMFLAGS_EVENTCALLBACK`

### Event-Driven Callback

```cpp
// Set event handle for callback notification
HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
audio_client->SetEventHandle(event);

// Audio thread waits on event
WaitForSingleObject(event, INFINITE);
// ... process audio ...
```

## Audio Ring Buffer

### Design
- Single-Producer Single-Consumer (SPSC)
- Lock-free using atomic indices
- Power-of-two capacity for fast modulo
- Cache-line aligned to prevent false sharing

### Usage

```cpp
// Producer (audio callback)
ring_buffer.push(frames, count);

// Consumer (processing thread)
size_t popped = ring_buffer.pop(frames, count);
```

### Sizing
- Minimum: 2x max chunk size
- Recommended: 1-2 seconds of audio
- At 48kHz mono: 48000-96000 frames

## Resampling

### Requirements
- 48kHz ↔ 16kHz/24kHz (model rates)
- Low latency (< 1ms)
- High quality (alias rejection > 80dB)
- Streaming (stateful)

### Implementation
Current: Linear interpolation (placeholder)
Production: Polyphase FIR (soxr, libresample, or custom)

### Quality Levels
| Level | Taps | Latency | CPU |
|-------|------|---------|-----|
| Fast | 8 | 0.5ms | Low |
| Medium | 16 | 1ms | Medium |
| High | 32+ | 2ms | High |

## Device Management

### Hot-Plug Handling
1. Register for device notifications (`IMMNotificationClient`)
2. On device removal: pause stream, notify UI
3. On device addition: refresh list, offer switch
4. Auto-recover if default device changes

### Recovery Strategy
```
Device Lost
    │
    ├─▶ Pause processing thread
    ├─▶ Stop audio stream
    ├─▶ Wait for device return (timeout: 5s)
    ├─▶ Re-enumerate devices
    ├─▶ Reinitialize with same config
    └─▶ Resume processing
```

## Virtual Audio Routing

### VB-Cable Setup
1. Install VB-Cable Driver
2. Restart system
3. New devices appear:
   - **CABLE Input (VB-Audio)** - Output from app
   - **CABLE Output (VB-Audio)** - Input to Discord

### Windows Sound Settings
```
System → Sound → Input: CABLE Output (VB-Audio Virtual Cable)
System → Sound → Output: CABLE Input (VB-Audio Virtual Cable)  [for monitoring]
```

### Discord Settings
```
Voice & Video → Input Device: CABLE Output (VB-Audio Virtual Cable)
Voice & Video → Output Device: Your headphones/speakers
```

### Application Config
```json
{
    "audio": {
        "input_device_id": "{your_mic_guid}",
        "output_device_id": "{cable_input_guid}"
    }
}
```

## Audio Quality

### Processing Chain
```
Input → HPF (80Hz) → Gain → [VC Model] → Limiter (-1dB) → Gain → Output
```

### DC Offset Protection
- High-pass filter at 80Hz removes DC
- Continuous DC measurement and correction

### Clipping Prevention
- Soft limiter with 1ms lookahead
- Peak detection with configurable release
- Gain reduction reported in metrics

### Gain Staging
- Input gain: Compensate for quiet mics
- Model expects specific level (typically -20dBFS to -10dBFS)
- Output gain: Match destination expectations
- Mix control: Dry/wet blend for natural sound

## Troubleshooting

### No Input Signal
- Check Windows Privacy → Microphone access
- Verify device ID matches exactly
- Try shared mode if exclusive fails

### Glitches/Underruns
- Increase buffer_frames (128 → 256)
- Set power plan to High Performance
- Disable CPU throttling
- Check for other audio apps

### High Latency
- Decrease buffer_frames
- Enable exclusive mode
- Disable Windows audio enhancements
- Use dedicated USB audio interface

### Sample Rate Issues
- Ensure mic supports 48kHz
- Check for automatic resampling in Windows
- Verify model sample rate matches