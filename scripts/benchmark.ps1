<#+
.SYNOPSIS
    Benchmark script for RT Voice Changer
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

Write-Host "=== RT Voice Changer Benchmark Script ===" -ForegroundColor Cyan

if (-not (Test-Path "$buildDir\bin\rtvc.exe")) {
    Write-Error "Executable not found. Run build.ps1 first."
    exit 1
}

# Create output directory
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultsFile = Join-Path $outputDir "benchmark_${timestamp}.json"

Write-Host "Running benchmarks for $Duration seconds..." -ForegroundColor Cyan

# Run application benchmark
$benchmarkArgs = @(
    "--benchmark",
    "--duration", $Duration,
    "--output", $resultsFile
)

& "$buildDir\bin\rtvc.exe" @benchmarkArgs
$exitCode = $LASTEXITCODE

if ($exitCode -ne 0) {
    Write-Error "Benchmark failed"
    exit $exitCode
}

# Run latency test
Write-Host "`nRunning latency test..." -ForegroundColor Cyan
$latencyArgs = @(
    "--latency-test",
    "--duration", 10,
    "--output", (Join-Path $outputDir "latency_${timestamp}.json")
)

& "$buildDir\bin\rtvc.exe" @latencyArgs

Write-Host "`nBenchmark results saved to:" -ForegroundColor Green
Write-Host "  $resultsFile" -ForegroundColor Yellow
Write-Host "  $outputDir\latency_${timestamp}.json" -ForegroundColor Yellow