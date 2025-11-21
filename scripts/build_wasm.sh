#!/bin/bash
set -e

echo "========================================"
echo "Building geom-core WASM bindings"
echo "========================================"

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Check if emscripten is available
if ! command -v emcmake &> /dev/null; then
    echo "ERROR: Emscripten SDK not found!"
    echo "Please install and activate emsdk:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

# Create build directory
mkdir -p build/wasm
cd build/wasm

# Configure with Emscripten
echo "Configuring CMake with Emscripten..."
emcmake cmake ../.. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PYTHON_BINDINGS=OFF \
    -DBUILD_WASM_BINDINGS=ON

# Build
echo "Building..."
emmake make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Create dist directory and copy artifacts
echo "Copying artifacts to dist/..."
mkdir -p ../../dist
cp wasm/geom_core.js ../../dist/ 2>/dev/null || true
cp wasm/geom_core.wasm ../../dist/ 2>/dev/null || true

echo "========================================"
echo "WASM build complete!"
echo "Artifacts in: dist/"
echo "========================================"

# List built files
ls -lh ../../dist/ 2>/dev/null || true
