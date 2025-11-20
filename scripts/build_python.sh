#!/bin/bash
set -e

echo "=================================="
echo "Building geom-core Python Bindings"
echo "=================================="

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo ""
echo "Configuring CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PYTHON_BINDINGS=ON

# Build
echo ""
echo "Building..."
cmake --build . --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=================================="
echo "Build complete!"
echo "=================================="
echo "Python module location: $BUILD_DIR/python/geom_core_py.so"
echo ""
echo "To test the module:"
echo "  cd $PROJECT_ROOT"
echo "  PYTHONPATH=$BUILD_DIR/python python3 tests/test_basic.py"
echo "=================================="
