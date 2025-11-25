#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "Types.hpp"
#include "ShapeRegistry.hpp"

namespace madfam::geom::cad {

/**
 * @brief Main CAD engine interface for the unified geometry module
 * 
 * This is the primary entry point for all geometry operations.
 * Designed for zero-lag browser execution with optional remote compute offloading.
 */
class Engine {
public:
    Engine();
    ~Engine();
    
    // ===========================================================================
    // Lifecycle
    // ===========================================================================
    
    /**
     * @brief Initialize the engine (load OCCT if available)
     * @return true if initialized successfully
     */
    bool initialize();
    
    /**
     * @brief Check if engine is ready for operations
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief Get engine version string
     */
    std::string getVersion() const;
    
    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();
    
    // ===========================================================================
    // Primitives - Always <5ms, local execution only
    // ===========================================================================
    
    Result<ShapeHandle> makeBox(const BoxParams& params);
    Result<ShapeHandle> makeSphere(const SphereParams& params);
    Result<ShapeHandle> makeCylinder(const CylinderParams& params);
    Result<ShapeHandle> makeCone(const ConeParams& params);
    Result<ShapeHandle> makeTorus(const TorusParams& params);
    
    // 2D Primitives (for profiles)
    Result<ShapeHandle> makeLine(const Vector3& start, const Vector3& end);
    Result<ShapeHandle> makeCircle(const Vector3& center, double radius, const Vector3& normal = {0, 0, 1});
    Result<ShapeHandle> makeRectangle(const Vector3& center, double width, double height);
    Result<ShapeHandle> makePolygon(const std::vector<Vector3>& points, bool closed = true);
    Result<ShapeHandle> makeArc(const Vector3& start, const Vector3& middle, const Vector3& end);
    Result<ShapeHandle> makeWire(const std::vector<std::string>& edgeIds);
    
    // ===========================================================================
    // Boolean Operations - Complexity varies, may route to remote
    // ===========================================================================
    
    Result<ShapeHandle> booleanUnion(const BooleanUnionParams& params);
    Result<ShapeHandle> booleanSubtract(const BooleanSubtractParams& params);
    Result<ShapeHandle> booleanIntersect(const BooleanIntersectParams& params);
    
    // Convenience overloads
    Result<ShapeHandle> booleanUnion(const std::string& id1, const std::string& id2);
    Result<ShapeHandle> booleanSubtract(const std::string& baseId, const std::string& toolId);
    Result<ShapeHandle> booleanIntersect(const std::string& id1, const std::string& id2);
    
    // ===========================================================================
    // Feature Operations - May route to remote for complex shapes
    // ===========================================================================
    
    Result<ShapeHandle> extrude(const ExtrudeParams& params);
    Result<ShapeHandle> revolve(const RevolveParams& params);
    Result<ShapeHandle> sweep(const SweepParams& params);
    Result<ShapeHandle> loft(const LoftParams& params);
    
    Result<ShapeHandle> fillet(const FilletParams& params);
    Result<ShapeHandle> chamfer(const ChamferParams& params);
    Result<ShapeHandle> shell(const ShellParams& params);
    Result<ShapeHandle> offset(const OffsetParams& params);
    
    // ===========================================================================
    // Transforms - Always <2ms, local execution only
    // ===========================================================================
    
    Result<ShapeHandle> translate(const TranslateParams& params);
    Result<ShapeHandle> rotate(const RotateParams& params);
    Result<ShapeHandle> scale(const ScaleParams& params);
    Result<ShapeHandle> mirror(const MirrorParams& params);
    Result<ShapeHandle> transform(const MatrixTransformParams& params);
    Result<ShapeHandle> copy(const std::string& shapeId);
    
    // ===========================================================================
    // Analysis - Complexity varies by mesh size
    // ===========================================================================
    
    Result<double> getVolume(const std::string& shapeId);
    Result<double> getSurfaceArea(const std::string& shapeId);
    Result<BoundingBox> getBoundingBox(const std::string& shapeId);
    Result<Vector3> getCenterOfMass(const std::string& shapeId);
    Result<bool> isWatertight(const std::string& shapeId);
    Result<bool> isSolid(const std::string& shapeId);
    
    // Tessellation for visualization (always local for responsiveness)
    Result<MeshData> tessellate(const std::string& shapeId, const TessellateOptions& options = {});
    
    // ===========================================================================
    // File I/O - Large files may route to remote
    // ===========================================================================
    
    Result<ShapeHandle> importSTEP(const std::string& data);
    Result<ShapeHandle> importSTEPFromFile(const std::string& filepath);
    Result<ShapeHandle> importSTL(const std::string& data);
    Result<ShapeHandle> importSTLFromFile(const std::string& filepath);
    
    Result<std::string> exportSTEP(const std::string& shapeId);
    Result<std::string> exportSTL(const std::string& shapeId, bool binary = true);
    Result<std::string> exportOBJ(const std::string& shapeId);
    
    // ===========================================================================
    // Memory Management
    // ===========================================================================
    
    bool disposeShape(const std::string& shapeId);
    void disposeAll();
    size_t getShapeCount() const;
    size_t getMemoryUsage() const;
    ShapeHandle getShapeHandle(const std::string& shapeId) const;
    std::vector<ShapeHandle> getAllShapes() const;
    
    // ===========================================================================
    // Zero-Lag Optimization Hints
    // ===========================================================================
    
    /**
     * @brief Precompute an operation in the background
     * 
     * Call this when user hovers over a tool or shows intent.
     * Result will be cached and returned instantly when the actual operation is called.
     */
    void precompute(const PrecomputeHint& hint);
    
    /**
     * @brief Warm up specific modules (load lazily-loaded WASM)
     */
    void warmup(const std::vector<std::string>& modules);
    
    /**
     * @brief Prefetch shapes into fast cache
     */
    void prefetch(const std::vector<std::string>& shapeIds);
    
    /**
     * @brief Estimate operation complexity before execution
     */
    ComplexityEstimate estimateComplexity(const std::string& operation, 
                                          const std::vector<std::string>& shapeIds) const;
    
    /**
     * @brief Cancel any pending precomputation for an operation
     */
    void cancelPrecompute(const std::string& operationKey);
    
    // ===========================================================================
    // Health & Metrics
    // ===========================================================================
    
    struct HealthStatus {
        bool healthy;
        bool occtAvailable;
        std::string version;
        size_t shapeCount;
        size_t memoryUsedBytes;
        double cacheHitRate;
    };
    
    HealthStatus healthCheck() const;
    ShapeRegistry::Stats getStats() const;
    
    // Slow operation callback (for UI feedback)
    using SlowOperationCallback = std::function<void(const std::string& operation, double durationMs)>;
    void onSlowOperation(SlowOperationCallback callback, double thresholdMs = 100);
    
private:
    bool initialized_ = false;
    bool occtAvailable_ = false;
    
    // Callbacks
    std::vector<std::pair<SlowOperationCallback, double>> slowOpCallbacks_;
    
    // Helper methods
    void notifySlowOperation(const std::string& op, double durationMs);
    std::string generateOperationKey(const std::string& op, const std::vector<std::string>& ids) const;
    
    template<typename Func>
    auto timedOperation(const std::string& opName, Func&& func) -> decltype(func());
};

/**
 * @brief Global engine instance (for WASM single-instance usage)
 */
Engine& getGlobalEngine();

} // namespace madfam::geom::cad
