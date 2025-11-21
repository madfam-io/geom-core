# 🌿 Geom-Core Solarpunk Viewer

**Real-time 3D Printability Analysis in Your Browser**

This is a WebGL-based viewer that uses Three.js and WebAssembly to provide instant visual feedback on 3D model printability. Drop an STL file and see which parts need support structures, color-coded in real-time.

## Features

- **Zero-Copy WASM Integration**: Typed Arrays share memory between C++ and JavaScript for maximum performance
- **Overhang Analysis**: Color-codes triangles based on their angle relative to the build platform
  - 🟢 Green: Safe (self-supporting)
  - 🔴 Red: Overhang (needs support)
  - 🔵 Blue: Ground (build platform contact)
- **Wall Thickness Visualization**: Shows wall thickness using color gradients
- **Interactive 3D Viewport**: Rotate, pan, and zoom with intuitive controls
- **Solarpunk Aesthetic**: Nature-inspired design with sustainable vibes

## Quick Start

### 1. Build the WASM Module

From the project root:

```bash
# Create build directory
mkdir -p build-wasm && cd build-wasm

# Configure with Emscripten
emcmake cmake .. -DBUILD_WASM=ON -DCMAKE_BUILD_TYPE=Release

# Build
emmake make -j$(nproc)

# The build outputs:
# - geom-core.js
# - geom-core.wasm
```

### 2. Serve the Viewer

You need a local web server (browsers block WASM loading from `file://` URLs).

**Option A: Using Python**
```bash
cd viewer
python3 -m http.server 8080
```

**Option B: Using Node.js**
```bash
npm install -g http-server
cd viewer
http-server -p 8080
```

**Option C: Using VSCode Live Server**
- Install the "Live Server" extension
- Right-click `index.html` → "Open with Live Server"

### 3. Open in Browser

Navigate to: `http://localhost:8080`

### 4. Load a Model

- Click "Browse Files" or drag-and-drop an STL file
- The model will load and render in 3D
- Click "Run Analysis" to see the overhang visualization

## Usage Guide

### Controls

- **Left Mouse**: Rotate the model
- **Right Mouse**: Pan the camera
- **Scroll Wheel**: Zoom in/out

### Settings

- **Critical Angle**: Adjust the overhang threshold (default: 45°)
  - Lower values = stricter (more overhangs detected)
  - Higher values = more permissive (fewer overhangs)

- **Visualization Mode**:
  - *Overhang Analysis*: Shows which triangles need support
  - *Wall Thickness*: Shows wall thickness with color gradient (red = thin, green = thick)
  - *Normal*: Shows the model without analysis

- **Auto-rotate**: Continuously rotate the model
- **Show wireframe**: Display the mesh wireframe

### Results Panel

After running analysis, you'll see:
- **Overhang Area**: Total surface area requiring support (mm²)
- **Overhang %**: Percentage of total surface area
- **Printability Score**: 0-100 score (higher = more printable)

## Technical Details

### Data Flow

1. **STL Upload** → Parsed in JavaScript → Binary data sent to WASM
2. **WASM Analysis** → Computes per-triangle overhang status → Returns Uint8Array
3. **Three.js Visualization** → Maps array to vertex colors → Renders in WebGL

### Memory Efficiency

The viewer uses **zero-copy** data transfer via `emscripten::typed_memory_view`:
- No copying of large vertex/triangle arrays
- JavaScript TypedArrays share memory with C++ std::vector
- Instant visualization updates even on large meshes

### Compatibility

- **Browsers**: Chrome 90+, Firefox 88+, Edge 90+, Safari 14+
- **WebAssembly**: Requires WASM support (all modern browsers)
- **WebGL**: Requires WebGL 1.0+ (built-in to modern browsers)

## File Structure

```
viewer/
├── index.html       # Main HTML structure
├── style.css        # Solarpunk aesthetic styling
├── app.js           # Three.js + WASM integration
└── README.md        # This file
```

## Development

### Rebuild WASM After Changes

If you modify C++ code:

```bash
cd build-wasm
emmake make -j$(nproc)
# Refresh browser to reload WASM
```

### Debugging

**Check browser console** for detailed logs:
- WASM module loading status
- STL parsing info
- Analysis progress
- Error messages

**Common Issues**:

1. **"Failed to load WASM module"**
   - Ensure `build-wasm/geom-core.js` and `geom-core.wasm` exist
   - Check browser console for network errors
   - Verify you're using a web server (not `file://`)

2. **"Analysis failed"**
   - Check that the STL file is valid binary format
   - Verify the mesh is loaded (check vertex count in UI)
   - Look for C++ error messages in console

3. **Colors not showing**
   - Run analysis at least once
   - Select "Overhang Analysis" mode
   - Ensure the mesh loaded successfully

## Performance Notes

- **Small meshes** (<10k triangles): Instant analysis
- **Medium meshes** (10k-100k triangles): <1 second
- **Large meshes** (>100k triangles): 1-5 seconds

Wall thickness analysis is slower than overhang analysis (requires ray casting).

## Contributing

To add new analysis features:

1. Add C++ method to `Analyzer.cpp`
2. Expose via WASM binding in `WasmEntry.cpp`
3. Add UI controls in `index.html`
4. Implement visualization in `app.js`

## License

Part of the Geom-Core project. See main repository for license details.

---

Built with ❤️ using C++, WebAssembly, and Three.js
