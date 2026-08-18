<#+
.SYNOPSIS
    Build script for RT Voice Changer
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",

    [Parameter(Mandatory=$false)]
    [switch]$Clean,

    [Parameter(Mandatory=$false)]
    [switch]$Tests,

    [Parameter(Mandatory=$false)]
    [switch]$Benchmarks,

    [Parameter(Mandatory=$false)]
    [string]$Generator = "Ninja",

    [Parameter(Mandatory=$false)]
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectRoot = Join-Path $scriptDir ".."
$buildDir = Join-Path $projectRoot "build" "windows-$Config.ToLower()"

Write-Host "=== RT Voice Changer Build Script ===" -ForegroundColor Cyan
Write-Host "Config: $Config" -ForegroundColor Yellow
Write-Host "Build Dir: $buildDir" -ForegroundColor Yellow

# Check for required tools
$tools = @("cmake", "ninja", "git")
foreach ($tool in $tools) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "$tool not found in PATH. Please install it first."
        exit 1
    }
}

# Check for vcpkg
$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot -or -not (Test-Path "$vcpkgRoot/scripts/buildsystems/vcpkg.cmake")) {
    Write-Warning "VCPKG_ROOT not set or vcpkg.cmake not found. Dependencies may not be found."
}

# Clean if requested
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# Configure
Write-Host "Configuring with CMake..." -ForegroundColor Cyan
$cmakeArgs = @(
    "-S", $projectRoot,
    "-B", $buildDir,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$Config"
)

if ($Tests) { $cmakeArgs += "-DBUILD_TESTS=ON" }
if ($Benchmarks) { $cmakeArgs += "-DBUILD_BENCHMARKS=ON" }
if ($vcpkgRoot) { $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake" }

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configure failed"
    exit $LASTEXITCODE
}

# Build
Write-Host "Building..." -ForegroundColor Cyan
$buildArgs = @(
    "--build", $buildDir,
    "--config", $Config
)
if ($Verbose) { $buildArgs += "--verbose" }

& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit $LASTEXITCODE
}

Write-Host "Build successful!" -ForegroundColor Green
Write-Host "Executable: $buildDir\bin\rtvc.exe" -ForegroundColor Green