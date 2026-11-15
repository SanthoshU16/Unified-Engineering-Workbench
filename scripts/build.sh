#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

echo "========================================================"
echo "    Building Unified Engineering Workbench (C++17)     "
echo "========================================================"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

echo ""
echo "========================================================"
echo "               Running CTest Test Suite                 "
echo "========================================================"
ctest --test-dir build --output-on-failure

echo ""
echo "Build and tests completed successfully!"

