<#+
.SYNOPSIS
    Package script for V-Morph
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",

    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "dist",

    [Parameter(Mandatory=$false)]
    [switch]$IncludeModels,

    [Parameter(Mandatory=$false)]
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectRoot = Join-Path $scriptDir ".."
$buildDir = Join-Path $projectRoot "build" "windows-$Config.ToLower()"
$distDir = Join-Path $projectRoot $OutputDir "v-morph-$Version-windows-x64"

Write-Host "=== V-Morph Package Script ===" -ForegroundColor Cyan
Write-Host "Version: $Version" -ForegroundColor Yellow
Write-Host "Output: $distDir" -ForegroundColor Yellow

if (-not (Test-Path "$buildDir\bin\v-morph.exe")) {
    Write-Error "Executable not found. Run build.ps1 first."
    exit 1
}

# Clean and create dist directory
if (Test-Path $distDir) {
    Remove-Item -Recurse -Force $distDir
}
New-Item -ItemType Directory -Path $distDir | Out-Null

# Copy executable and DLLs
Write-Host "Copying binaries..." -ForegroundColor Cyan
Copy-Item "$buildDir\bin\v-morph.exe" "$distDir\" -Force

# Copy config files
Write-Host "Copying configs..." -ForegroundColor Cyan
New-Item -ItemType Directory -Path "$distDir\configs" | Out-Null
Copy-Item "$projectRoot\configs\*.json" "$distDir\configs\" -Force

# Copy docs
Write-Host "Copying documentation..." -ForegroundColor Cyan
New-Item -ItemType Directory -Path "$distDir\docs" | Out-Null
Copy-Item "$projectRoot\docs\*.md" "$distDir\docs\" -Force
Copy-Item "$projectRoot\README.md" "$distDir\" -Force
Copy-Item "$projectRoot\LICENSE" "$distDir\" -Force

# Copy models if requested
if ($IncludeModels -and (Test-Path "$projectRoot\models")) {
    Write-Host "Copying models..." -ForegroundColor Cyan
    Copy-Item "$projectRoot\models" "$distDir\" -Recurse -Force
}

# Create installer using CPack if available
Write-Host "Creating installer with CPack..." -ForegroundColor Cyan
Set-Location $buildDir
& cpack -G ZIP -D CPACK_PACKAGE_VERSION=$Version
if ($LASTEXITCODE -eq 0) {
    $zipFile = Get-ChildItem $buildDir -Filter "*.zip" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($zipFile) {
        Copy-Item $zipFile.FullName "$distDir\..\v-morph-$Version-windows-x64.zip" -Force
        Write-Host "Installer created: $distDir\..\v-morph-$Version-windows-x64.zip" -ForegroundColor Green
    }
}

Write-Host "`nPackage created at: $distDir" -ForegroundColor Green
Write-Host "Contents:" -ForegroundColor Cyan
Get-ChildItem $distDir -Recurse | ForEach-Object { Write-Host "  $($_.FullName.Substring($distDir.Length + 1))" }