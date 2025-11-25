#include "geom-core/cad/Engine.hpp"
#include "geom-core/cad/ShapeRegistry.hpp"

// OCCT headers (conditional compilation for native builds)
#ifdef MADFAM_OCCT_AVAILABLE
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <GC_MakeSegment.hxx>
#include <GC_MakeCircle.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_Circle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <gp_Pnt.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#endif

#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace madfam::geom::cad {

// ===========================================================================
// OCCT Shape Wrapper (when OCCT is available)
// ===========================================================================

#ifdef MADFAM_OCCT_AVAILABLE

class OCCTShape : public InternalShape {
public:
    OCCTShape(const TopoDS_Shape& shape) : shape_(shape) {
        computeBoundingBox();
    }
    
    const TopoDS_Shape& getOCCT() const { return shape_; }
    
    BoundingBox getBoundingBox() const override {
        return bbox_;
    }
    
    std::string computeHash() const override {
        // Simple hash based on bounding box and topology
        std::stringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << bbox_.min.x << bbox_.min.y << bbox_.min.z;
        ss << bbox_.max.x << bbox_.max.y << bbox_.max.z;
        ss << static_cast<int>(shape_.ShapeType());
        
        // Use string hash
        std::hash<std::string> hasher;
        size_t hash = hasher(ss.str());
        
        std::stringstream result;
        result << std::hex << hash;
        return result.str();
    }
    
    size_t getEstimatedMemoryBytes() const override {
        // Rough estimate based on shape complexity
        // A simple primitive is ~1KB, complex shapes can be ~100KB+
        double volume = bbox_.volume();
        size_t baseSize = 1024; // 1KB minimum
        
        // Add size based on volume complexity
        if (volume > 1e6) {
            baseSize += 10240;
        } else if (volume > 1e3) {
            baseSize += 5120;
        }
        
        return baseSize;
    }
    
private:
    TopoDS_Shape shape_;
    BoundingBox bbox_;
    
    void computeBoundingBox() {
        Bnd_Box box;
        BRepBndLib::Add(shape_, box);
        
        if (!box.IsVoid()) {
            double xmin, ymin, zmin, xmax, ymax, zmax;
            box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
            bbox_.min = Vector3(xmin, ymin, zmin);
            bbox_.max = Vector3(xmax, ymax, zmax);
        }
    }
};

// Helper to convert Vector3 to gp_Pnt
inline gp_Pnt toGpPnt(const Vector3& v) {
    return gp_Pnt(v.x, v.y, v.z);
}

// Helper to convert Vector3 to gp_Dir
inline gp_Dir toGpDir(const Vector3& v) {
    return gp_Dir(v.x, v.y, v.z);
}

// Helper to convert Vector3 to gp_Vec
inline gp_Vec toGpVec(const Vector3& v) {
    return gp_Vec(v.x, v.y, v.z);
}

#endif // MADFAM_OCCT_AVAILABLE

// ===========================================================================
// Fallback Shape (when OCCT is not available - simple placeholder)
// ===========================================================================

class PlaceholderShape : public InternalShape {
public:
    PlaceholderShape(ShapeType type, const BoundingBox& bbox)
        : type_(type), bbox_(bbox) {}
    
    BoundingBox getBoundingBox() const override { return bbox_; }
    
    std::string computeHash() const override {
        std::stringstream ss;
        ss << static_cast<int>(type_) << "_";
        ss << bbox_.min.x << bbox_.min.y << bbox_.min.z;
        ss << bbox_.max.x << bbox_.max.y << bbox_.max.z;
        
        std::hash<std::string> hasher;
        std::stringstream result;
        result << std::hex << hasher(ss.str());
        return result.str();
    }
    
    size_t getEstimatedMemoryBytes() const override {
        return 256; // Placeholder is minimal
    }
    
private:
    ShapeType type_;
    BoundingBox bbox_;
};

// ===========================================================================
// Primitive Implementations
// ===========================================================================

Result<ShapeHandle> Engine::makeBox(const BoxParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.width <= 0 || params.height <= 0 || params.depth <= 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Box dimensions must be positive");
    }
    
    std::unique_ptr<InternalShape> shape;
    BoundingBox bbox;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        if (params.center.has_value()) {
            auto c = params.center.value();
            gp_Pnt corner(
                c.x - params.width / 2.0,
                c.y - params.height / 2.0,
                c.z - params.depth / 2.0
            );
            BRepPrimAPI_MakeBox makeBox(corner, params.width, params.height, params.depth);
            makeBox.Build();
            
            if (!makeBox.IsDone()) {
                return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create box");
            }
            
            shape = std::make_unique<OCCTShape>(makeBox.Shape());
        } else {
            BRepPrimAPI_MakeBox makeBox(params.width, params.height, params.depth);
            makeBox.Build();
            
            if (!makeBox.IsDone()) {
                return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create box");
            }
            
            shape = std::make_unique<OCCTShape>(makeBox.Shape());
        }
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    // Fallback: create placeholder
    if (params.center.has_value()) {
        auto c = params.center.value();
        bbox.min = Vector3(c.x - params.width/2, c.y - params.height/2, c.z - params.depth/2);
        bbox.max = Vector3(c.x + params.width/2, c.y + params.height/2, c.z + params.depth/2);
    } else {
        bbox.min = Vector3(0, 0, 0);
        bbox.max = Vector3(params.width, params.height, params.depth);
    }
    shape = std::make_unique<PlaceholderShape>(ShapeType::Solid, bbox);
#endif
    
    // Register shape
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Solid);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    result.wasCached = false;
    
    notifySlowOperation("makeBox", durationMs);
    ShapeRegistry::instance().recordOperation(durationMs);
    
    return result;
}

Result<ShapeHandle> Engine::makeSphere(const SphereParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.radius <= 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Sphere radius must be positive");
    }
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        gp_Pnt center(0, 0, 0);
        if (params.center.has_value()) {
            center = toGpPnt(params.center.value());
        }
        
        BRepPrimAPI_MakeSphere makeSphere(center, params.radius);
        makeSphere.Build();
        
        if (!makeSphere.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create sphere");
        }
        
        shape = std::make_unique<OCCTShape>(makeSphere.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    Vector3 c = params.center.value_or(Vector3(0, 0, 0));
    bbox.min = Vector3(c.x - params.radius, c.y - params.radius, c.z - params.radius);
    bbox.max = Vector3(c.x + params.radius, c.y + params.radius, c.z + params.radius);
    shape = std::make_unique<PlaceholderShape>(ShapeType::Solid, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Solid);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    notifySlowOperation("makeSphere", durationMs);
    ShapeRegistry::instance().recordOperation(durationMs);
    
    return result;
}

Result<ShapeHandle> Engine::makeCylinder(const CylinderParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.radius <= 0 || params.height <= 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Cylinder dimensions must be positive");
    }
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        gp_Ax2 axis;
        if (params.center.has_value()) {
            axis = gp_Ax2(toGpPnt(params.center.value()), toGpDir(params.axis));
        } else {
            axis = gp_Ax2(gp_Pnt(0, 0, 0), toGpDir(params.axis));
        }
        
        BRepPrimAPI_MakeCylinder makeCyl(axis, params.radius, params.height);
        makeCyl.Build();
        
        if (!makeCyl.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create cylinder");
        }
        
        shape = std::make_unique<OCCTShape>(makeCyl.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    Vector3 c = params.center.value_or(Vector3(0, 0, 0));
    bbox.min = Vector3(c.x - params.radius, c.y - params.radius, c.z);
    bbox.max = Vector3(c.x + params.radius, c.y + params.radius, c.z + params.height);
    shape = std::make_unique<PlaceholderShape>(ShapeType::Solid, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Solid);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    notifySlowOperation("makeCylinder", durationMs);
    ShapeRegistry::instance().recordOperation(durationMs);
    
    return result;
}

Result<ShapeHandle> Engine::makeCone(const ConeParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.radius1 < 0 || params.radius2 < 0 || params.height <= 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Cone dimensions must be valid");
    }
    
    if (params.radius1 == 0 && params.radius2 == 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "At least one cone radius must be positive");
    }
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        gp_Ax2 axis;
        if (params.center.has_value()) {
            axis = gp_Ax2(toGpPnt(params.center.value()), toGpDir(params.axis));
        } else {
            axis = gp_Ax2(gp_Pnt(0, 0, 0), toGpDir(params.axis));
        }
        
        BRepPrimAPI_MakeCone makeCone(axis, params.radius1, params.radius2, params.height);
        makeCone.Build();
        
        if (!makeCone.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create cone");
        }
        
        shape = std::make_unique<OCCTShape>(makeCone.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    Vector3 c = params.center.value_or(Vector3(0, 0, 0));
    double maxR = std::max(params.radius1, params.radius2);
    bbox.min = Vector3(c.x - maxR, c.y - maxR, c.z);
    bbox.max = Vector3(c.x + maxR, c.y + maxR, c.z + params.height);
    shape = std::make_unique<PlaceholderShape>(ShapeType::Solid, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Solid);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    notifySlowOperation("makeCone", durationMs);
    ShapeRegistry::instance().recordOperation(durationMs);
    
    return result;
}

Result<ShapeHandle> Engine::makeTorus(const TorusParams& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (params.majorRadius <= 0 || params.minorRadius <= 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Torus radii must be positive");
    }
    
    if (params.minorRadius >= params.majorRadius) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Minor radius must be less than major radius");
    }
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        gp_Ax2 axis;
        if (params.center.has_value()) {
            axis = gp_Ax2(toGpPnt(params.center.value()), toGpDir(params.axis));
        } else {
            axis = gp_Ax2(gp_Pnt(0, 0, 0), toGpDir(params.axis));
        }
        
        BRepPrimAPI_MakeTorus makeTorus(axis, params.majorRadius, params.minorRadius);
        makeTorus.Build();
        
        if (!makeTorus.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create torus");
        }
        
        shape = std::make_unique<OCCTShape>(makeTorus.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    Vector3 c = params.center.value_or(Vector3(0, 0, 0));
    double outerR = params.majorRadius + params.minorRadius;
    bbox.min = Vector3(c.x - outerR, c.y - outerR, c.z - params.minorRadius);
    bbox.max = Vector3(c.x + outerR, c.y + outerR, c.z + params.minorRadius);
    shape = std::make_unique<PlaceholderShape>(ShapeType::Solid, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Solid);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    notifySlowOperation("makeTorus", durationMs);
    ShapeRegistry::instance().recordOperation(durationMs);
    
    return result;
}

// ===========================================================================
// 2D Primitive Implementations
// ===========================================================================

Result<ShapeHandle> Engine::makeLine(const Vector3& start, const Vector3& end) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        GC_MakeSegment makeSegment(toGpPnt(start), toGpPnt(end));
        if (!makeSegment.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create line segment");
        }
        
        BRepBuilderAPI_MakeEdge makeEdge(makeSegment.Value());
        makeEdge.Build();
        
        if (!makeEdge.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create edge from segment");
        }
        
        shape = std::make_unique<OCCTShape>(makeEdge.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    bbox.min = Vector3(
        std::min(start.x, end.x),
        std::min(start.y, end.y),
        std::min(start.z, end.z)
    );
    bbox.max = Vector3(
        std::max(start.x, end.x),
        std::max(start.y, end.y),
        std::max(start.z, end.z)
    );
    shape = std::make_unique<PlaceholderShape>(ShapeType::Edge, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Edge);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    return result;
}

Result<ShapeHandle> Engine::makeCircle(const Vector3& center, double radius, const Vector3& normal) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (radius <= 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Circle radius must be positive");
    }
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        gp_Ax2 axis(toGpPnt(center), toGpDir(normal));
        Handle(Geom_Circle) circle = new Geom_Circle(axis, radius);
        
        BRepBuilderAPI_MakeEdge makeEdge(circle);
        makeEdge.Build();
        
        if (!makeEdge.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create circle edge");
        }
        
        BRepBuilderAPI_MakeWire makeWire(makeEdge.Edge());
        makeWire.Build();
        
        if (!makeWire.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create circle wire");
        }
        
        shape = std::make_unique<OCCTShape>(makeWire.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    bbox.min = Vector3(center.x - radius, center.y - radius, center.z);
    bbox.max = Vector3(center.x + radius, center.y + radius, center.z);
    shape = std::make_unique<PlaceholderShape>(ShapeType::Wire, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Wire);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    return result;
}

Result<ShapeHandle> Engine::makeRectangle(const Vector3& center, double width, double height) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (width <= 0 || height <= 0) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Rectangle dimensions must be positive");
    }
    
    // Create rectangle as a polygon
    std::vector<Vector3> points = {
        Vector3(center.x - width/2, center.y - height/2, center.z),
        Vector3(center.x + width/2, center.y - height/2, center.z),
        Vector3(center.x + width/2, center.y + height/2, center.z),
        Vector3(center.x - width/2, center.y + height/2, center.z)
    };
    
    return makePolygon(points, true);
}

Result<ShapeHandle> Engine::makePolygon(const std::vector<Vector3>& points, bool closed) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (points.size() < 2) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Polygon requires at least 2 points");
    }
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        BRepBuilderAPI_MakeWire makeWire;
        
        for (size_t i = 0; i < points.size() - 1; ++i) {
            GC_MakeSegment seg(toGpPnt(points[i]), toGpPnt(points[i + 1]));
            if (seg.IsDone()) {
                BRepBuilderAPI_MakeEdge edge(seg.Value());
                if (edge.IsDone()) {
                    makeWire.Add(edge.Edge());
                }
            }
        }
        
        // Close the polygon if requested
        if (closed && points.size() > 2) {
            GC_MakeSegment seg(toGpPnt(points.back()), toGpPnt(points.front()));
            if (seg.IsDone()) {
                BRepBuilderAPI_MakeEdge edge(seg.Value());
                if (edge.IsDone()) {
                    makeWire.Add(edge.Edge());
                }
            }
        }
        
        makeWire.Build();
        if (!makeWire.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create polygon wire");
        }
        
        shape = std::make_unique<OCCTShape>(makeWire.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    bbox.min = points[0];
    bbox.max = points[0];
    for (const auto& p : points) {
        bbox.min.x = std::min(bbox.min.x, p.x);
        bbox.min.y = std::min(bbox.min.y, p.y);
        bbox.min.z = std::min(bbox.min.z, p.z);
        bbox.max.x = std::max(bbox.max.x, p.x);
        bbox.max.y = std::max(bbox.max.y, p.y);
        bbox.max.z = std::max(bbox.max.z, p.z);
    }
    shape = std::make_unique<PlaceholderShape>(ShapeType::Wire, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Wire);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    return result;
}

Result<ShapeHandle> Engine::makeArc(const Vector3& start, const Vector3& middle, const Vector3& end) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        GC_MakeArcOfCircle makeArc(toGpPnt(start), toGpPnt(middle), toGpPnt(end));
        if (!makeArc.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create arc (points may be collinear)");
        }
        
        BRepBuilderAPI_MakeEdge makeEdge(makeArc.Value());
        makeEdge.Build();
        
        if (!makeEdge.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create edge from arc");
        }
        
        shape = std::make_unique<OCCTShape>(makeEdge.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    BoundingBox bbox;
    bbox.min = Vector3(
        std::min({start.x, middle.x, end.x}),
        std::min({start.y, middle.y, end.y}),
        std::min({start.z, middle.z, end.z})
    );
    bbox.max = Vector3(
        std::max({start.x, middle.x, end.x}),
        std::max({start.y, middle.y, end.y}),
        std::max({start.z, middle.z, end.z})
    );
    shape = std::make_unique<PlaceholderShape>(ShapeType::Edge, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Edge);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    return result;
}

Result<ShapeHandle> Engine::makeWire(const std::vector<std::string>& edgeIds) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (edgeIds.empty()) {
        return Result<ShapeHandle>::error("INVALID_PARAMS", "Wire requires at least one edge");
    }
    
    std::unique_ptr<InternalShape> shape;
    
#ifdef MADFAM_OCCT_AVAILABLE
    try {
        BRepBuilderAPI_MakeWire makeWire;
        
        for (const auto& id : edgeIds) {
            auto* internalShape = ShapeRegistry::instance().getShape(id);
            if (!internalShape) {
                return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", "Edge not found: " + id);
            }
            
            auto* occtShape = dynamic_cast<OCCTShape*>(internalShape);
            if (!occtShape) {
                return Result<ShapeHandle>::error("INVALID_SHAPE", "Shape is not an OCCT shape: " + id);
            }
            
            const TopoDS_Shape& topoShape = occtShape->getOCCT();
            if (topoShape.ShapeType() == TopAbs_EDGE) {
                makeWire.Add(TopoDS::Edge(topoShape));
            } else if (topoShape.ShapeType() == TopAbs_WIRE) {
                makeWire.Add(TopoDS::Wire(topoShape));
            } else {
                return Result<ShapeHandle>::error("INVALID_SHAPE", "Shape is not an edge or wire: " + id);
            }
        }
        
        makeWire.Build();
        if (!makeWire.IsDone()) {
            return Result<ShapeHandle>::error("OCCT_ERROR", "Failed to create wire from edges");
        }
        
        shape = std::make_unique<OCCTShape>(makeWire.Shape());
    } catch (const std::exception& e) {
        return Result<ShapeHandle>::error("OCCT_EXCEPTION", e.what());
    }
#else
    // Fallback: compute combined bounding box
    BoundingBox bbox;
    bool first = true;
    
    for (const auto& id : edgeIds) {
        auto handle = ShapeRegistry::instance().getHandle(id);
        if (!handle.isValid()) {
            return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", "Edge not found: " + id);
        }
        
        if (first) {
            bbox = handle.bbox;
            first = false;
        } else {
            bbox.min.x = std::min(bbox.min.x, handle.bbox.min.x);
            bbox.min.y = std::min(bbox.min.y, handle.bbox.min.y);
            bbox.min.z = std::min(bbox.min.z, handle.bbox.min.z);
            bbox.max.x = std::max(bbox.max.x, handle.bbox.max.x);
            bbox.max.y = std::max(bbox.max.y, handle.bbox.max.y);
            bbox.max.z = std::max(bbox.max.z, handle.bbox.max.z);
        }
    }
    
    shape = std::make_unique<PlaceholderShape>(ShapeType::Wire, bbox);
#endif
    
    std::string id = ShapeRegistry::instance().registerShape(std::move(shape), ShapeType::Wire);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(id);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    
    return result;
}

} // namespace madfam::geom::cad
