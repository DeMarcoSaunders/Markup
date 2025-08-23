#!/bin/bash

# MarkUp UI Build Script
# This script helps build the MarkUp UI library with raylib

set -e  # Exit on any error

echo "🎨 MarkUp UI Build Script"
echo "=========================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Please run this script from the project root."
    exit 1
fi

# Create build directory
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "📁 Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# Check if raylib exists
if [ ! -d "external/raylib" ]; then
    echo "⚠️  Warning: raylib not found in external/raylib/"
    echo "   You can either:"
    echo "   1. Clone raylib: git clone https://github.com/raysan5/raylib.git external/raylib"
    echo "   2. Install raylib system-wide and CMake will find it"
    echo "   3. Set RAYLIB_LIBRARY and RAYLIB_INCLUDE_DIR environment variables"
    echo ""
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Configure with CMake
echo "🔧 Configuring with CMake..."
cd "$BUILD_DIR"

# Default configuration
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"

# Check for debug flag
if [[ "$1" == "debug" ]]; then
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Debug -DENABLE_DEBUG=ON"
    echo "🐛 Building in debug mode..."
fi

# Check for examples flag
if [[ "$1" == "examples" ]] || [[ "$2" == "examples" ]]; then
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_EXAMPLES=ON"
    echo "📚 Building examples..."
fi

# Run CMake
cmake .. $CMAKE_ARGS

# Build
echo "🔨 Building..."
cmake --build . --config Release

echo ""
echo "✅ Build completed successfully!"
echo ""
echo "📁 Build output:"
echo "   - Library: $BUILD_DIR/libmarkup-ui.a"
echo "   - Headers: $BUILD_DIR/include/"
echo ""
echo "🚀 To run examples:"
echo "   cd $BUILD_DIR"
echo "   ./examples/example_name"
echo ""
echo "📖 To install:"
echo "   cd $BUILD_DIR"
echo "   sudo make install"

