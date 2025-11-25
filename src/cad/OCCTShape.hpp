#pragma once

/**
 * OCCTShape - Internal shape wrapper for OpenCASCADE TopoDS_Shape
 * 
 * This provides the bridge between our generic InternalShape interface
 * and the OCCT-specific implementation.
 */

#include "geom-core/cad/ShapeRegistry.hpp"

#ifdef GC_USE_OCCT

#include <TopoDS_Shape.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <BRepTools.hxx>

#include <sstream>
#include <iomanip>
#include <functional>

namespace madfam::geom::cad {

/**
 * @brief OCCT-backed implementation of InternalShape
 */
class OCCTShape : public InternalShape {
public:
    explicit OCCTShape(const TopoDS_Shape& shape, ShapeType type = ShapeType::Solid)
        : shape_(shape), type_(type) {
        computeCachedProperties();
    }
    
    explicit OCCTShape(TopoDS_Shape&& shape, ShapeType type = ShapeType::Solid)
        : shape_(std::move(shape)), type_(type) {
        computeCachedProperties();
    }
    
    ~OCCTShape() override = default;
    
    // InternalShape interface
    ShapeType getType() const override { return type_; }
    
    BoundingBox getBoundingBox() const override {
        return cachedBBox_;
    }
    
    double getVolume() const override {
        if (!cachedVolume_.has_value()) {
            GProp_GProps props;
            BRepGProp::VolumeProperties(shape_, props);
            cachedVolume_ = props.Mass();
        }
        return cachedVolume_.value();
    }
    
    double getSurfaceArea() const override {
        if (!cachedSurfaceArea_.has_value()) {
            GProp_GProps props;
            BRepGProp::SurfaceProperties(shape_, props);
            cachedSurfaceArea_ = props.Mass();
        }
        return cachedSurfaceArea_.value();
    }
    
    Vector3 getCenterOfMass() const override {
        if (!cachedCenterOfMass_.has_value()) {
            GProp_GProps props;
            BRepGProp::VolumeProperties(shape_, props);
            gp_Pnt center = props.CentreOfMass();
            cachedCenterOfMass_ = Vector3(center.X(), center.Y(), center.Z());
        }
        return cachedCenterOfMass_.value();
    }
    
    size_t getEstimatedMemoryBytes() const override {
        // Rough estimate based on shape complexity
        // Each face ~1KB, each edge ~100B, each vertex ~50B
        size_t bytes = 1024; // Base overhead
        
        // Count topology
        for (TopExp_Explorer exp(shape_, TopAbs_FACE); exp.More(); exp.Next()) {
            bytes += 1024;
        }
        for (TopExp_Explorer exp(shape_, TopAbs_EDGE); exp.More(); exp.Next()) {
            bytes += 100;
        }
        for (TopExp_Explorer exp(shape_, TopAbs_VERTEX); exp.More(); exp.Next()) {
            bytes += 50;
        }
        
        return bytes;
    }
    
    std::string computeHash() const override {
        // Hash based on bounding box and topology count
        std::stringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << cachedBBox_.min.x << cachedBBox_.min.y << cachedBBox_.min.z;
        ss << cachedBBox_.max.x << cachedBBox_.max.y << cachedBBox_.max.z;
        ss << static_cast<int>(shape_.ShapeType());
        
        std::hash<std::string> hasher;
        std::stringstream result;
        result << std::hex << hasher(ss.str());
        return result.str();
    }
    
    /**
     * @brief Tessellate shape to triangle mesh for visualization
     * 
     * Optimized for WASM performance with SIMD-friendly data layout.
     */
    MeshData tessellate(const TessellateOptions& options) const override {
        MeshData mesh;
        
        // Perform incremental meshing
        BRepMesh_IncrementalMesh mesher(
            shape_,
            options.linearDeflection,
            options.relative,
            options.angularDeflection
        );
        mesher.Perform();
        
        // Pre-count vertices and triangles for allocation
        size_t totalVertices = 0;
        size_t totalTriangles = 0;
        
        for (TopExp_Explorer exp(shape_, TopAbs_FACE); exp.More(); exp.Next()) {
            TopoDS_Face face = TopoDS::Face(exp.Current());
            TopLoc_Location loc;
            Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
            
            if (!tri.IsNull()) {
                totalVertices += tri->NbNodes();
                totalTriangles += tri->NbTriangles();
            }
        }
        
        // Pre-allocate for performance
        mesh.positions.reserve(totalVertices * 3);
        mesh.normals.reserve(totalVertices * 3);
        mesh.indices.reserve(totalTriangles * 3);
        if (options.computeUVs) {
            mesh.uvs.reserve(totalVertices * 2);
        }
        
        // Extract mesh data
        uint32_t indexOffset = 0;
        
        for (TopExp_Explorer exp(shape_, TopAbs_FACE); exp.More(); exp.Next()) {
            TopoDS_Face face = TopoDS::Face(exp.Current());
            TopLoc_Location loc;
            Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
            
            if (tri.IsNull()) continue;
            
            const gp_Trsf& transform = loc.Transformation();
            const bool reversed = (face.Orientation() == TopAbs_REVERSED);
            
            // Get nodes
            const TColgp_Array1OfPnt& nodes = tri->Nodes();
            
            // Add vertices
            for (int i = nodes.Lower(); i <= nodes.Upper(); ++i) {
                gp_Pnt p = nodes(i).Transformed(transform);
                mesh.positions.push_back(static_cast<float>(p.X()));
                mesh.positions.push_back(static_cast<float>(p.Y()));
                mesh.positions.push_back(static_cast<float>(p.Z()));
            }
            
            // Compute and add normals
            if (options.computeNormals && tri->HasNormals()) {
                const TShort_Array1OfShortReal& norms = tri->Normals();
                for (int i = 0; i < tri->NbNodes(); ++i) {
                    int idx = i * 3;
                    gp_Dir n(norms(idx + 1), norms(idx + 2), norms(idx + 3));
                    n.Transform(transform);
                    if (reversed) n.Reverse();
                    
                    mesh.normals.push_back(static_cast<float>(n.X()));
                    mesh.normals.push_back(static_cast<float>(n.Y()));
                    mesh.normals.push_back(static_cast<float>(n.Z()));
                }
            } else if (options.computeNormals) {
                // Fallback: use face normal for all vertices
                BRepGProp_Face faceProps(face);
                gp_Pnt pnt;
                gp_Vec normal;
                double u = 0, v = 0;
                faceProps.Normal(u, v, pnt, normal);
                
                if (reversed) normal.Reverse();
                
                for (int i = 0; i < tri->NbNodes(); ++i) {
                    mesh.normals.push_back(static_cast<float>(normal.X()));
                    mesh.normals.push_back(static_cast<float>(normal.Y()));
                    mesh.normals.push_back(static_cast<float>(normal.Z()));
                }
            }
            
            // Add UVs if requested
            if (options.computeUVs && tri->HasUVNodes()) {
                const TColgp_Array1OfPnt2d& uvNodes = tri->UVNodes();
                for (int i = uvNodes.Lower(); i <= uvNodes.Upper(); ++i) {
                    mesh.uvs.push_back(static_cast<float>(uvNodes(i).X()));
                    mesh.uvs.push_back(static_cast<float>(uvNodes(i).Y()));
                }
            }
            
            // Add triangles
            const Poly_Array1OfTriangle& triangles = tri->Triangles();
            for (int i = triangles.Lower(); i <= triangles.Upper(); ++i) {
                int n1, n2, n3;
                triangles(i).Get(n1, n2, n3);
                
                // Convert from 1-based to 0-based indexing
                n1 -= nodes.Lower();
                n2 -= nodes.Lower();
                n3 -= nodes.Lower();
                
                if (reversed) {
                    mesh.indices.push_back(indexOffset + n1);
                    mesh.indices.push_back(indexOffset + n3);
                    mesh.indices.push_back(indexOffset + n2);
                } else {
                    mesh.indices.push_back(indexOffset + n1);
                    mesh.indices.push_back(indexOffset + n2);
                    mesh.indices.push_back(indexOffset + n3);
                }
            }
            
            indexOffset += tri->NbNodes();
        }
        
        return mesh;
    }
    
    std::unique_ptr<InternalShape> clone() const override {
        // BRepBuilderAPI_Copy for deep copy
        BRepBuilderAPI_Copy copier(shape_);
        return std::make_unique<OCCTShape>(copier.Shape(), type_);
    }
    
    // OCCT-specific access
    void* getOCCTShape() override {
        return &shape_;
    }
    
    const void* getOCCTShape() const override {
        return &shape_;
    }
    
    const TopoDS_Shape& shape() const { return shape_; }
    TopoDS_Shape& shape() { return shape_; }
    
private:
    void computeCachedProperties() {
        // Compute bounding box (always needed)
        Bnd_Box box;
        BRepBndLib::Add(shape_, box);
        
        if (!box.IsVoid()) {
            double xmin, ymin, zmin, xmax, ymax, zmax;
            box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
            cachedBBox_.min = Vector3(xmin, ymin, zmin);
            cachedBBox_.max = Vector3(xmax, ymax, zmax);
        }
    }
    
    TopoDS_Shape shape_;
    ShapeType type_;
    
    // Cached properties (computed lazily)
    BoundingBox cachedBBox_;
    mutable std::optional<double> cachedVolume_;
    mutable std::optional<double> cachedSurfaceArea_;
    mutable std::optional<Vector3> cachedCenterOfMass_;
};

// Helper to get OCCT shape from internal shape
inline const TopoDS_Shape& getOCCT(const InternalShape* shape) {
    return *static_cast<const TopoDS_Shape*>(shape->getOCCTShape());
}

inline TopoDS_Shape& getOCCT(InternalShape* shape) {
    return *static_cast<TopoDS_Shape*>(shape->getOCCTShape());
}

} // namespace madfam::geom::cad

#endif // GC_USE_OCCT
