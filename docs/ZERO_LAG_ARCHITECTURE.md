# Zero-Lag Geometry Engine Architecture

> **Goal**: Zero additional calculation lag for Sim4D UX in browser, with external GPU compute for paid tiers

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Sim4D Browser Client                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                      @madfam/geom-core SDK                            │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐  │  │
│  │  │                    ComputeOrchestrator                          │  │  │
│  │  │                                                                 │  │  │
│  │  │   ┌──────────────────┐      ┌──────────────────────────────┐   │  │  │
│  │  │   │  LocalExecutor   │      │     RemoteExecutor           │   │  │  │
│  │  │   │  (WASM Worker)   │      │  (WebSocket → GPU Server)    │   │  │  │
│  │  │   │                  │      │                              │   │  │  │
│  │  │   │  • Primitives    │      │  • Heavy Boolean Ops         │   │  │  │
│  │  │   │  • Quick Booleans│      │  • Complex STEP Import       │   │  │  │
│  │  │   │  • Transforms    │      │  • Large Mesh Analysis       │   │  │  │
│  │  │   │  • Tessellation  │      │  • Parallel Batch Jobs       │   │  │  │
│  │  │   │  • Simple Export │      │  • GPU-accelerated Ops       │   │  │  │
│  │  │   └──────────────────┘      └──────────────────────────────┘   │  │  │
│  │  │              │                           │                      │  │  │
│  │  │              └───────────┬───────────────┘                      │  │  │
│  │  │                          │                                      │  │  │
│  │  │              ┌───────────┴───────────┐                          │  │  │
│  │  │              │   OperationRouter     │                          │  │  │
│  │  │              │   (Smart Decision)    │                          │  │  │
│  │  │              └───────────────────────┘                          │  │  │
│  │  └─────────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        │ WebSocket (paid tier)
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Compute Server (galvana)                             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                    geom-core Native (C++ / GPU)                       │  │
│  │                                                                       │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │  │
│  │  │    OCCT     │  │    CUDA     │  │   OpenCL    │  │  Job Queue  │  │  │
│  │  │   Engine    │  │  Meshing    │  │  Boolean    │  │  Manager    │  │  │
│  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Zero-Lag Strategy

### 1. Operation Classification

| Category | Latency Target | Executor | Examples |
|----------|---------------|----------|----------|
| **Instant** | <16ms | Local WASM | Primitive creation, transforms |
| **Interactive** | <100ms | Local WASM | Simple booleans, tessellation |
| **Background** | <1s | Local or Remote | Complex booleans, STEP import |
| **Heavy** | >1s | Remote (paid) | Large mesh analysis, batch ops |

### 2. Optimistic Local Execution

```typescript
// All operations start locally by default
const result = await geom.primitives.makeBox(100, 100, 100);
// ↪ Runs in local WASM worker, returns in <5ms

// Complex operations use speculative execution
const boolResult = await geom.boolean.subtract(base, [tool1, tool2, tool3]);
// ↪ Starts locally immediately
// ↪ If operation takes >200ms, queues to remote (if paid tier)
// ↪ Returns whichever completes first
```

### 3. Predictive Pre-computation

```typescript
// When user hovers over a tool, pre-compute likely operations
geom.hints.precompute({
  operation: 'boolean.subtract',
  base: selectedShape,
  tools: [hoveredShape]
});
// ↪ Runs in background, result cached
// ↪ When user clicks, result is instant
```

## WASM Module Optimization

### Build Configuration

```cmake
# Optimized for browser performance
set(WASM_LINK_FLAGS
    "-lembind"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME='GeomCore'"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s INITIAL_MEMORY=67108864"      # 64MB initial
    "-s MAXIMUM_MEMORY=536870912"     # 512MB max
    "-s WASM_BIGINT=1"
    "-s ENVIRONMENT=web,worker"
    "-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
    "-s USE_PTHREADS=1"
    "-s PROXY_TO_PTHREAD=1"           # Main thread stays responsive
    "--closure 1"                      # Minify JS glue
    "-O3"                              # Maximum optimization
    "-flto"                            # Link-time optimization
)
```

### Module Split for Lazy Loading

```
geom-core-base.wasm      (~2MB)   - Primitives, transforms, basic analysis
geom-core-boolean.wasm   (~3MB)   - Boolean operations
geom-core-occt.wasm      (~15MB)  - STEP/IGES import, advanced modeling
geom-core-analysis.wasm  (~2MB)   - Printability, mesh analysis
```

### SharedArrayBuffer for Zero-Copy

```cpp
// C++ side: Direct memory access
val getMeshPositions(const std::string& shapeId) {
    float* positions = shapes[shapeId]->getPositions();
    size_t count = shapes[shapeId]->getPositionCount();
    return val(typed_memory_view(count * 3, positions));
}

// JS side: Zero-copy access to mesh data
const positions = geom.getMeshPositions(shapeId);
// positions is a Float32Array view into WASM memory
// No copy, no GC pressure, instant access
```

## Operation Router Logic

```typescript
interface OperationMetrics {
  estimatedComplexity: number;  // 0-1 scale
  inputSize: number;            // bytes
  historicalDuration: number;   // ms (rolling average)
  memoryPressure: number;       // current WASM heap usage
}

function routeOperation(op: Operation, metrics: OperationMetrics): Executor {
  // Always local for simple ops
  if (metrics.estimatedComplexity < 0.2) {
    return 'local';
  }
  
  // Local if under memory pressure (remote won't help)
  if (metrics.memoryPressure > 0.8 && op.type !== 'heavy_compute') {
    return 'local';
  }
  
  // Free tier: always local
  if (!user.hasPaidTier) {
    return 'local';
  }
  
  // Paid tier: route heavy ops to remote
  if (metrics.estimatedComplexity > 0.7 || metrics.historicalDuration > 500) {
    return 'remote';
  }
  
  // Race local vs remote for medium complexity
  if (metrics.estimatedComplexity > 0.4) {
    return 'race';
  }
  
  return 'local';
}
```

## Remote Compute Protocol

### WebSocket Message Format

```typescript
interface ComputeRequest {
  id: string;
  operation: string;
  params: Record<string, unknown>;
  priority: 'realtime' | 'background' | 'batch';
  transferables?: ArrayBuffer[];  // Binary data sent efficiently
}

interface ComputeResponse {
  id: string;
  success: boolean;
  result?: {
    shapeData?: ArrayBuffer;      // Binary shape representation
    meshData?: ArrayBuffer;       // Tessellated mesh
    metadata?: Record<string, unknown>;
  };
  error?: { code: string; message: string };
  metrics?: {
    serverDurationMs: number;
    queueWaitMs: number;
    gpuUtilization: number;
  };
}
```

### Connection Management

```typescript
class RemoteComputeClient {
  private ws: WebSocket;
  private pendingRequests: Map<string, PromiseCallbacks>;
  private reconnectAttempts = 0;
  
  async connect(endpoint: string, authToken: string): Promise<void> {
    this.ws = new WebSocket(endpoint);
    this.ws.binaryType = 'arraybuffer';
    
    // Heartbeat every 30s to keep connection alive
    this.startHeartbeat();
    
    // Auto-reconnect with exponential backoff
    this.ws.onclose = () => this.reconnect();
  }
  
  async execute<T>(request: ComputeRequest): Promise<T> {
    // Send request
    const payload = this.encodeRequest(request);
    this.ws.send(payload);
    
    // Wait for response with timeout
    return new Promise((resolve, reject) => {
      this.pendingRequests.set(request.id, { resolve, reject });
      setTimeout(() => {
        if (this.pendingRequests.has(request.id)) {
          this.pendingRequests.delete(request.id);
          reject(new Error('Remote compute timeout'));
        }
      }, 30000);
    });
  }
}
```

## GPU Compute Server Architecture

### Job Queue with Priority

```python
# galvana/compute_server/job_queue.py
from enum import Enum
from dataclasses import dataclass
from typing import Optional
import asyncio
import heapq

class Priority(Enum):
    REALTIME = 0   # Interactive ops, max 100ms
    BACKGROUND = 1  # User-triggered heavy ops
    BATCH = 2       # Automated analysis jobs

@dataclass
class ComputeJob:
    id: str
    priority: Priority
    operation: str
    params: dict
    created_at: float
    user_tier: str
    
    def __lt__(self, other):
        if self.priority.value != other.priority.value:
            return self.priority.value < other.priority.value
        return self.created_at < other.created_at

class JobQueue:
    def __init__(self, max_concurrent: int = 4):
        self.queue: list[ComputeJob] = []
        self.active_jobs: dict[str, ComputeJob] = {}
        self.max_concurrent = max_concurrent
        self.semaphore = asyncio.Semaphore(max_concurrent)
    
    async def submit(self, job: ComputeJob) -> str:
        heapq.heappush(self.queue, job)
        return job.id
    
    async def process_next(self) -> Optional[ComputeJob]:
        async with self.semaphore:
            if not self.queue:
                return None
            job = heapq.heappop(self.queue)
            self.active_jobs[job.id] = job
            return job
```

### GPU-Accelerated Operations

```cpp
// geom-core/src/gpu/BooleanGPU.cu
#include <cuda_runtime.h>
#include "geom-core/BooleanOps.hpp"

namespace madfam::geom::gpu {

/**
 * GPU-accelerated mesh boolean using parallel voxelization
 */
class GPUBooleanEngine {
public:
    GPUBooleanEngine(int deviceId = 0);
    
    ShapeHandle booleanUnion(
        const std::vector<ShapeHandle>& shapes,
        float voxelSize = 0.1f
    );
    
    ShapeHandle booleanSubtract(
        const ShapeHandle& base,
        const std::vector<ShapeHandle>& tools,
        float voxelSize = 0.1f
    );

private:
    // Parallel voxelization kernel
    __global__ void voxelizeMesh(
        const float* vertices,
        const uint32_t* indices,
        int numTriangles,
        uint8_t* voxelGrid,
        int3 gridDim,
        float3 gridOrigin,
        float voxelSize
    );
    
    // Parallel boolean kernel (union = OR, subtract = AND NOT)
    __global__ void booleanOperation(
        const uint8_t* gridA,
        const uint8_t* gridB,
        uint8_t* result,
        int numVoxels,
        BooleanOp op
    );
    
    // Marching cubes for mesh reconstruction
    __global__ void marchingCubes(
        const uint8_t* voxelGrid,
        float* outputVertices,
        uint32_t* outputIndices,
        int3 gridDim,
        float voxelSize
    );
};

} // namespace madfam::geom::gpu
```

## TypeScript SDK API

```typescript
// @madfam/geom-core

export interface GeomCoreConfig {
  // Execution strategy
  executionMode: 'local-only' | 'hybrid' | 'remote-preferred';
  
  // Local WASM config
  wasmPath?: string;
  workerCount?: number;
  preloadModules?: ('base' | 'boolean' | 'occt' | 'analysis')[];
  
  // Remote compute config (paid tier)
  remoteEndpoint?: string;
  authToken?: string;
  
  // Performance tuning
  enablePrecompute?: boolean;
  cacheSize?: number;  // MB
}

export async function createGeomCore(config: GeomCoreConfig): Promise<GeomCore>;

export interface GeomCore {
  // Modules (same API regardless of execution location)
  primitives: PrimitivesAPI;
  boolean: BooleanAPI;
  features: FeaturesAPI;
  transform: TransformAPI;
  analysis: AnalysisAPI;
  fileIO: FileIOAPI;
  memory: MemoryAPI;
  
  // Execution hints for zero-lag
  hints: {
    precompute(operation: PrecomputeHint): void;
    warmup(modules: string[]): Promise<void>;
    prefetch(shapeIds: string[]): Promise<void>;
  };
  
  // Observability
  metrics: {
    getExecutionStats(): ExecutionStats;
    onSlowOperation(callback: (op: SlowOperationEvent) => void): void;
  };
  
  // Lifecycle
  shutdown(): Promise<void>;
}
```

## Performance Targets

| Operation | Local Target | Remote Target | Notes |
|-----------|-------------|---------------|-------|
| Primitive creation | <5ms | N/A | Always local |
| Simple transform | <2ms | N/A | Always local |
| Tessellation (10K tris) | <20ms | N/A | Always local |
| Boolean (2 shapes) | <50ms | <30ms + RTT | Race if >50ms |
| Boolean (5+ shapes) | <200ms | <50ms + RTT | Prefer remote |
| STEP import (1MB) | <500ms | <100ms + RTT | Remote for large files |
| Printability analysis | <100ms | <30ms | Local for small meshes |
| Batch analysis (10 parts) | N/A | <500ms | Remote only |

## Implementation Phases

### Phase 1: Local WASM Optimization (Week 1)
- [ ] Split WASM into lazy-loaded modules
- [ ] Implement SharedArrayBuffer zero-copy
- [ ] Add SIMD optimizations where supported
- [ ] Benchmark all operations

### Phase 2: Operation Router (Week 2)
- [ ] Implement complexity estimator
- [ ] Build operation history tracking
- [ ] Create race execution logic
- [ ] Add precompute hints API

### Phase 3: Remote Compute Server (Week 3-4)
- [ ] Set up WebSocket server in galvana
- [ ] Implement job queue with priorities
- [ ] Add GPU-accelerated boolean operations
- [ ] Build auth/rate limiting for paid tier

### Phase 4: SDK Integration (Week 5)
- [ ] Create unified TypeScript SDK
- [ ] Add metrics and observability
- [ ] Write migration guide for sim4d
- [ ] Performance testing and tuning

---

*This architecture ensures sim4d users experience zero lag for common operations while enabling paid-tier users to offload heavy computation to GPU servers.*
