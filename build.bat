@echo off
setlocal
cd /d "%~dp0"

echo ==========================================================
echo  Stage Piano Studio - Windows Build Launcher
echo ==========================================================

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed with code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Stage Piano Studio compiled and ready to use!
pause
