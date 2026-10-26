# ==============================================================================
# Stage Piano Studio - Build & Run (Windows)
# Usage:
#   powershell -ExecutionPolicy Bypass -File run.ps1             # compile (incremental) and open the app
#   powershell -ExecutionPolicy Bypass -File run.ps1 -NoBuild    # just open the last build
#   powershell -ExecutionPolicy Bypass -File run.ps1 -Restart    # close a running instance first
#   powershell -ExecutionPolicy Bypass -File run.ps1 -Stress     # run the offline glitch/stress harness instead
#   powershell -ExecutionPolicy Bypass -File run.ps1 -Stress -Seconds 60
# ==============================================================================
param (
    [switch]$NoBuild,
    [switch]$Restart,
    [switch]$Stress,
    [int]$Seconds = 20
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

$appExe = Join-Path $ScriptDir "Stage Piano Studio.exe"
$libraryDir = Join-Path $ScriptDir "library"

if ($Stress) {
    # Real-time paced stress test of the audio engine (no GUI, no audio device):
    # drone only, piano only, then piano+synth+drone. Prints glitch indicators per scenario.
    $cmakeCandidates = @(
        "cmake.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )
    $cmakeExe = $cmakeCandidates | Where-Object { (Get-Command $_ -ErrorAction SilentlyContinue) -or (Test-Path $_) } | Select-Object -First 1
    if (-not $cmakeExe) { Write-Error "CMake not found (install Visual Studio 2022 with C++ / CMake)."; exit 1 }

    $stressDir = Join-Path $ScriptDir "build-stress-rel"
    & $cmakeExe -B $stressDir -S $ScriptDir -DSTRESS_ASAN=OFF | Out-Null
    & $cmakeExe --build $stressDir --config Release --target stress_engine --parallel
    if ($LASTEXITCODE -ne 0) { Write-Error "stress_engine build failed."; exit $LASTEXITCODE }

    & (Join-Path $stressDir "Release\stress_engine.exe") $libraryDir $Seconds
    exit $LASTEXITCODE
}

$running = Get-Process -Name "Stage Piano Studio" -ErrorAction SilentlyContinue
if ($running) {
    if ($Restart) {
        Write-Host ">>> Closing running Stage Piano Studio..." -ForegroundColor Yellow
        $running | ForEach-Object { $_.CloseMainWindow() | Out-Null }
        Start-Sleep -Seconds 2
        $running | Where-Object { -not $_.HasExited } | Stop-Process -Force
    } elseif (-not $NoBuild) {
        Write-Error "Stage Piano Studio is already running (the build cannot replace the .exe). Close it or use -Restart."
        exit 1
    }
}

if (-not $NoBuild) {
    & (Join-Path $ScriptDir "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $appExe)) {
    Write-Error "App not found at $appExe. Run without -NoBuild first."
    exit 1
}

Write-Host ">>> Starting Stage Piano Studio..." -ForegroundColor Cyan
Start-Process -FilePath $appExe -WorkingDirectory $ScriptDir
