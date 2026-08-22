<#+
.SYNOPSIS
    Benchmark script for V-Morph
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",

    [Parameter(Mandatory=$false)]
    [int]$Duration = 30,

    [Parameter(Mandatory=$false)]
    [string]$Output = "benchmark_results"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectRoot = Join-Path $scriptDir ".."
$buildDir = Join-Path $projectRoot "build" "windows-$Config.ToLower()"
$outputDir = Join-Path $projectRoot $Output

Write-Host "=== V-Morph Benchmark Script ===" -ForegroundColor Cyan

# Check for benchmark executable
$benchmarkExe = Join-Path $buildDir "bin\v-morph_benchmarks.exe"
$latencyExe = Join-Path $buildDir "bin\latency_test.exe"
$mainExe = Join-Path $buildDir "bin\v-morph.exe"

if (-not (Test-Path $mainExe)) {
    Write-Error "Main executable not found. Run build.ps1 first."
    exit 1
}

# Create output directory
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultsFile = Join-Path $outputDir "benchmark_${timestamp}.json"

Write-Host "Running benchmarks for $Duration seconds..." -ForegroundColor Cyan

# Run main application benchmark if available
if (Test-Path $benchmarkExe) {
    Write-Host "Running v-morph_benchmarks.exe..." -ForegroundColor Cyan
    & $benchmarkExe --model "models/test_identity.onnx" --iterations 1000 --warmup 10
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        Write-Error "Benchmark failed"
        exit $exitCode
    }
} else {
    Write-Host "Benchmark executable not found, running main app with --benchmark..." -ForegroundColor Yellow
    & $mainExe --benchmark --duration $Duration --output $resultsFile
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        Write-Error "Benchmark failed"
        exit $exitCode
    }
}

# Run latency test
Write-Host "`nRunning latency test..." -ForegroundColor Cyan
if (Test-Path $latencyExe) {
    & $latencyExe --duration 10 --output (Join-Path $outputDir "latency_${timestamp}.json")
} else {
    & $mainExe --latency-test --duration 10 --output (Join-Path $outputDir "latency_${timestamp}.json")
}

Write-Host "`nBenchmark results saved to:" -ForegroundColor Green
Write-Host "  $outputDir" -ForegroundColor Yellow