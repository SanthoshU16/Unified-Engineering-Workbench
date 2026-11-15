#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================================"
echo "      Starting Unified Engineering Workbench...        "
echo "========================================================"

if command -v cmake >/dev/null 2>&1 && [ -f "./CMakeLists.txt" ]; then
    echo "[Build] Checking the source and rebuilding changed backend files..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
fi

if [ -f "./build/backend/workbench_server" ]; then
    exec ./build/backend/workbench_server --port 8080 --web-root "$SCRIPT_DIR"
elif [ -f "./workbench_server" ]; then
    exec ./workbench_server --port 8080 --web-root "$SCRIPT_DIR"
else
    echo "[Notice] Pre-built binary not found. Building with CMake..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    exec ./build/backend/workbench_server --port 8080 --web-root "$SCRIPT_DIR"
fi

