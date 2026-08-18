# Voice Conversion Model Evaluation

## Evaluation Criteria

| Criterion | Weight | Description |
|-----------|--------|-------------|
| License | Critical | Must allow commercial use, modification, distribution |
| Streaming Support | Critical | Causal/streaming inference with low lookahead |
| CPU Performance | High | Real-time on modern CPU without GPU |
| Latency | High | Algorithmic + inference < 20ms |
| Quality | High | Natural voice conversion, minimal artifacts |
| ONNX Export | High | Must export to ONNX for C++ inference |
| Model Size | Medium | < 100MB preferred |
| Maintenance | Medium | Active development, recent commits |
| Pretrained Weights | High | Available for immediate use |

## Candidate Models

### 1. LLVC (Low-Latency Voice Conversion)

**Paper**: "LLVC: Low-Latency Voice Conversion" (2023)
**Repo**: https://github.com/xxx/LLVC (placeholder)

| Property | Value |
|----------|-------|
| License | MIT (assumed) |
| Architecture | Causal encoder + decoder, streaming |
| Sample Rate | 16kHz / 24kHz |
| Lookahead | ~10ms (reported) |
| RTF (CPU) | ~2.8x (reported) |
| Model Size | ~15MB |
| ONNX Export | Supported |
| Streaming State | Yes (GRU/LSTM) |
| Zero-shot | Yes |

**Status**: **PRIMARY CANDIDATE** - Explicitly designed for low-latency streaming VC.

**Verification Needed**:
- [ ] Export to ONNX and verify operators
- [ ] Benchmark CPU inference time on target hardware
- [ ] Measure actual end-to-end latency
- [ ] Verify streaming state management
- [ ] Check license of pretrained weights

---

### 2. StreamVC

**Paper**: "StreamVC: Streaming Voice Conversion" (2023)
**Repo**: https://github.com/xxx/StreamVC (placeholder)

| Property | Value |
|----------|-------|
| License | Unknown |
| Architecture | Streaming transformer |
| Sample Rate | 16kHz |
| Lookahead | ~20ms (reported) |
| RTF (CPU) | Unknown |
| Model Size | ~50MB |
| ONNX Export | Partial (transformer ops) |
| Streaming State | Yes (KV cache) |
| Zero-shot | Yes |

**Status**: **SECONDARY** - Transformer-based may have higher latency. Unofficial implementations report incomplete streaming.

**Verification Needed**:
- [ ] Find maintained implementation
- [ ] Verify ONNX export completeness
- [ ] Benchmark CPU performance

---

### 3. RVC (Retrieval-based Voice Conversion)

**Paper**: "RVC: Retrieval-based Voice Conversion" (2023)
**Repo**: https://github.com/RVC-Project/Retrieval-based-Voice-Conversion

| Property | Value |
|----------|-------|
| License | MIT (code), Custom (weights) |
| Architecture | VITS-based, non-streaming |
| Sample Rate | 40kHz / 48kHz |
| Lookahead | Full utterance (non-streaming) |
| RTF (CPU) | > 10x (heavy) |
| Model Size | ~200MB+ |
| ONNX Export | Difficult (complex ops) |
| Streaming State | No |
| Zero-shot | Limited |

**Status**: **NOT SUITABLE** for low-latency core. Can be optional high-quality backend.

---

### 4. QuickVC

**Paper**: "QuickVC: Quick Voice Conversion" (2022)
**Repo**: https://github.com/xxx/QuickVC (placeholder)

| Property | Value |
|----------|-------|
| License | Unknown |
| Architecture | Non-streaming |
| Sample Rate | 22.05kHz |
| Lookahead | Full utterance |
| RTF (CPU) | Unknown |
| Model Size | ~30MB |
| ONNX Export | Unknown |

**Status**: **NOT SUITABLE** - Non-streaming architecture.

---

### 5. ALO-VC

**Paper**: "ALO-VC: Any-to-Any Voice Conversion" (2023)
**Repo**: https://github.com/xxx/ALO-VC (placeholder)

| Property | Value |
|----------|-------|
| License | Unknown |
| Architecture | Flow-based |
| Sample Rate | 16kHz |
| Streaming | Unknown |

**Status**: **NEEDS RESEARCH**

---

## Vocoder Options (for LLVC pipeline)

### LPCNet
- **License**: BSD-3
- **CPU RTF**: ~0.05x (very fast)
- **Quality**: Good
- **Streaming**: Yes
- **ONNX**: Supported

### FARGAN
- **License**: MIT
- **CPU RTF**: ~0.1x
- **Quality**: High
- **Streaming**: Yes (causal)
- **ONNX**: Supported

### HiFi-GAN (lightweight variants)
- **License**: MIT
- **CPU RTF**: ~0.5x (varies)
- **Quality**: Very high
- **Streaming**: Some variants
- **ONNX**: Supported

## Recommendation

**Primary: LLVC + LPCNet/FARGAN vocoder**

Rationale:
1. Explicitly designed for low-latency streaming
2. Reported sub-20ms latency at 16kHz
3. CPU-friendly (2.8x RTF reported)
4. Open license (MIT assumed)
5. ONNX exportable
6. Streaming state support (recurrent layers)

**Fallback: Custom lightweight encoder + FARGAN**

If LLVC proves unsuitable:
- Train/find lightweight causal encoder
- Pair with FARGAN vocoder
- Target: < 10ms algorithmic latency

## Next Steps

1. **Clone LLVC repo** and examine architecture
2. **Export to ONNX** using PyTorch ONNX exporter
3. **Verify operators** compatible with ONNX Runtime
4. **Benchmark** on target hardware (i7-13620H, RTX 4060)
5. **Measure actual latency** using impulse response method
6. **Document** findings and update this evaluation

## Hardware-Specific Notes

### Target: i7-13620H (10C/16T, AVX2, AVX-512 partial)
- AVX2 fully supported → use ONNX Runtime CPU EP with AVX2
- 16 threads → can run inference on dedicated thread
- 32GB RAM → model size not constrained

### GPU: RTX 4060 Laptop (8GB VRAM, CC 8.9)
- CUDA 12.x supported
- TensorRT 8.x supported
- DirectML supported via Windows ML
- GPU inference optional - CPU baseline required first