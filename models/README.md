# Models Directory

This directory contains voice conversion models and their manifests.

## Structure

```
models/
├── manifests/          # Model metadata (JSON)
├── llvc/              # LLVC models (primary)
├── rvc/               # RVC models (optional)
└── custom/            # User models
```

## Model Manifest Format

Each model requires a manifest file in `manifests/`:

```json
{
    "name": "llvc-base-16k",
    "version": "1.0.0",
    "format": "onnx",
    "sample_rate": 16000,
    "channels": 1,
    "input_shape": [1, 1, 160],
    "output_shape": [1, 1, 160],
    "streaming": true,
    "lookahead_ms": 10,
    "license": "MIT",
    "sha256": "abc123...",
    "runtime": "onnxruntime",
    "description": "LLVC base model for 16kHz streaming VC",
    "requirements": {
        "min_ort_version": "1.16",
        "providers": ["CPU", "CUDA"],
        "memory_mb": 50
    },
    "speaker_embedding": {
        "required": false,
        "dim": 256
    }
}
```

## Downloading Models

Models are NOT included in the repository. Download manually:

### LLVC (Recommended)
```bash
# When available
mkdir -p models/llvc
# Download from official release
# Place .onnx file in models/llvc/
# Create manifest in models/manifests/llvc-base-16k.json
```

### RVC (Optional)
```bash
mkdir -p models/rvc
# Download from Hugging Face or RVC repo
# Note: Check license for commercial use
```

## Model Verification

The application verifies:
1. File exists
2. SHA256 matches manifest
3. ONNX model loads without errors
4. Input/output shapes match
5. Required operators supported
6. License compatibility

## Adding Custom Models

1. Export model to ONNX (opset ≤ 18)
2. Place in `models/custom/your_model.onnx`
3. Create manifest in `models/manifests/your_model.json`
4. Select in UI or config: `"model_path": "models/custom/your_model.onnx"`

## Model Requirements for V-Morph

| Requirement | Specification |
|-------------|---------------|
| Format | ONNX (opset 13-18) |
| Input | Float32, [batch, channels, samples] or [batch, channels, frames, features] |
| Output | Float32, same shape as input (streaming) |
| Sample Rate | 16kHz or 24kHz preferred |
| Channels | 1 (mono) |
| Streaming | Must maintain state between chunks |
| Lookahead | < 20ms |
| CPU RTF | < 0.5 (2x real-time) |
| Model Size | < 100MB |

## License Compliance

Before adding a model:
- [ ] Code license allows commercial use
- [ ] Weights license allows commercial use
- [ ] Dataset license allows commercial use
- [ ] No conflicting dependencies

Document in manifest `license` field.