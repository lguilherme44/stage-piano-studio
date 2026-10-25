# ==============================================================================
# Stage Piano Studio - Windows PowerShell Build Script
# Usage:
#   powershell -ExecutionPolicy Bypass -File build.ps1
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Clean
# ==============================================================================
param (
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Stage Piano Studio - Windows Build System" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate CMake (PATH or standard Visual Studio locations)
$cmakeCandidates = @(
    "cmake.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\CMake\bin\cmake.exe"
)

$cmakeExe = $null
foreach ($c in $cmakeCandidates) {
    if (Get-Command $c -ErrorAction SilentlyContinue) {
        $cmakeExe = $c
        break
    }
    if (Test-Path $c) {
        $cmakeExe = $c
        break
    }
}

if (-not $cmakeExe) {
    Write-Error "CMake could not be found. Please ensure Visual Studio 2022 with C++ / CMake is installed."
    exit 1
}

Write-Host ">>> Using CMake: $cmakeExe" -ForegroundColor Green

$buildDir = Join-Path $ScriptDir "gui\build"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host ">>> Cleaning build directory: $buildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# 2. Configure with ASIO support
Write-Host ">>> Configuring CMake project with native ASIO support..." -ForegroundColor Cyan
& $cmakeExe -B $buildDir -S (Join-Path $ScriptDir "gui")
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
    exit $LASTEXITCODE
}

# 3. Build Release binary
Write-Host ">>> Compiling Stage Piano Studio (Release x64, parallel)..." -ForegroundColor Cyan
& $cmakeExe --build $buildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build compilation failed."
    exit $LASTEXITCODE
}

# 4. Copy artifact to project root & Desktop
$artifactExe = Join-Path $buildDir "StagePianoStudio_artefacts\Release\Stage Piano Studio.exe"
$destExe = Join-Path $ScriptDir "Stage Piano Studio.exe"

if (Test-Path $artifactExe) {
    Copy-Item -Force $artifactExe $destExe
    Write-Host ">>> Executable updated: $destExe" -ForegroundColor Green

    $desktopPath = [System.IO.Path]::Combine($env:USERPROFILE, "Desktop", "Stage Piano Studio.exe")
    if (Test-Path (Split-Path -Parent $desktopPath)) {
        Copy-Item -Force $artifactExe $desktopPath
        Write-Host ">>> Desktop shortcut updated: $desktopPath" -ForegroundColor Green
    }
} else {
    Write-Error "Built executable not found at: $artifactExe"
    exit 1
}

Write-Host ">>> [SUCCESS] Build completed successfully on Windows!" -ForegroundColor Green
