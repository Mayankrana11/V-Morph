# Licensing Documentation

## Project License

**V-Morph**: MIT License (see LICENSE file)

## Third-Party Dependencies

### Runtime Dependencies (Linked)

| Library | License | Version | Purpose |
|---------|---------|---------|---------|
| ONNX Runtime | MIT | 1.16+ | ML Inference |
| RtAudio | MIT | 6.0.0 | Audio I/O (fallback) |
| Dear ImGui | MIT | 1.91.8 | UI |
| spdlog | MIT | 1.14.1 | Logging |
| nlohmann/json | MIT | 3.11.3 | Configuration |
| concurrentqueue | BSD-2-Clause | 1.0.4 | Lock-free queues |
| taskflow | MIT | 3.6.0 | Thread pool |

### Build Dependencies (Not Distributed)

| Tool | License |
|------|---------|
| CMake | BSD-3-Clause |
| Ninja | Apache-2.0 |
| GoogleTest | BSD-3-Clause |
| Google Benchmark | Apache-2.0 |

## Model Licenses

### LLVC (Primary Candidate)
- **Code**: MIT (assumed, verify)
- **Weights**: Need verification
- **Dataset**: Need verification

### LPCNet (Vocoder Option)
- **Code**: BSD-3-Clause
- **Weights**: BSD-3-Clause (Mozilla)
- **Dataset**: Public domain speech

### FARGAN (Vocoder Option)
- **Code**: MIT
- **Weights**: MIT/CC-BY (verify)
- **Dataset**: Need verification

### RVC (Optional Backend)
- **Code**: MIT
- **Weights**: Custom (non-commercial in many cases)
- **Dataset**: Various (often non-commercial)

**⚠️ Important**: Model weights and datasets often have different licenses than code. Always verify before commercial distribution.

## Windows Components

| Component | License |
|-----------|---------|
| WASAPI | Part of Windows SDK (Microsoft EULA) |
| MMCSS (avrt.dll) | Part of Windows |
| VB-Cable | Free for personal use, commercial license required |

## Compliance Checklist

### For Distribution
- [ ] All MIT/BSD licenses included in installer
- [ ] ONNX Runtime license included
- [ ] Model weights license verified
- [ ] VB-Cable redistribution rights confirmed
- [ ] No GPL/LGPL dependencies in runtime

### For Commercial Use
- [ ] Verify model weights allow commercial use
- [ ] Verify dataset licenses allow commercial use
- [ ] Obtain VB-Cable commercial license if needed
- [ ] Review Microsoft Windows SDK EULA

## License File Generation

The build system generates `THIRD_PARTY_LICENSES.txt` in the package:

```cmake
# In CMakeLists.txt
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/LicenseTemplate.txt
    ${CMAKE_BINARY_DIR}/THIRD_PARTY_LICENSES.txt
)
```

## Adding New Dependencies

Before adding a dependency:
1. Check license compatibility (MIT/BSD/Apache-2.0 preferred)
2. Avoid GPL/LGPL (viral)
3. Verify model weights license separately
4. Document in this file
5. Add to `cmake/LicenseTemplate.txt`

## License Headers

All source files should include:

```cpp
// V-Morph
// Copyright (c) 2026 Mayank Rana
// SPDX-License-Identifier: MIT
```

## SBOM (Software Bill of Materials)

Generate with:
```bash
# Using cyclonedx-bom
cyclonedx-bom -o sbom.json
```

Or via vcpkg:
```bash
vcpkg x-sbom --format cyclonedx
```