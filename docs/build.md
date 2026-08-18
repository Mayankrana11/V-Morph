# Build Instructions

## Prerequisites

### Required
- **Windows 10/11** (64-bit)
- **Visual Studio 2022** (17.8+) with:
  - Desktop development with C++
  - Windows 10/11 SDK (10.0.22621+)
  - C++ CMake tools for Windows
- **CMake** ≥ 3.25
- **Ninja** (recommended)
- **Git**

### Optional
- **CUDA Toolkit 12.x** - GPU acceleration
- **vcpkg** - Package management
- **VB-Cable** - Virtual audio for Discord

## Installation

### 1. Visual Studio 2022
Download: https://visualstudio.microsoft.com/downloads/

Select workloads:
- ✅ Desktop development with C++
- ✅ Windows 10/11 SDK (latest)

### 2. CMake & Ninja
```powershell
winget install Kitware.CMake Ninja-build.Ninja
```

Verify:
```powershell
cmake --version
ninja --version
```

### 3. vcpkg (Recommended)
```powershell
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

Install dependencies:
```powershell
.\vcpkg install onnxruntime:x64-windows
```

### 4. CUDA Toolkit (Optional)
Download: https://developer.nvidia.com/cuda-toolkit

Select: CUDA 12.x, cuDNN, TensorRT

## Building

### Using Build Script (Recommended)
```powershell
# Release build
.\scripts\build.ps1 -Config Release

# Debug build with tests
.\scripts\build.ps1 -Config Debug -Tests

# With benchmarks
.\scripts\build.ps1 -Config Release -Benchmarks
```

### Manual CMake
```powershell
# Configure
cmake --preset windows-release

# Build
cmake --build --preset windows-release

# Run tests
ctest --preset windows-release
```

### Presets Available
| Preset | Config | Tests | Benchmarks |
|--------|--------|-------|------------|
| windows-debug | Debug | ✅ | ❌ |
| windows-release | Release | ❌ | ✅ |
| windows-relwithdebinfo | RelWithDebInfo | ✅ | ✅ |

## Configuration

### Default Config
Edit `configs/default.json`:
```json
{
    "audio": {
        "sample_rate": 48000,
        "buffer_frames": 128,
        "exclusive_mode": true
    },
    "voice": {
        "converter_type": "passthrough",
        "model_path": "",
        "chunk_size_ms": 20
    }
}
```

### Development Config
Use `configs/development.json` for lower latency:
```json
{
    "buffer_frames": 64,
    "chunk_size_ms": 10,
    "auto_start": true
}
```

## Running

### Basic Test
```powershell
# List devices
.\build\windows-release\bin\rtvc.exe --list-devices

# Diagnostics
.\build\windows-release\bin\rtvc.exe --diagnostics

# Run passthrough
.\build\windows-release\bin\rtvc.exe --config configs/default.json
```

### With Virtual Audio (Discord)
1. Install VB-Cable
2. Configure Windows Sound:
   - Output: CABLE Input
   - Input (Discord): CABLE Output
3. Run:
```powershell
.\build\windows-release\bin\rtvc.exe ^
    --input-device "{your_mic_guid}" ^
    --output-device "{cable_input_guid}"
```

## Testing

### Unit Tests
```powershell
.\scripts\test.ps1 -Config Release
```

### Benchmarks
```powershell
.\scripts\benchmark.ps1 -Config Release -Duration 60
```

### Latency Test
```powershell
.\build\windows-release\bin\rtvc.exe --latency-test --duration 30
```

## Packaging

### Create Distribution
```powershell
.\scripts\package.ps1 -Config Release -Version 0.1.0
```

Output: `dist/rtvc-0.1.0-windows-x64/`

### Create Installer (ZIP)
```powershell
.\scripts\package.ps1 -Config Release -Version 0.1.0
```

Output: `dist/rtvc-0.1.0-windows-x64.zip`

## CI/CD

### GitHub Actions (Example)
```yaml
name: Build
on: [push, pull_request]
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup vcpkg
        run: |
          git clone https://github.com/microsoft/vcpkg
          ./vcpkg/bootstrap-vcpkg.bat
          ./vcpkg/vcpkg install onnxruntime:x64-windows
      - name: Configure
        run: cmake --preset windows-release
      - name: Build
        run: cmake --build --preset windows-release
      - name: Test
        run: ctest --preset windows-release
```

## Troubleshooting Build

### "onnxruntime not found"
```powershell
# Ensure vcpkg integration
vcpkg integrate install

# Or set manually
cmake -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake ...
```

### "Could not find CUDA"
```powershell
# Install CUDA Toolkit, not just driver
# Verify: nvcc --version
```

### "Access denied" on build
- Run PowerShell as Administrator
- Or disable Controlled Folder Access for build dir

### Out of memory during build
- Reduce parallel jobs: `cmake --build --preset windows-release -j 4`
- Close other applications

## Development Workflow

### Code Style
- C++20
- clang-format (LLVM style)
- Run: `clang-format -i src/**/*.cpp src/**/*.h`

### Adding Files
1. Add to appropriate `src/` subdirectory
2. Update `src/CMakeLists.txt`
3. Re-run CMake configure

### Debugging
- Use Visual Studio Debugger (F5)
- Or: `devenv build/windows-release/rtvc.sln`

### Logs
- Debug build: Console output
- Release: `logs/rtvc.log` (if enabled in config)

## Cross-Compilation (Future)

### Linux
```bash
# Install dependencies
sudo apt install libasound2-dev libpulse-dev

# Build
cmake -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux
```

### macOS
```bash
# Install dependencies
brew install cmake ninja portaudio

# Build
cmake -B build/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos
```