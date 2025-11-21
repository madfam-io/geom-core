#pragma once
#include "Vector3.hpp"
#include "Mesh.hpp"
#include <vector>
#include <limits>
#include <memory>

namespace madfam::geom {

/**
 * @brief Ray for spatial queries
 */
struct Ray {
    Vector3 origin;
    Vector3 direction; // Should be normalized

    Ray() = default;
    Ray(const Vector3& o, const Vector3& d) : origin(o), direction(d) {}

    Vector3 at(double t) const {
        return origin + direction * t;
    }
};

/**
 * @brief Axis-Aligned Bounding Box
 */
struct AABB {
    Vector3 min;
    Vector3 max;

    AABB() : min(Vector3(std::numeric_limits<double>::max(),
                        std::numeric_limits<double>::max(),
                        std::numeric_limits<double>::max())),
             max(Vector3(std::numeric_limits<double>::lowest(),
                        std::numeric_limits<double>::lowest(),
                        std::numeric_limits<double>::lowest())) {}

    AABB(const Vector3& minPt, const Vector3& maxPt) : min(minPt), max(maxPt) {}

    /**
     * @brief Expand AABB to include a point
     */
    void expand(const Vector3& point) {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    /**
     * @brief Expand AABB to include another AABB
     */
    void expand(const AABB& other) {
        expand(other.min);
        expand(other.max);
    }

    /**
     * @brief Get center of AABB
     */
    Vector3 center() const {
        return Vector3((min.x + max.x) * 0.5,
                      (min.y + max.y) * 0.5,
                      (min.z + max.z) * 0.5);
    }

    /**
     * @brief Get surface area (for SAH)
     */
    double surfaceArea() const {
        double dx = max.x - min.x;
        double dy = max.y - min.y;
        double dz = max.z - min.z;
        return 2.0 * (dx * dy + dy * dz + dz * dx);
    }

    /**
     * @brief Test ray-box intersection (slab method)
     */
    bool intersect(const Ray& ray, double& tMin, double& tMax) const;
};

/**
 * @brief Hit information from ray casting
 */
struct RayHit {
    bool hit;
    double distance;
    int triangleIndex;
    Vector3 point;
    Vector3 normal;

    RayHit() : hit(false), distance(std::numeric_limits<double>::max()), triangleIndex(-1) {}
};

/**
 * @brief Axis-Aligned Bounding Box Tree for spatial acceleration
 *
 * Implements a simple BVH (Bounding Volume Hierarchy) for fast ray-triangle queries.
 * Essential for wall thickness analysis on large meshes.
 */
class AABBTree {
public:
    AABBTree() = default;
    ~AABBTree() = default;

    /**
     * @brief Build tree from mesh data
     * @param vertices Mesh vertex array
     * @param faces Mesh triangle array
     */
    void build(const std::vector<Vector3>& vertices,
              const std::vector<Triangle>& faces);

    /**
     * @brief Cast a ray through the tree
     * @param ray Ray to cast
     * @param maxDistance Maximum distance to search
     * @return Hit information
     */
    RayHit rayCast(const Ray& ray, double maxDistance = std::numeric_limits<double>::max()) const;

    /**
     * @brief Check if tree is built
     */
    bool isBuilt() const { return root != nullptr; }

private:
    struct Node {
        AABB bounds;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        std::vector<int> triangleIndices; // Leaf nodes only

        bool isLeaf() const { return !left && !right; }
    };

    std::unique_ptr<Node> root;
    const std::vector<Vector3>* vertices = nullptr;
    const std::vector<Triangle>* faces = nullptr;

    /**
     * @brief Recursively build tree
     */
    std::unique_ptr<Node> buildNode(std::vector<int>& triangleIndices, int depth);

    /**
     * @brief Compute AABB for a set of triangles
     */
    AABB computeBounds(const std::vector<int>& triangleIndices) const;

    /**
     * @brief Recursively traverse tree for ray casting
     */
    void rayCastRecursive(const Node* node, const Ray& ray,
                         double maxDistance, RayHit& bestHit) const;
};

/**
 * @brief Möller-Trumbore ray-triangle intersection
 *
 * @param ray Ray to test
 * @param v0, v1, v2 Triangle vertices
 * @param t Distance along ray to intersection (output)
 * @param u, v Barycentric coordinates (output)
 * @return true if intersection found
 */
bool intersectRayTriangle(const Ray& ray,
                         const Vector3& v0,
                         const Vector3& v1,
                         const Vector3& v2,
                         double& t,
                         double& u,
                         double& v);

/**
 * @brief Calculate triangle normal
 */
Vector3 calculateTriangleNormal(const Vector3& v0, const Vector3& v1, const Vector3& v2);

/**
 * @brief Calculate triangle area
 */
double calculateTriangleArea(const Vector3& v0, const Vector3& v1, const Vector3& v2);

} // namespace madfam::geom
