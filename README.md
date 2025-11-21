# geom-core

![CI/CD Pipeline](https://img.shields.io/badge/build-passing-brightgreen)
![Python 3.7+](https://img.shields.io/badge/python-3.7+-blue)
![C++17](https://img.shields.io/badge/c++-17-blue)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

High-performance C++ geometry analysis engine for 3D printing, with Python and WebAssembly bindings.

## Features

- **Zero Dependencies**: Core library uses only C++17 standard library for lightweight builds
- **Binary STL Support**: Fast binary STL parser with vertex deduplication
- **STEP File Support**: Optional Open CASCADE Technology (OCCT) integration for CAD file import
- **Mesh Analysis**: Volume calculation, watertight checking, bounding box computation
- **Printability Analysis**: Overhang detection, wall thickness analysis with spatial acceleration
- **Auto-Orientation**: Automatically find optimal mesh orientation to minimize support material
- **Cross-Platform**: Python bindings (Linux, macOS, Windows) and WebAssembly for browsers
- **Production-Ready**: Comprehensive test suite, CI/CD pipeline, proper packaging

## Installation

### Python (via pip)

```bash
pip install geom-core
```

### Build from Source

```bash
git clone https://github.com/madfam-io/geom-core.git
cd geom-core
pip install .
```

### Optional: STEP File Support

To enable STEP file loading (requires Open CASCADE Technology):

**Ubuntu/Debian:**
```bash
sudo apt-get install libocct-data-exchange-dev libocct-ocaf-dev \
  libocct-modeling-data-dev libocct-modeling-algorithms-dev
cd geom-core
cmake -DUSE_OCCT=ON ..
make
```

**macOS:**
```bash
brew install opencascade
cd geom-core
cmake -DUSE_OCCT=ON ..
make
```

## Quick Start

### Python

```python
import geom_core_py as gc

# Load an STL file
analyzer = gc.Analyzer()
analyzer.load_stl("model.stl")

# Or load a STEP file (requires OCCT)
# analyzer.load_step("model.step", linear_deflection=0.1)

# Basic mesh info
print(f"Vertices: {analyzer.get_vertex_count()}")
print(f"Triangles: {analyzer.get_triangle_count()}")
print(f"Volume: {analyzer.get_volume()} mm³")
print(f"Watertight: {analyzer.is_watertight()}")

# Printability analysis
analyzer.build_spatial_index()
report = analyzer.get_printability_report(
    critical_angle_degrees=45.0,
    min_wall_thickness_mm=0.8
)
print(f"Printability Score: {report.score}/100")
print(f"Overhang Area: {report.overhang_area} mm²")
print(f"Thin Wall Vertices: {report.thin_wall_vertex_count}")

# Auto-orientation
result = analyzer.auto_orient()
print(f"Improvement: {result.improvement_percent}%")
print(f"Optimal Up Vector: ({result.optimal_up_vector.x}, "
      f"{result.optimal_up_vector.y}, {result.optimal_up_vector.z})")
```

### WebAssembly

```javascript
const geomCore = await import('./geom_core.js');

const analyzer = new geomCore.Analyzer();
const fileData = new Uint8Array(await file.arrayBuffer());
analyzer.loadSTLFromBytes(fileData);

const report = analyzer.getPrintabilityReport(45.0, 0.8);
console.log(`Score: ${report.score}`);

const orientation = analyzer.autoOrient();
console.log(`Improvement: ${orientation.improvementPercent}%`);
```

## Architecture

### Core Components

- **Vector3**: 3D vector operations (dot product, cross product, normalization)
- **Matrix3**: 3x3 rotation matrices with Rodrigues' formula
- **Mesh**: Binary STL parser, volume calculation, manifold checking
- **Spatial**: AABB tree for ray casting acceleration (Möller-Trumbore intersection)
- **Analyzer**: High-level API wrapping all functionality

### Performance Optimizations

- **Vertex Deduplication**: O(N log N) using `std::map` during STL loading
- **Spatial Acceleration**: AABB tree with BVH for O(log N) ray queries
- **Auto-Orientation**: Tests orientations by rotating test vectors, not mesh vertices (1000x faster)

## Development

### Build Python Bindings

```bash
./scripts/build_python.sh
```

### Build WASM

```bash
# Requires Emscripten SDK
./scripts/build_wasm.sh
```

### Run Tests

```bash
./scripts/run_tests.sh
```

### Run CI Locally

```bash
# Install act (GitHub Actions local runner)
# https://github.com/nektos/act
act -j python-build
```

## Testing

The project includes comprehensive tests for all features:

- `tests/test_mesh.py`: Basic mesh operations (Milestone 2)
- `tests/test_printability.py`: Printability analysis (Milestone 4)
- `tests/test_auto_orient.py`: Auto-orientation (Milestone 5)
- `tests/test_step.py`: STEP file loading API (Milestone 7)

All tests run automatically via GitHub Actions on every push.

## API Reference

### Analyzer Class

#### Mesh Loading
- `load_stl(filepath)`: Load binary STL file
- `load_stl_from_bytes(data)`: Load from memory (WASM-friendly)
- `load_step(filepath, linear_deflection=0.1, angular_deflection=0.5)`: Load STEP/STP file (requires OCCT)

#### Mesh Properties
- `get_vertex_count()`: Number of vertices
- `get_triangle_count()`: Number of triangles
- `get_volume()`: Volume in mm³
- `is_watertight()`: Check if mesh is manifold
- `get_bounding_box()`: Get dimensions as Vector3

#### Printability
- `build_spatial_index()`: Build AABB tree for analysis
- `get_printability_report(critical_angle, min_thickness)`: Analyze printability
- `auto_orient(sample_resolution, critical_angle)`: Find optimal orientation

### PrintabilityReport

- `score`: Overall printability (0-100)
- `overhang_area`: Surface area requiring support (mm²)
- `overhang_percentage`: Percentage of total surface
- `thin_wall_vertex_count`: Number of vertices with thin walls
- `total_surface_area`: Total mesh surface area (mm²)

### OrientationResult

- `optimal_up_vector`: Best orientation direction (Vector3)
- `original_overhang_area`: Before optimization (mm²)
- `optimized_overhang_area`: After optimization (mm²)
- `improvement_percent`: Percentage improvement (0-100)

## Milestones

- ✅ **Milestone 1**: Project scaffold with CMake + Python bindings
- ✅ **Milestone 2**: Mesh analysis (STL parser, volume, watertight)
- ✅ **Milestone 3**: WebAssembly support
- ✅ **Milestone 4**: Printability analysis (overhangs, wall thickness)
- ✅ **Milestone 5**: Auto-orientation algorithm
- ✅ **Milestone 6**: CI/CD pipeline & packaging
- ✅ **Milestone 7**: STEP file support via Open CASCADE Technology

## Technical Details

### Overhang Detection

Surfaces are considered overhangs if their normal angle from vertical exceeds the critical angle (typically 45°):

```
dot_product = normal · up_vector
if dot_product < -cos(critical_angle):
    surface is overhang
```

### Wall Thickness Analysis

For each vertex, cast a ray inward (along negative normal) and check distance to opposite wall:

```
ray = Ray(vertex + ε·normal, -normal)
hit = spatial_tree.ray_cast(ray, max_distance)
if hit.distance < min_thickness:
    vertex has thin wall
```

### Auto-Orientation

Tests 26 candidate orientations (6 cardinals + 12 edges + 8 corners) by rotating the test vector instead of the mesh:

```
for candidate_up_vector in sphere_samples:
    overhang_area = analyze_overhangs(candidate_up_vector)
    if overhang_area < best:
        best = candidate_up_vector
```

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

MIT License - see [LICENSE](LICENSE) for details

## Acknowledgments

- Built with [pybind11](https://github.com/pybind/pybind11) for Python bindings
- WASM support via [Emscripten](https://emscripten.org/)
- Inspired by the need for lightweight 3D printing analysis tools

## Support

- Issues: https://github.com/madfam-io/geom-core/issues
- Documentation: https://github.com/madfam-io/geom-core#readme
