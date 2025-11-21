/**
 * Geom-Core Solarpunk Viewer
 * Three.js + WASM Integration for 3D Printability Analysis
 */

// ============================================
// Global State
// ============================================

const state = {
    wasmModule: null,
    analyzer: null,
    scene: null,
    camera: null,
    renderer: null,
    controls: null,
    meshObject: null,
    currentFile: null,
    currentGeometry: null,
    autoRotate: false,
    showWireframe: false,
    visualMode: 'overhang'
};

// ============================================
// WASM Initialization
// ============================================

async function initWASM() {
    showLoading('loadingIndicator', true);
    console.log('Initializing WASM module...');

    try {
        // Wait for the Emscripten module to be ready
        state.wasmModule = await Module();
        state.analyzer = new state.wasmModule.Analyzer();
        console.log('WASM module loaded successfully!');
        showLoading('loadingIndicator', false);
    } catch (error) {
        console.error('Failed to load WASM module:', error);
        alert('Failed to load WASM engine. Please check console for details.');
        showLoading('loadingIndicator', false);
    }
}

// ============================================
// Three.js Scene Setup
// ============================================

function initThreeJS() {
    const canvas = document.getElementById('canvas3d');
    const viewport = canvas.parentElement;

    // Scene
    state.scene = new THREE.Scene();
    state.scene.background = new THREE.Color(0xe8f5e9);

    // Camera
    state.camera = new THREE.PerspectiveCamera(
        60,
        viewport.clientWidth / viewport.clientHeight,
        0.1,
        1000
    );
    state.camera.position.set(100, 100, 100);

    // Renderer
    state.renderer = new THREE.WebGLRenderer({
        canvas: canvas,
        antialias: true,
        alpha: true
    });
    state.renderer.setSize(viewport.clientWidth, viewport.clientHeight);
    state.renderer.setPixelRatio(window.devicePixelRatio);
    state.renderer.shadowMap.enabled = true;

    // Orbit Controls
    state.controls = new THREE.OrbitControls(state.camera, canvas);
    state.controls.enableDamping = true;
    state.controls.dampingFactor = 0.05;
    state.controls.screenSpacePanning = true;

    // Lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
    state.scene.add(ambientLight);

    const directionalLight1 = new THREE.DirectionalLight(0xffffff, 0.8);
    directionalLight1.position.set(100, 200, 100);
    directionalLight1.castShadow = true;
    state.scene.add(directionalLight1);

    const directionalLight2 = new THREE.DirectionalLight(0x87ceeb, 0.4);
    directionalLight2.position.set(-100, -50, -100);
    state.scene.add(directionalLight2);

    // Grid Helper
    const gridHelper = new THREE.GridHelper(200, 20, 0x4A7C59, 0xC9E4CA);
    state.scene.add(gridHelper);

    // Axes Helper
    const axesHelper = new THREE.AxesHelper(50);
    state.scene.add(axesHelper);

    // Handle window resize
    window.addEventListener('resize', onWindowResize);

    // Start animation loop
    animate();

    console.log('Three.js scene initialized');
}

function onWindowResize() {
    const viewport = document.querySelector('.viewport');
    state.camera.aspect = viewport.clientWidth / viewport.clientHeight;
    state.camera.updateProjectionMatrix();
    state.renderer.setSize(viewport.clientWidth, viewport.clientHeight);
}

function animate() {
    requestAnimationFrame(animate);

    if (state.autoRotate && state.meshObject) {
        state.meshObject.rotation.z += 0.005;
    }

    state.controls.update();
    state.renderer.render(state.scene, state.camera);
}

// ============================================
// File Upload Handling
// ============================================

function setupFileUpload() {
    const dropZone = document.getElementById('dropZone');
    const fileInput = document.getElementById('fileInput');
    const browseBtn = document.getElementById('browseBtn');

    // Click to browse
    dropZone.addEventListener('click', () => fileInput.click());
    browseBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        fileInput.click();
    });

    // File selected
    fileInput.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (file) handleFile(file);
    });

    // Drag and drop
    dropZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropZone.classList.add('drag-over');
    });

    dropZone.addEventListener('dragleave', () => {
        dropZone.classList.remove('drag-over');
    });

    dropZone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropZone.classList.remove('drag-over');
        const file = e.dataTransfer.files[0];
        if (file && file.name.toLowerCase().endsWith('.stl')) {
            handleFile(file);
        } else {
            alert('Please drop a valid STL file');
        }
    });
}

async function handleFile(file) {
    console.log('Loading file:', file.name);
    state.currentFile = file;

    const arrayBuffer = await file.arrayBuffer();
    const uint8Array = new Uint8Array(arrayBuffer);

    // Load into WASM
    if (!state.analyzer) {
        alert('WASM module not ready. Please wait...');
        return;
    }

    // Convert to Emscripten memory
    const dataPtr = state.wasmModule._malloc(uint8Array.length);
    state.wasmModule.HEAPU8.set(uint8Array, dataPtr);

    // Create a string view of the binary data
    const dataString = String.fromCharCode.apply(null, uint8Array);

    const success = state.analyzer.loadSTLFromBytes(dataString);
    state.wasmModule._free(dataPtr);

    if (!success) {
        alert('Failed to load STL file. Please check the console.');
        return;
    }

    const vertexCount = state.analyzer.getVertexCount();
    const triangleCount = state.analyzer.getTriangleCount();

    console.log(`Loaded STL: ${vertexCount} vertices, ${triangleCount} triangles`);

    // Update UI
    document.getElementById('fileName').textContent = file.name;
    document.getElementById('vertexCount').textContent = vertexCount.toLocaleString();
    document.getElementById('triangleCount').textContent = triangleCount.toLocaleString();
    document.getElementById('fileInfo').style.display = 'block';
    document.getElementById('analyzeBtn').disabled = false;

    // Build spatial index for wall thickness analysis
    state.analyzer.buildSpatialIndex();

    // Render the mesh in Three.js
    renderMeshInThreeJS(uint8Array);
}

// ============================================
// STL Parsing & Three.js Rendering
// ============================================

function renderMeshInThreeJS(stlData) {
    // Parse binary STL
    const dataView = new DataView(stlData.buffer);
    const triangleCount = dataView.getUint32(80, true);

    const geometry = new THREE.BufferGeometry();
    const vertices = [];
    const normals = [];

    let offset = 84; // Header (80) + triangle count (4)

    for (let i = 0; i < triangleCount; i++) {
        // Normal (3 floats)
        const nx = dataView.getFloat32(offset, true);
        const ny = dataView.getFloat32(offset + 4, true);
        const nz = dataView.getFloat32(offset + 8, true);
        offset += 12;

        // Three vertices (9 floats)
        for (let j = 0; j < 3; j++) {
            const vx = dataView.getFloat32(offset, true);
            const vy = dataView.getFloat32(offset + 4, true);
            const vz = dataView.getFloat32(offset + 8, true);
            vertices.push(vx, vy, vz);
            normals.push(nx, ny, nz);
            offset += 12;
        }

        // Attribute byte count (2 bytes, unused)
        offset += 2;
    }

    geometry.setAttribute('position', new THREE.Float32BufferAttribute(vertices, 3));
    geometry.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));
    geometry.computeBoundingSphere();

    state.currentGeometry = geometry;

    // Create mesh material
    const material = new THREE.MeshPhongMaterial({
        color: 0x87A96B,
        flatShading: false,
        vertexColors: false,
        side: THREE.DoubleSide
    });

    // Remove old mesh
    if (state.meshObject) {
        state.scene.remove(state.meshObject);
        state.meshObject.geometry.dispose();
        state.meshObject.material.dispose();
    }

    // Add new mesh
    state.meshObject = new THREE.Mesh(geometry, material);
    state.scene.add(state.meshObject);

    // Center camera on mesh
    const boundingSphere = geometry.boundingSphere;
    const center = boundingSphere.center;
    const radius = boundingSphere.radius;

    state.camera.position.set(
        center.x + radius * 1.5,
        center.y + radius * 1.5,
        center.z + radius * 1.5
    );
    state.controls.target.set(center.x, center.y, center.z);
    state.camera.lookAt(center);

    console.log('Mesh rendered in Three.js');
}

// ============================================
// Analysis & Visualization
// ============================================

function runAnalysis() {
    if (!state.analyzer || !state.currentGeometry) {
        alert('No mesh loaded');
        return;
    }

    showLoading('analysisIndicator', true);

    // Use setTimeout to allow UI to update
    setTimeout(() => {
        try {
            const criticalAngle = parseFloat(document.getElementById('criticalAngle').value);
            const visualMode = document.getElementById('visualMode').value;

            if (visualMode === 'overhang') {
                applyOverhangVisualization(criticalAngle);
            } else if (visualMode === 'wallThickness') {
                applyWallThicknessVisualization();
            } else {
                // Reset to normal coloring
                resetMeshColors();
            }

            // Get printability report for stats
            const report = state.analyzer.getPrintabilityReport(criticalAngle, 1.0);
            displayResults(report);

        } catch (error) {
            console.error('Analysis error:', error);
            alert('Analysis failed. Check console for details.');
        } finally {
            showLoading('analysisIndicator', false);
        }
    }, 100);
}

function applyOverhangVisualization(criticalAngle) {
    console.log('Running overhang analysis...');

    // Get overhang map from WASM (Uint8Array)
    const overhangMap = state.analyzer.getOverhangMapJS(criticalAngle);
    const triangleCount = state.analyzer.getTriangleCount();

    console.log('Overhang map size:', overhangMap.length);

    // Create color attribute (3 vertices per triangle, RGB per vertex)
    const colors = new Float32Array(triangleCount * 3 * 3);

    // Color palette
    const colorSafe = new THREE.Color(0x4CAF50);      // Green
    const colorOverhang = new THREE.Color(0xFF5722);  // Red
    const colorGround = new THREE.Color(0x2196F3);    // Blue

    for (let i = 0; i < triangleCount; i++) {
        const status = overhangMap[i];
        let color;

        if (status === 0) {
            color = colorSafe;
        } else if (status === 1) {
            color = colorOverhang;
        } else {
            color = colorGround;
        }

        // Apply color to all 3 vertices of this triangle
        const baseIndex = i * 9; // 3 vertices * 3 channels
        for (let v = 0; v < 3; v++) {
            const idx = baseIndex + v * 3;
            colors[idx] = color.r;
            colors[idx + 1] = color.g;
            colors[idx + 2] = color.b;
        }
    }

    // Apply to geometry
    state.currentGeometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
    state.meshObject.material.vertexColors = true;
    state.meshObject.material.needsUpdate = true;

    console.log('Overhang visualization applied');
}

function applyWallThicknessVisualization() {
    console.log('Running wall thickness analysis...');

    // Get wall thickness map from WASM (Float32Array)
    const thicknessMap = state.analyzer.getWallThicknessMapJS(10.0); // Search up to 10mm
    const vertexCount = state.analyzer.getVertexCount();
    const triangleCount = state.analyzer.getTriangleCount();

    console.log('Thickness map size:', thicknessMap.length);

    // Create color attribute based on thickness
    const colors = new Float32Array(triangleCount * 3 * 3);

    // We need to map vertex indices to triangle vertices
    // Since our geometry is from STL (flat shading), we need to rebuild the index mapping

    // For simplicity, we'll apply a gradient from thin (red) to thick (green)
    const minThickness = 0.0;
    const maxThickness = 5.0; // mm

    // Color gradient: Red (thin) -> Yellow -> Green (thick)
    const colorThin = new THREE.Color(0xFF0000);
    const colorThick = new THREE.Color(0x00FF00);

    // Since STL uses flat triangles (no shared vertices), we have 3 * triangleCount vertices
    for (let i = 0; i < triangleCount * 3; i++) {
        // Map back to original vertex index (this is approximate for STL)
        // For STL files, each triangle vertex is unique, so we use modulo
        const vertexIndex = i % vertexCount;
        const thickness = thicknessMap[vertexIndex];

        // Normalize thickness to 0-1
        const t = Math.min(Math.max((thickness - minThickness) / (maxThickness - minThickness), 0), 1);

        // Lerp between colors
        const color = colorThin.clone().lerp(colorThick, t);

        const idx = i * 3;
        colors[idx] = color.r;
        colors[idx + 1] = color.g;
        colors[idx + 2] = color.b;
    }

    // Apply to geometry
    state.currentGeometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
    state.meshObject.material.vertexColors = true;
    state.meshObject.material.needsUpdate = true;

    console.log('Wall thickness visualization applied');
}

function resetMeshColors() {
    if (state.meshObject) {
        state.meshObject.material.vertexColors = false;
        state.meshObject.material.color.set(0x87A96B);
        state.meshObject.material.needsUpdate = true;
    }
}

function displayResults(report) {
    document.getElementById('overhangArea').textContent =
        report.overhangArea.toFixed(2) + ' mm²';
    document.getElementById('overhangPercent').textContent =
        report.overhangPercentage.toFixed(1) + '%';
    document.getElementById('printabilityScore').textContent =
        report.score.toFixed(1) + ' / 100';

    document.getElementById('resultsPanel').style.display = 'block';
}

// ============================================
// UI Event Handlers
// ============================================

function setupUIHandlers() {
    // Analyze button
    document.getElementById('analyzeBtn').addEventListener('click', runAnalysis);

    // Critical angle slider
    const angleSlider = document.getElementById('criticalAngle');
    const angleValue = document.getElementById('criticalAngleValue');
    angleSlider.addEventListener('input', (e) => {
        angleValue.textContent = e.target.value + '°';
    });

    // Auto-rotate toggle
    document.getElementById('autoRotate').addEventListener('change', (e) => {
        state.autoRotate = e.target.checked;
    });

    // Wireframe toggle
    document.getElementById('showWireframe').addEventListener('change', (e) => {
        state.showWireframe = e.target.checked;
        if (state.meshObject) {
            state.meshObject.material.wireframe = state.showWireframe;
        }
    });

    // Visual mode change
    document.getElementById('visualMode').addEventListener('change', (e) => {
        state.visualMode = e.target.value;
    });
}

// ============================================
// Utility Functions
// ============================================

function showLoading(elementId, show) {
    const element = document.getElementById(elementId);
    element.style.display = show ? 'block' : 'none';
}

// ============================================
// Application Entry Point
// ============================================

async function init() {
    console.log('🌿 Initializing Geom-Core Solarpunk Viewer...');

    // Initialize Three.js scene
    initThreeJS();

    // Setup UI handlers
    setupUIHandlers();
    setupFileUpload();

    // Initialize WASM
    await initWASM();

    console.log('✅ Viewer ready!');
}

// Start the application when the page loads
window.addEventListener('load', init);
