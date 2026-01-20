#!/bin/bash
# Build script for Themis Desktop Application

set -e

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

echo ""
echo "Build complete! Run with: ./build/themis_desktop"
