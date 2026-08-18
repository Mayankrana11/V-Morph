# Real-Time Safety Documentation

## Overview

This document defines the real-time safety rules for the RT Voice Changer audio callback and processing thread. All code executed on the real-time audio thread MUST comply with these rules.

---

## Forbidden Operations in Audio Callback

### Memory Management
- ❌ `new` / `delete`
- ❌ `malloc` / `free`
- ❌ `std::vector` operations that may allocate (push_back, resize, etc.)
- ❌ `std::string` operations
- ❌ Any STL container modification
- ❌ Smart pointer construction/destruction (`make_unique`, `make_shared`)

### Synchronization
- ❌ `std::mutex` / `lock_guard` / `unique_lock`
- ❌ `std::condition_variable`
- ❌ `std::atomic` operations with `memory_order_seq_cst` (use relaxed/acquire/release)
- ❌ Spinlocks with unbounded retry
- ❌ Thread creation/join/detach

### I/O Operations
- ❌ File I/O (read/write/open/close)
- ❌ Network I/O (send/recv/connect)
- ❌ Console I/O (std::cout, printf)
- ❌ Logging to file
- ❌ Device I/O (except audio device buffers)

### System Calls
- ❌ `Sleep` / `WaitForSingleObject` / blocking waits
- ❌ Dynamic library loading (`LoadLibrary`, `dlopen`)
- ❌ Process/thread creation
- ❌ Memory mapping (`VirtualAlloc`, `mmap`)

### C++ Features
- ❌ Virtual function calls (vtable lookup not guaranteed constant time)
- ❌ Exception throwing/catching
- ❌ RTTI (`dynamic_cast`, `typeid`)
- ❌ Lambda captures that allocate
- ❌ `std::function` invocation (may allocate)

---

## Allowed Operations in Audio Callback

### Memory Access
- ✅ Read/write pre-allocated arrays/buffers
- ✅ Stack allocation (fixed size, small)
- ✅ `std::array`, `std::span` (no allocation)
- ✅ Pointer arithmetic

### Atomic Operations (Lock-Free)
- ✅ `std::atomic` load/store with `memory_order_relaxed` / `acquire` / `release`
- ✅ `std::atomic` fetch_add/sub, compare_exchange_weak
- ✅ Lock-free ring buffer push/pop (SPSC)
- ✅ Lock-free queue operations (SPSC/MPSC if wait-free)

### Math & DSP
- ✅ Basic arithmetic (+, -, *, /)
- ✅ `std::sin`, `std::cos`, `std::sqrt` (compiler intrinsics)
- ✅ SIMD intrinsics (AVX2, SSE)
- ✅ Fixed-coefficient filters
- ✅ Linear interpolation

### Control Flow
- ✅ `if` / `switch` / `for` / `while` (bounded iterations)
- ✅ Function calls to `REALTIME_SAFE` functions
- ✅ Template instantiation (compile-time)

---

## Function Annotations

### REALTIME_SAFE
Mark functions that are guaranteed safe for real-time execution:

```cpp
REALTIME_SAFE
void processAudioFrame(float* input, float* output, size_t frames);
```

### REALTIME_UNSAFE
Mark functions that must NEVER be called from real-time thread:

```cpp
REALTIME_UNSAFE
void loadModel(const std::string& path);
```

---

## Callback Call Graph

```
WASAPI Callback (Real-Time Thread, ~2.67ms deadline)
│
├─▶ AudioRingBuffer::push() [REALTIME_SAFE]
│   └─▶ atomic store/release
│
├─▶ AudioRingBuffer::pop() [REALTIME_SAFE]
│   └─▶ atomic load/acquire
│
├─▶ PerformanceMonitor::onAudioCallbackStart() [REALTIME_SAFE]
│   └─▶ high_resolution_clock::now()
│
├─▶ PerformanceMonitor::onAudioCallbackEnd() [REALTIME_SAFE]
│   └─▶ atomic fetch_add, compare_exchange
│
└─▶ (NO voice conversion here!)
```

```
Processing Thread (High Priority, ~10ms budget)
│
├─▶ AudioRingBuffer::pop() [REALTIME_SAFE]
├─▶ Resampler::process() [REALTIME_SAFE]
├─▶ HighPassFilter::process() [REALTIME_SAFE]
├─▶ Gain::process() [REALTIME_SAFE]
├─▶ Limiter::process() [REALTIME_SAFE]
├─▶ IVoiceConverter::process() [REALTIME_SAFE*]
│   └─▶ PassthroughConverter::process() [REALTIME_SAFE]
│   └─▶ DSPVoiceConverter::process() [REALTIME_SAFE]
│   └─▶ OnnxStreamingConverter::process() [REALTIME_SAFE*]
│       ├─▶ Pre-allocated tensor copy
│       ├─▶ ort::Session::Run() [REALTIME_UNSAFE - may take variable time]
│       └─▶ Post-process output
│
└─▶ AudioRingBuffer::push() [REALTIME_SAFE]
```

**Note**: `OnnxStreamingConverter::process()` is marked conditionally safe - it uses pre-allocated tensors but ONNX Runtime inference time is not strictly bounded. The processing thread absorbs variance.

---

## Thread Safety Rules

### Audio Callback Thread
- Only accesses: ring buffers, atomic metrics counters
- Never calls: voice converter, model inference, DSP with dynamic allocation

### Processing Thread
- Runs at high priority (MMCSS Pro Audio)
- Can call voice converter, resampler, DSP
- Must complete within chunk deadline on average
- Can fall behind temporarily (queue absorbs)

### UI Thread
- Never accesses audio buffers directly
- Communicates via atomic flags / lock-free queues
- Loads models on background thread

### Model Loading Thread
- Creates ONNX sessions
- Runs warmup inference
- Must complete before activation

---

## Memory Pre-Allocation Requirements

All buffers used in real-time path must be pre-allocated at startup:

```cpp
// At startup (not in callback)
std::vector<float> input_buffer(max_frames);
std::vector<float> output_buffer(max_frames);
std::vector<float> converter_input(max_frames);
std::vector<float> converter_output(max_frames);
AudioRingBuffer input_queue(queue_capacity);
AudioRingBuffer output_queue(queue_capacity);
```

### Ring Buffer Sizing
- Minimum: 2x max chunk size
- Recommended: 1-2 seconds of audio
- At 48kHz mono: 48,000 - 96,000 frames

---

## Performance Monitoring (Real-Time Safe)

```cpp
// In PerformanceMonitor - all atomic operations use relaxed/acquire/release
void onAudioCallbackStart() {
    callback_start_ = std::chrono::high_resolution_clock::now();
}

void onAudioCallbackEnd() {
    double ms = elapsed_ms(callback_start_);
    callback_stats_.sum_ms.fetch_add(ms, std::memory_order_relaxed);
    callback_stats_.count.fetch_add(1, std::memory_order_relaxed);
    // ... min/max with compare_exchange_weak
}
```

---

## Verification Checklist

Before merging any code that touches real-time path:

- [ ] No heap allocations in callback or processing thread hot path
- [ ] No mutexes/locks in callback
- [ ] No virtual calls in callback
- [ ] No exceptions thrown from callback
- [ ] All buffers pre-allocated
- [ ] Ring buffer operations use correct memory ordering
- [ ] Function marked `REALTIME_SAFE` or `REALTIME_UNSAFE`
- [ ] Call graph reviewed for forbidden operations
- [ ] Stress tested with 8+ hour run
- [ ] Underrun/overrun counters monitored

---

## Emergency Fallback

If processing thread cannot keep up:

1. Detect via queue depth / underrun counter
2. Switch to passthrough mode atomically
3. Notify UI via atomic flag
4. Log from non-real-time thread
5. Attempt recovery after cooldown

```cpp
// Atomic fallback trigger
std::atomic<bool> emergency_fallback_{false};

// In processing thread
if (queue_depth > threshold) {
    emergency_fallback_.store(true, std::memory_order_release);
}

// In callback
if (emergency_fallback_.load(std::memory_order_acquire)) {
    // Passthrough only
    std::copy(input, input + frames, output);
}
```