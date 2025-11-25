/**
 * Transforms.cpp - Geometric Transformations
 * 
 * Implements translate, rotate, scale, mirror, and matrix transforms.
 * These are fast operations (<2ms) that always run locally.
 */

#include "geom-core/cad/Engine.hpp"
#include "geom-core/cad/ShapeRegistry.hpp"

#ifdef GC_USE_OCCT
#include "OCCTShape.hpp"
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#endif

#include <chrono>
#include <cmath>

namespace madfam::geom::cad {

// =============================================================================
// Translate
// =============================================================================

Result<ShapeHandle> Engine::translate(const TranslateParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        gp_Vec vec(params.offset.x, params.offset.y, params.offset.z);
        gp_Trsf trsf;
        trsf.SetTranslation(vec);
        
        BRepBuilderAPI_Transform transform(getOCCT(baseShape), trsf, true);
        
        if (!transform.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Translate operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(transform.Shape(), baseShape->getType());
        std::string id = registry.registerShape(std::move(resultShape), baseShape->getType());
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Translate requires OCCT support");
#endif
}

// =============================================================================
// Rotate
// =============================================================================

Result<ShapeHandle> Engine::rotate(const RotateParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        gp_Pnt origin(params.axisOrigin.x, params.axisOrigin.y, params.axisOrigin.z);
        gp_Dir dir(params.axisDirection.x, params.axisDirection.y, params.axisDirection.z);
        gp_Ax1 axis(origin, dir);
        
        gp_Trsf trsf;
        trsf.SetRotation(axis, params.angle);
        
        BRepBuilderAPI_Transform transform(getOCCT(baseShape), trsf, true);
        
        if (!transform.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Rotate operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(transform.Shape(), baseShape->getType());
        std::string id = registry.registerShape(std::move(resultShape), baseShape->getType());
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Rotate requires OCCT support");
#endif
}

// =============================================================================
// Scale
// =============================================================================

Result<ShapeHandle> Engine::scale(const ScaleParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        gp_Pnt center(params.center.x, params.center.y, params.center.z);
        
        gp_Trsf trsf;
        trsf.SetScale(center, params.factor);
        
        BRepBuilderAPI_Transform transform(getOCCT(baseShape), trsf, true);
        
        if (!transform.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Scale operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(transform.Shape(), baseShape->getType());
        std::string id = registry.registerShape(std::move(resultShape), baseShape->getType());
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Scale requires OCCT support");
#endif
}

// =============================================================================
// Mirror
// =============================================================================

Result<ShapeHandle> Engine::mirror(const MirrorParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        gp_Pnt point(params.planePoint.x, params.planePoint.y, params.planePoint.z);
        gp_Dir normal(params.planeNormal.x, params.planeNormal.y, params.planeNormal.z);
        gp_Ax2 plane(point, normal);
        
        gp_Trsf trsf;
        trsf.SetMirror(plane);
        
        BRepBuilderAPI_Transform transform(getOCCT(baseShape), trsf, true);
        
        if (!transform.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Mirror operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(transform.Shape(), baseShape->getType());
        std::string id = registry.registerShape(std::move(resultShape), baseShape->getType());
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Mirror requires OCCT support");
#endif
}

// =============================================================================
// Matrix Transform
// =============================================================================

Result<ShapeHandle> Engine::transform(const MatrixTransformParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        const double* m = params.matrix.m;
        
        gp_Trsf trsf;
        // OCCT uses 3x4 transformation matrix (rotation + translation)
        // Row 1: m[0], m[1], m[2], m[3]
        // Row 2: m[4], m[5], m[6], m[7]
        // Row 3: m[8], m[9], m[10], m[11]
        trsf.SetValues(
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11]
        );
        
        BRepBuilderAPI_Transform transform(getOCCT(baseShape), trsf, true);
        
        if (!transform.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Transform operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(transform.Shape(), baseShape->getType());
        std::string id = registry.registerShape(std::move(resultShape), baseShape->getType());
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Transform requires OCCT support");
#endif
}

} // namespace madfam::geom::cad
