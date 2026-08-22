<#+
.SYNOPSIS
    Test script for V-Morph
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",

    [Parameter(Mandatory=$false)]
    [string]$Filter = "",

    [Parameter(Mandatory=$false)]
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectRoot = Join-Path $scriptDir ".."
$buildDir = Join-Path $projectRoot "build" "windows-$Config.ToLower()"

Write-Host "=== V-Morph Test Script ===" -ForegroundColor Cyan

if (-not (Test-Path "$buildDir\bin\v-morph.exe")) {
    Write-Error "Executable not found. Run build.ps1 first."
    exit 1
}

# Run unit tests via CTest
Write-Host "Running CTest..." -ForegroundColor Cyan
$ctestArgs = @(
    "--test-dir", $buildDir,
    "-C", $Config,
    "--output-on-failure"
)
if ($Filter) { $ctestArgs += "-R", $Filter }
if ($Verbose) { $ctestArgs += "--verbose" }

& ctest @ctestArgs
$exitCode = $LASTEXITCODE

# Also run the application diagnostics
Write-Host "`nRunning diagnostics..." -ForegroundColor Cyan
& "$buildDir\bin\rtvc.exe" --diagnostics

if ($exitCode -ne 0) {
    Write-Error "Tests failed"
    exit $exitCode
}

Write-Host "All tests passed!" -ForegroundColor Green