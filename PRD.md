# Product Requirements Document (PRD): geom-core

| **Project Name** | `geom-core` |
| :--- | :--- |
| **Version** | 1.0 (Inception) |
| **Status** | **Draft** |
| **Owner** | MADFAM Co-labs |
| **License** | LGPL v2.1 (Compatible with OCCT) or MIT (if decoupled) |
| **Repository** | `github.com/madfam-io/geom-core` (Pending) |

---

## 1. Executive Summary
**geom-core** is a high-performance, cross-platform geometry analysis library written in C++. Its primary purpose is to mathematically evaluate 3D models for manufacturability (specifically additive manufacturing).

It follows the **"Write Once, Run Everywhere"** doctrine:
1.  **Core:** C++ logic backed by Open CASCADE Technology (OCCT).
2.  **Web:** Compiled to WebAssembly (WASM) for client-side real-time analysis in **Sim4D**.
3.  **Server:** Compiled to Python bindings for batch processing in **BlueprintTube** and **ForgeSight**.

This library is the "Single Source of Truth" for MADFAM. If `geom-core` says a part is printable, both our search engine and our design tool agree.

---

## 2. Problem Statement
Currently, printability analysis is fragmented:
* **Silos:** The analysis logic in a web CAD tool usually differs from the logic in a backend search crawler, leading to inconsistent results.
* **Performance:** Most web-based analysis is written in JavaScript, which is too slow for complex B-Rep geometry.
* **Access:** High-quality manufacturing analysis algorithms are locked behind proprietary, expensive software (e.g., Netfabb, Magics).

**The Solarpunk Goal:** Make industrial-grade analysis accessible to anyone with a browser or a terminal.

---

## 3. Strategic Goals & User Stories

### Goals
* **Parity:** Ensure identical analysis results in the Browser (Sim4D) and Server (BlueprintTube).
* **Performance:** Analyze an average STL/STEP file (<50MB) in under 2 seconds.
* **Portability:** Zero-dependency setup for the Python wheels (easy `pip install`).

### User Stories
* **As a Sim4D User:** I want to see real-time red flags on my model (e.g., "Wall too thin") as I edit it, without waiting for a server upload.
* **As BlueprintTube:** I need to batch-process 10,000 models overnight to generate "Printability Scores" and search tags.
* **As Primavera3D (Internal):** I want to reject uploaded files automatically if they are non-manifold, saving manual review time.
* **As an Open Source Dev:** I want to import `geom-core` to build my own slicing or nesting tool.

---

## 4. Technical Architecture

### 4.1 The Stack


* **Language:** C++17 (Standard).
* **Geometry Kernel:** Open CASCADE Technology (OCCT) - *Linked dynamically or statically depending on target.*
* **Bindings:**
    * **Python:** `pybind11` (Exposes C++ classes as Python objects).
    * **WASM:** `Emscripten` (Exposes C++ functions to JavaScript/TypeScript).

### 4.2 Key Modules
1.  **`MeshAnalysis`:** (Fast) Operations on triangulated data (STL/OBJ).
    * Checks: Manifoldness, Holes, Inverted Normals, Volume.
2.  **`BRepAnalysis`:** (Precise) Operations on parametric data (STEP/IGES).
    * Checks: Face validity, Tolerance checks, sliver faces.
3.  **`Printability`:** (Heuristics) The "Score" logic.
    * Checks: Overhang angle detection, Min wall thickness, Aspect ratio.

---

## 5. Functional Requirements (The API)

The library must expose the following functions to both Python and JS:

### Phase 1: The Basics (Sanity Check)
* `load_model(buffer)`: Ingest STEP, STL, OBJ.
* `get_bounding_box()`: Returns dimensions (X, Y, Z).
* `get_volume()`: Returns cubic mm.
* `is_watertight()`: Boolean check (Manifold).
* `get_surface_area()`: Returns square mm.

### Phase 2: The "Printability Score" (The Value Add)
* `analyze_overhangs(critical_angle)`: Returns % of surface requiring support structures.
* `analyze_wall_thickness(min_mm)`: Returns heat map data or boolean failure if walls < `min_mm`.
* `calculate_printability_score()`: Returns a float (0.0 to 100.0) based on a weighted algorithm of the above factors.

### Phase 3: Advanced (Future)
* `auto_orient()`: Returns the optimal X,Y,Z rotation to minimize support material.
* `estimate_print_time(machine_profile)`: Rough time estimation.

---

## 6. Non-Functional Requirements
* **WASM Bundle Size:** Must be optimized (target < 5MB gzipped without OCCT full kernel, or use lazy loading). *Note: Including full OCCT in WASM is heavy (20MB+). Strategy: Create a stripped-down build of OCCT containing only necessary math libraries.*
* **Thread Safety:** The Python module must release the GIL (Global Interpreter Lock) during heavy calculations to allow concurrency in Enclii.
* **Error Handling:** Must not crash the host application (Sim4D/Server) on bad geometry. Return graceful error codes.

---

## 7. Roadmap & Execution

### Milestone 1: "Hello World" (Week 1)
* Repo initialized.
* CMake build system set up.
* Simple function `add(a, b)` compiling to both WASM and Python.
* **Deliverable:** A working CI pipeline.

### Milestone 2: Mesh Basics (Weeks 2-4)
* Integrate basic mesh processing.
* Implement `is_watertight` and `get_volume`.
* **Dogfooding:** Sim4D team integrates the WASM build to test loading files.

### Milestone 3: The "Score" & Python (Weeks 5-8)
* Implement the Scoring Algorithm.
* Generate Python wheels.
* **Dogfooding:** BlueprintTube team starts batch-processing test datasets.

---

## 8. Success Metrics
1.  **Sim4D Performance:** Analysis of a standard "Benchy" boat takes < 500ms in the browser.
2.  **BlueprintTube Stability:** Can process 1,000 files without a memory leak or crash.
3.  **Community:** 50+ GitHub Stars in the first month of public announcement (Validation of interest).
