#!/bin/bash
set -e

echo "======================================"
echo "Building geom-core WebAssembly Module"
echo "======================================"

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/wasm"

echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"

# Docker image for Emscripten
EMSDK_IMAGE="emscripten/emsdk:latest"

echo ""
echo "Step 1: Configuring with emcmake..."
docker run --rm \
    -v "$PROJECT_ROOT:/src" \
    -u "$(id -u):$(id -g)" \
    -w /src \
    "$EMSDK_IMAGE" \
    emcmake cmake \
        -S /src \
        -B /src/build/wasm \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_PYTHON_BINDINGS=OFF \
        -DBUILD_WASM_BINDINGS=ON

echo ""
echo "Step 2: Building with emmake..."
docker run --rm \
    -v "$PROJECT_ROOT:/src" \
    -u "$(id -u):$(id -g)" \
    -w /src \
    "$EMSDK_IMAGE" \
    emmake make -C /src/build/wasm -j4

echo ""
echo "======================================"
echo "Build complete!"
echo "======================================"
echo "WASM module location: $BUILD_DIR/wasm/"
echo ""
echo "Files generated:"
ls -lh "$BUILD_DIR/wasm/" 2>/dev/null || echo "  (no files yet - check for errors above)"
echo ""
echo "To test the module:"
echo "  cd $PROJECT_ROOT/web_test"
echo "  python3 -m http.server 8000"
echo "  # Then open http://localhost:8000 in your browser"
echo "======================================"
