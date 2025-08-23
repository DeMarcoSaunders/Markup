@echo off
REM MarkUp UI Build Script for Windows
REM This script helps build the MarkUp UI library with raylib

echo 🎨 MarkUp UI Build Script
echo ==========================

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo ❌ Error: CMakeLists.txt not found. Please run this script from the project root.
    pause
    exit /b 1
)

REM Create build directory
if not exist "build" (
    echo 📁 Creating build directory...
    mkdir build
)

REM Check if raylib exists
if not exist "external\raylib" (
    echo ⚠️  Warning: raylib not found in external\raylib\
    echo    You can either:
    echo    1. Clone raylib: git clone https://github.com/raysan5/raylib.git external\raylib
    echo    2. Install raylib system-wide and CMake will find it
    echo    3. Set RAYLIB_LIBRARY and RAYLIB_INCLUDE_DIR environment variables
    echo.
    set /p "continue=Continue anyway? (y/N): "
    if /i not "%continue%"=="y" (
        pause
        exit /b 1
    )
)

REM Configure with CMake
echo 🔧 Configuring with CMake...
cd build

REM Default configuration
set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Release

REM Check for debug flag
if "%1"=="debug" (
    set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Debug -DENABLE_DEBUG=ON
    echo 🐛 Building in debug mode...
)

REM Check for examples flag
if "%1"=="examples" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_EXAMPLES=ON
    echo 📚 Building examples...
)
if "%2"=="examples" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_EXAMPLES=ON
    echo 📚 Building examples...
)

REM Run CMake
cmake .. %CMAKE_ARGS%

REM Build
echo 🔨 Building...
cmake --build . --config Release

echo.
echo ✅ Build completed successfully!
echo.
echo 📁 Build output:
echo    - Library: build\markup-ui.lib
echo    - Headers: build\include\
echo.
echo 🚀 To run examples:
echo    cd build
echo    examples\example_name.exe
echo.
echo 📖 To install:
echo    cd build
echo    cmake --install .
echo.
pause

