#!/usr/bin/env bash
# ==============================================================================
# Stage Piano Studio - Universal Cross-Platform Build Script
# Automatically detects the operating system (macOS, Linux, Windows) and compiles.
#
# Usage:
#   ./build.sh            # Standard Release build
#   ./build.sh --clean    # Clean build directory before compilation
#   ./build.sh --no-test  # Skip running automated unit tests
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

CLEAN_BUILD=false
RUN_TESTS=true

for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN_BUILD=true ;;
        --no-test|--no-tests) RUN_TESTS=false ;;
        *) echo "Unknown option: $arg" ;;
    esac
done

OS_TYPE="$(uname -s)"
case "${OS_TYPE}" in
    Darwin*)  PLATFORM="macOS" ;;
    Linux*)   PLATFORM="Linux" ;;
    MINGW*|MSYS*|CYGWIN*) PLATFORM="Windows" ;;
    *)        PLATFORM="Unknown" ;;
esac

echo "=========================================================="
echo " Stage Piano Studio - Universal Build System"
echo " Detected Platform: ${PLATFORM} (${OS_TYPE})"
echo "=========================================================="

if [[ "${PLATFORM}" == "Windows" ]]; then
    CMAKE_BIN="cmake"
    if ! command -v cmake &>/dev/null; then
        VS_PATHS=(
            "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
            "/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
            "/c/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
            "/c/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
            "/c/Program Files/CMake/bin/cmake.exe"
        )
        for p in "${VS_PATHS[@]}"; do
            if [[ -x "$p" ]]; then
                CMAKE_BIN="$p"
                break
            fi
        done
    fi

    echo ">>> Using CMake: ${CMAKE_BIN}"

    if [[ "${CLEAN_BUILD}" == "true" ]]; then
        echo ">>> Cleaning build directories..."
        rm -rf gui/build
    fi

    echo ">>> Configuring CMake project with ASIO and Low-Latency drivers..."
    "${CMAKE_BIN}" -B gui/build -S gui

    echo ">>> Compiling Stage Piano Studio (Release, parallel)..."
    "${CMAKE_BIN}" --build gui/build --config Release --parallel

    SRC_EXE="gui/build/StagePianoStudio_artefacts/Release/Stage Piano Studio.exe"
    if [[ -f "${SRC_EXE}" ]]; then
        cp -f "${SRC_EXE}" "./Stage Piano Studio.exe"
        echo ">>> Installed executable: ./Stage Piano Studio.exe"

        DESKTOP_DIR="/c/Users/${USER:-lguil}/Desktop"
        if [[ -d "${DESKTOP_DIR}" ]]; then
            cp -f "${SRC_EXE}" "${DESKTOP_DIR}/Stage Piano Studio.exe" 2>/dev/null || true
            echo ">>> Updated Desktop executable: ${DESKTOP_DIR}/Stage Piano Studio.exe"
        fi
    fi

    echo ">>> [SUCCESS] Build completed successfully on Windows!"
    exit 0
fi

# ==============================================================================
# macOS and Linux Build Pipeline
# ==============================================================================
if ! command -v cmake &>/dev/null; then
    echo "[ERROR] 'cmake' is not installed or not in PATH."
    if [[ "${PLATFORM}" == "macOS" ]]; then
        echo "Install via Homebrew: brew install cmake"
    elif [[ "${PLATFORM}" == "Linux" ]]; then
        echo "Install via apt: sudo apt install -y cmake build-essential"
    fi
    exit 1
fi

if [[ "${CLEAN_BUILD}" == "true" ]]; then
    echo ">>> Cleaning build directories..."
    rm -rf build gui/build
fi

echo ">>> 1/2 Configuring and building Standalone GUI App..."
cmake -B gui/build -S gui -DCMAKE_BUILD_TYPE=Release
cmake --build gui/build --config Release --parallel

if [[ "${RUN_TESTS}" == "true" ]]; then
    echo ">>> 2/2 Configuring and executing C++ DSP test suite..."
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --output-on-failure
fi

if [[ "${PLATFORM}" == "macOS" ]]; then
    echo ">>> [SUCCESS] Built: 'gui/build/StagePianoStudio_artefacts/Stage Piano Studio.app'"
elif [[ "${PLATFORM}" == "Linux" ]]; then
    echo ">>> [SUCCESS] Built: 'gui/build/StagePianoStudio_artefacts/Stage Piano Studio'"
fi
echo ">>> All systems operational!"
