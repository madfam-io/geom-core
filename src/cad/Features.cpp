/**
 * Features.cpp - Advanced CAD Modeling Features
 * 
 * Implements extrude, revolve, sweep, loft, fillet, chamfer, shell, offset.
 * Ported from sim4d/engine-occt for unified geom-core module.
 */

#include "geom-core/cad/Engine.hpp"
#include "geom-core/cad/ShapeRegistry.hpp"

#ifdef GC_USE_OCCT
#include "OCCTShape.hpp"
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax1.hxx>
#endif

#include <chrono>

namespace madfam::geom::cad {

// =============================================================================
// Extrude
// =============================================================================

Result<ShapeHandle> Engine::extrude(const ExtrudeParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.profileId.empty()) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Profile shape ID required");
    }
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* profile = registry.getShape(params.profileId);
        if (!profile) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Profile not found: " + params.profileId);
        }
        
        // Create extrusion vector
        gp_Vec vec(
            params.direction.x * params.distance,
            params.direction.y * params.distance,
            params.direction.z * params.distance
        );
        
        BRepPrimAPI_MakePrism prism(getOCCT(profile), vec);
        prism.Build();
        
        if (!prism.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Extrude operation failed");
        }
        
        auto shape = std::make_unique<OCCTShape>(prism.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(shape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("extrude", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Extrude requires OCCT support");
#endif
}

// =============================================================================
// Revolve
// =============================================================================

Result<ShapeHandle> Engine::revolve(const RevolveParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.profileId.empty()) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Profile shape ID required");
    }
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* profile = registry.getShape(params.profileId);
        if (!profile) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Profile not found: " + params.profileId);
        }
        
        // Create axis
        gp_Pnt origin(params.axisOrigin.x, params.axisOrigin.y, params.axisOrigin.z);
        gp_Dir dir(params.axisDirection.x, params.axisDirection.y, params.axisDirection.z);
        gp_Ax1 axis(origin, dir);
        
        BRepPrimAPI_MakeRevol revol(getOCCT(profile), axis, params.angle);
        revol.Build();
        
        if (!revol.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Revolve operation failed");
        }
        
        auto shape = std::make_unique<OCCTShape>(revol.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(shape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("revolve", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Revolve requires OCCT support");
#endif
}

// =============================================================================
// Sweep
// =============================================================================

Result<ShapeHandle> Engine::sweep(const SweepParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* profile = registry.getShape(params.profileId);
        auto* path = registry.getShape(params.pathId);
        
        if (!profile) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Profile not found: " + params.profileId);
        }
        if (!path) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Path not found: " + params.pathId);
        }
        
        // Path must be a wire
        TopoDS_Wire pathWire;
        const TopoDS_Shape& pathShape = getOCCT(path);
        if (pathShape.ShapeType() == TopAbs_WIRE) {
            pathWire = TopoDS::Wire(pathShape);
        } else if (pathShape.ShapeType() == TopAbs_EDGE) {
            BRepBuilderAPI_MakeWire wireBuilder(TopoDS::Edge(pathShape));
            pathWire = wireBuilder.Wire();
        } else {
            return Result<ShapeHandle>::error("INVALID_SHAPE", "Path must be a wire or edge");
        }
        
        BRepOffsetAPI_MakePipe pipe(pathWire, getOCCT(profile));
        pipe.Build();
        
        if (!pipe.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Sweep operation failed");
        }
        
        auto shape = std::make_unique<OCCTShape>(pipe.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(shape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("sweep", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Sweep requires OCCT support");
#endif
}

// =============================================================================
// Loft
// =============================================================================

Result<ShapeHandle> Engine::loft(const LoftParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.profileIds.size() < 2) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Loft requires at least 2 profiles");
    }
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        BRepOffsetAPI_ThruSections loft(!params.ruled, params.closed);
        
        for (const auto& profileId : params.profileIds) {
            auto* profile = registry.getShape(profileId);
            if (!profile) {
                return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                    "Profile not found: " + profileId);
            }
            
            const TopoDS_Shape& profileShape = getOCCT(profile);
            if (profileShape.ShapeType() == TopAbs_WIRE) {
                loft.AddWire(TopoDS::Wire(profileShape));
            } else {
                return Result<ShapeHandle>::error("INVALID_SHAPE", 
                    "Profile must be a wire: " + profileId);
            }
        }
        
        loft.Build();
        
        if (!loft.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Loft operation failed");
        }
        
        auto shape = std::make_unique<OCCTShape>(loft.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(shape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("loft", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Loft requires OCCT support");
#endif
}

// =============================================================================
// Fillet
// =============================================================================

Result<ShapeHandle> Engine::fillet(const FilletParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        const TopoDS_Shape& shape = getOCCT(baseShape);
        BRepFilletAPI_MakeFillet fillet(shape);
        
        // Add edges
        if (params.edgeIds.empty()) {
            // All edges
            for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) {
                fillet.Add(params.radius, TopoDS::Edge(exp.Current()));
            }
        } else {
            // Specific edges (would need edge ID mapping)
            // For now, apply to all edges
            for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) {
                fillet.Add(params.radius, TopoDS::Edge(exp.Current()));
            }
        }
        
        fillet.Build();
        
        if (!fillet.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Fillet operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(fillet.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(resultShape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("fillet", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Fillet requires OCCT support");
#endif
}

// =============================================================================
// Chamfer
// =============================================================================

Result<ShapeHandle> Engine::chamfer(const ChamferParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        const TopoDS_Shape& shape = getOCCT(baseShape);
        BRepFilletAPI_MakeChamfer chamfer(shape);
        
        // Add edges
        for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) {
            chamfer.Add(params.distance, TopoDS::Edge(exp.Current()));
        }
        
        chamfer.Build();
        
        if (!chamfer.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Chamfer operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(chamfer.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(resultShape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("chamfer", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Chamfer requires OCCT support");
#endif
}

// =============================================================================
// Shell
// =============================================================================

Result<ShapeHandle> Engine::shell(const ShellParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        const TopoDS_Shape& shape = getOCCT(baseShape);
        
        // Collect faces to remove
        TopTools_ListOfShape facesToRemove;
        
        if (params.faceIdsToRemove.empty()) {
            // Remove first face by default
            TopExp_Explorer exp(shape, TopAbs_FACE);
            if (exp.More()) {
                facesToRemove.Append(exp.Current());
            }
        }
        
        BRepOffsetAPI_MakeThickSolid shell;
        shell.MakeThickSolidByJoin(shape, facesToRemove, params.thickness, 1.0e-3);
        
        if (!shell.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Shell operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(shell.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(resultShape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("shell", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Shell requires OCCT support");
#endif
}

// =============================================================================
// Offset
// =============================================================================

Result<ShapeHandle> Engine::offset(const OffsetParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef GC_USE_OCCT
    try {
        auto& registry = ShapeRegistry::instance();
        
        auto* baseShape = registry.getShape(params.shapeId);
        if (!baseShape) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", 
                "Shape not found: " + params.shapeId);
        }
        
        const TopoDS_Shape& shape = getOCCT(baseShape);
        
        BRepOffsetAPI_MakeOffsetShape offset;
        offset.PerformByJoin(
            shape, 
            params.distance, 
            1.0e-7,
            BRepOffset_Skin,
            Standard_False,
            Standard_False,
            params.joinArcs ? GeomAbs_Arc : GeomAbs_Intersection
        );
        
        if (!offset.IsDone()) {
            return Result<ShapeHandle>::error("OPERATION_FAILED", "Offset operation failed");
        }
        
        auto resultShape = std::make_unique<OCCTShape>(offset.Shape(), ShapeType::Solid);
        std::string id = registry.registerShape(std::move(resultShape), ShapeType::Solid);
        ShapeHandle handle = registry.getHandle(id);
        
        auto end = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        auto result = Result<ShapeHandle>::ok(std::move(handle));
        result.durationMs = durationMs;
        
        notifySlowOperation("offset", durationMs);
        return result;
        
    } catch (const Standard_Failure& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.GetMessageString());
    }
#else
    return Result<ShapeHandle>::error("NOT_IMPLEMENTED", "Offset requires OCCT support");
#endif
}

} // namespace madfam::geom::cad
