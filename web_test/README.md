# WebAssembly Test Harness

This directory contains a minimal test harness for the geom-core WebAssembly module.

## Building the WASM Module

The WASM module is built using Emscripten via Docker (no local installation required):

```bash
cd ..
./scripts/build_wasm.sh
```

This will:
1. Pull the official Emscripten Docker image
2. Configure the build with CMake (emcmake)
3. Compile the C++ code to WebAssembly
4. Generate `geom_core.js` and `geom_core.wasm` in `build/wasm/wasm/`

## Running the Tests

After building, start a local web server:

```bash
cd web_test
python3 -m http.server 8000
```

Then open http://localhost:8000 in your browser.

## What the Test Does

The `index.html` page:

1. Loads the WebAssembly module
2. Generates a binary STL cube programmatically (10×10×10 mm)
3. Creates an `Analyzer` instance
4. Loads the STL data from memory (no file system needed!)
5. Verifies:
   - ✓ 8 vertices (deduplicated)
   - ✓ 12 triangles
   - ✓ Volume = 1000 mm³
   - ✓ Watertight check passes
   - ✓ Bounding box dimensions

All analysis happens **client-side** in the browser with zero server requests!

## Technical Notes

- The STL data is passed as a binary string from JavaScript to C++
- Emscripten's `std::string` can handle binary data directly
- The module uses `-s MODULARIZE=1` for clean async loading
- Memory growth is enabled with `-s ALLOW_MEMORY_GROWTH=1`

## Bundle Size

The generated WASM bundle is lightweight because:
- ✅ Zero dependencies (no OCCT)
- ✅ Pure C++17 standard library
- ✅ Optimized for size with Emscripten flags

Expected bundle size: **< 100 KB** (gzipped)

This meets the PRD requirement of < 5MB for WASM builds.
