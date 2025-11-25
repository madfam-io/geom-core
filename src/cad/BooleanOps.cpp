/**
 * BooleanOps.cpp - Optimized Boolean Operations for WASM
 * 
 * These are the "hot path" boolean operations that benefit from C++ performance.
 * Designed for zero-lag execution with complexity estimation and caching.
 */

#include "geom-core/cad/Engine.hpp"
#include "geom-core/cad/ShapeRegistry.hpp"

#ifdef GC_USE_OCCT
#include "OCCTShape.hpp"
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <TopTools_ListOfShape.hxx>
#include <Message_ProgressIndicator.hxx>
#endif

#include <chrono>
#include <sstream>

namespace madfam::geom::cad {

namespace {

// Generate cache key for boolean operation
std::string makeBooleanCacheKey(
    const std::string& opName,
    const std::vector<std::string>& shapeIds
) {
    std::stringstream ss;
    ss << opName;
    for (const auto& id : shapeIds) {
        ss << ":" << id;
    }
    return ss.str();
}

} // anonymous namespace

// =============================================================================
// Boolean Union
// =============================================================================

Result<ShapeHandle> Engine::booleanUnion(const BooleanUnionParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.shapeIds.size() < 2) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Union requires at least 2 shapes");
    }
    
    // Check cache first
    std::string cacheKey = makeBooleanCacheKey("union", params.shapeIds);
    auto cached = ShapeRegistry::instance().getCachedResult(cacheKey);
    if (cached.has_value()) {
        auto handle = ShapeRegistry::instance().getHandle(cached.value());
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.wasCached = true;
        result.durationMs = 0;
        return result;
    }
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        // Get first shape
        auto* firstShape = registry.getShape(params.shapeIds[0]);
        if (!firstShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeIds[0]);
        }
        
        TopoDS_Shape result = getOCCT(firstShape);
        
        // Fuse remaining shapes
        for (size_t i = 1; i < params.shapeIds.size(); ++i) {
            auto* toolShape = registry.getShape(params.shapeIds[i]);
            if (!toolShape) {
                return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                    "Shape not found: " + params.shapeIds[i]);
            }
            
            BRepAlgoAPI_Fuse fuse(result, getOCCT(toolShape));
            fuse.SetRunParallel(true);  // Enable parallel processing
            fuse.Build();
            
            if (!fuse.IsDone()) {
                return Result<ShapeHandle>::error("BOOLEAN_FAILED", 
                    "Union operation failed at shape " + std::to_string(i));
            }
            
            result = fuse.Shape();
        }
        
        // Register result
        auto shape = std::make_unique<OCCTShape>(std::move(result), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(shape), ShapeType::Solid);
        
        // Cache the result
        registry.cacheResult(cacheKey, id);
        
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto res = Result<ShapeHandle>::ok(std::move(handle));
        res.durationMs = durationMs;
        res.wasCached = false;
        
        notifySlowOperation("booleanUnion", durationMs);
        registry.recordOperation(durationMs);
        
        return res;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("EXCEPTION", e.what());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", 
        "Boolean operations require OCCT support");
#endif
}

// Convenience overload
Result<ShapeHandle> Engine::booleanUnion(const std::string& id1, const std::string& id2) {
    BooleanUnionParams params;
    params.shapeIds = {id1, id2};
    return booleanUnion(params);
}

// =============================================================================
// Boolean Subtract
// =============================================================================

Result<ShapeHandle> Engine::booleanSubtract(const BooleanSubtractParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.baseId.empty()) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Base shape ID required");
    }
    if (params.toolIds.empty()) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "At least one tool shape required");
    }
    
    // Check cache
    std::vector<std::string> allIds = {params.baseId};
    allIds.insert(allIds.end(), params.toolIds.begin(), params.toolIds.end());
    std::string cacheKey = makeBooleanCacheKey("subtract", allIds);
    
    auto cached = ShapeRegistry::instance().getCachedResult(cacheKey);
    if (cached.has_value()) {
        auto handle = ShapeRegistry::instance().getHandle(cached.value());
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.wasCached = true;
        result.durationMs = 0;
        return result;
    }
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        // Get base shape
        auto* baseShape = registry.getShape(params.baseId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Base shape not found: " + params.baseId);
        }
        
        TopoDS_Shape result = getOCCT(baseShape);
        
        // Cut with each tool
        for (const auto& toolId : params.toolIds) {
            auto* toolShape = registry.getShape(toolId);
            if (!toolShape) {
                return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                    "Tool shape not found: " + toolId);
            }
            
            BRepAlgoAPI_Cut cut(result, getOCCT(toolShape));
            cut.SetRunParallel(true);
            cut.Build();
            
            if (!cut.IsDone()) {
                return Result<ShapeHandle>::error("BOOLEAN_FAILED", 
                    "Subtract operation failed with tool: " + toolId);
            }
            
            result = cut.Shape();
        }
        
        // Register result
        auto shape = std::make_unique<OCCTShape>(std::move(result), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(shape), ShapeType::Solid);
        
        // Cache result
        registry.cacheResult(cacheKey, id);
        
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto res = Result<ShapeHandle>::ok(std::move(handle));
        res.durationMs = durationMs;
        res.wasCached = false;
        
        notifySlowOperation("booleanSubtract", durationMs);
        registry.recordOperation(durationMs);
        
        return res;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("EXCEPTION", e.what());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", 
        "Boolean operations require OCCT support");
#endif
}

// Convenience overload
Result<ShapeHandle> Engine::booleanSubtract(const std::string& baseId, const std::string& toolId) {
    BooleanSubtractParams params;
    params.baseId = baseId;
    params.toolIds = {toolId};
    return booleanSubtract(params);
}

// =============================================================================
// Boolean Intersect
// =============================================================================

Result<ShapeHandle> Engine::booleanIntersect(const BooleanIntersectParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.shapeIds.size() < 2) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Intersect requires at least 2 shapes");
    }
    
    // Check cache
    std::string cacheKey = makeBooleanCacheKey("intersect", params.shapeIds);
    auto cached = ShapeRegistry::instance().getCachedResult(cacheKey);
    if (cached.has_value()) {
        auto handle = ShapeRegistry::instance().getHandle(cached.value());
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.wasCached = true;
        result.durationMs = 0;
        return result;
    }
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        // Get first shape
        auto* firstShape = registry.getShape(params.shapeIds[0]);
        if (!firstShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeIds[0]);
        }
        
        TopoDS_Shape result = getOCCT(firstShape);
        
        // Intersect with remaining shapes
        for (size_t i = 1; i < params.shapeIds.size(); ++i) {
            auto* toolShape = registry.getShape(params.shapeIds[i]);
            if (!toolShape) {
                return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                    "Shape not found: " + params.shapeIds[i]);
            }
            
            BRepAlgoAPI_Common common(result, getOCCT(toolShape));
            common.SetRunParallel(true);
            common.Build();
            
            if (!common.IsDone()) {
                return Result<ShapeHandle>::error("BOOLEAN_FAILED", 
                    "Intersect operation failed at shape " + std::to_string(i));
            }
            
            result = common.Shape();
        }
        
        // Register result
        auto shape = std::make_unique<OCCTShape>(std::move(result), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(shape), ShapeType::Solid);
        
        // Cache result
        registry.cacheResult(cacheKey, id);
        
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto res = Result<ShapeHandle>::ok(std::move(handle));
        res.durationMs = durationMs;
        res.wasCached = false;
        
        notifySlowOperation("booleanIntersect", durationMs);
        registry.recordOperation(durationMs);
        
        return res;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("EXCEPTION", e.what());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", 
        "Boolean operations require OCCT support");
#endif
}

// Convenience overload
Result<ShapeHandle> Engine::booleanIntersect(const std::string& id1, const std::string& id2) {
    BooleanIntersectParams params;
    params.shapeIds = {id1, id2};
    return booleanIntersect(params);
}

} // namespace madfam::geom::cad
