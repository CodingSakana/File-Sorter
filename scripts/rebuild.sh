#!/bin/bash
set -euo pipefail

rm -rf build
mkdir -p build

# Configure the project: Use Ninja generator, specify source directory as '.', and build directory as 'build'
cmake -G Ninja -S . -B build

# Build the project: Use maximum available CPU cores
cmake --build build --config Release -j$(nproc)