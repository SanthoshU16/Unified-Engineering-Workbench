#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

echo "Checking the current source build..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo "Running all headless computational tasks and exporting evidence to results/..."
mkdir -p results
./build/backend/workbench_server --run-all-headless

echo "Results successfully generated in results/:"
ls -lh results/

