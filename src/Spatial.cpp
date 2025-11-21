#include "geom-core/Spatial.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace madfam::geom {

// ==========================================
// AABB Implementation
// ==========================================

bool AABB::intersect(const Ray& ray, double& tMin, double& tMax) const {
    // Slab method for ray-box intersection
    tMin = 0.0;
    tMax = std::numeric_limits<double>::max();

    for (int i = 0; i < 3; ++i) {
        double origin_i = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
        double dir_i = (i == 0) ? ray.direction.x : (i == 1) ? ray.direction.y : ray.direction.z;
        double min_i = (i == 0) ? min.x : (i == 1) ? min.y : min.z;
        double max_i = (i == 0) ? max.x : (i == 1) ? max.y : max.z;

        if (std::abs(dir_i) < 1e-8) {
            // Ray is parallel to slab
            if (origin_i < min_i || origin_i > max_i) {
                return false;
            }
        } else {
            double invD = 1.0 / dir_i;
            double t1 = (min_i - origin_i) * invD;
            double t2 = (max_i - origin_i) * invD;

            if (t1 > t2) std::swap(t1, t2);

            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);

            if (tMin > tMax) {
                return false;
            }
        }
    }

    return true;
}

// ==========================================
// Ray-Triangle Intersection (Möller-Trumbore)
// ==========================================

bool intersectRayTriangle(const Ray& ray,
                         const Vector3& v0,
                         const Vector3& v1,
                         const Vector3& v2,
                         double& t,
                         double& u,
                         double& v) {
    const double EPSILON = 1e-8;

    // Compute edges
    Vector3 edge1 = v1 - v0;
    Vector3 edge2 = v2 - v0;

    // Compute determinant
    Vector3 h = ray.direction % edge2; // cross product
    double a = edge1 * h; // dot product

    // Check if ray is parallel to triangle
    if (std::abs(a) < EPSILON) {
        return false;
    }

    double f = 1.0 / a;
    Vector3 s = ray.origin - v0;
    u = f * (s * h);

    if (u < 0.0 || u > 1.0) {
        return false;
    }

    Vector3 q = s % edge1;
    v = f * (ray.direction * q);

    if (v < 0.0 || u + v > 1.0) {
        return false;
    }

    // Compute t
    t = f * (edge2 * q);

    if (t > EPSILON) {
        return true; // Ray intersection
    }

    return false; // Line intersection but not ray
}

// ==========================================
// Geometry Utilities
// ==========================================

Vector3 calculateTriangleNormal(const Vector3& v0, const Vector3& v1, const Vector3& v2) {
    Vector3 edge1 = v1 - v0;
    Vector3 edge2 = v2 - v0;
    return (edge1 % edge2).normalized();
}

double calculateTriangleArea(const Vector3& v0, const Vector3& v1, const Vector3& v2) {
    Vector3 edge1 = v1 - v0;
    Vector3 edge2 = v2 - v0;
    return (edge1 % edge2).length() * 0.5;
}

// ==========================================
// AABBTree Implementation
// ==========================================

void AABBTree::build(const std::vector<Vector3>& verts,
                    const std::vector<Triangle>& tris) {
    vertices = &verts;
    faces = &tris;

    // Create list of all triangle indices
    std::vector<int> triangleIndices(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) {
        triangleIndices[i] = static_cast<int>(i);
    }

    // Build tree recursively
    root = buildNode(triangleIndices, 0);
}

AABB AABBTree::computeBounds(const std::vector<int>& triangleIndices) const {
    AABB bounds;

    for (int triIdx : triangleIndices) {
        const Triangle& tri = (*faces)[triIdx];
        bounds.expand((*vertices)[tri.v0]);
        bounds.expand((*vertices)[tri.v1]);
        bounds.expand((*vertices)[tri.v2]);
    }

    return bounds;
}

std::unique_ptr<AABBTree::Node> AABBTree::buildNode(std::vector<int>& triangleIndices, int depth) {
    auto node = std::make_unique<Node>();

    // Compute bounds for this node
    node->bounds = computeBounds(triangleIndices);

    // Leaf condition: few triangles or max depth
    const int MAX_LEAF_TRIANGLES = 10;
    const int MAX_DEPTH = 32;

    if (triangleIndices.size() <= MAX_LEAF_TRIANGLES || depth >= MAX_DEPTH) {
        // Create leaf
        node->triangleIndices = std::move(triangleIndices);
        return node;
    }

    // Choose split axis (longest axis)
    Vector3 extent = node->bounds.max - node->bounds.min;
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent.x && extent.z > extent.y) axis = 2;

    // Sort triangles by centroid along axis
    std::sort(triangleIndices.begin(), triangleIndices.end(),
        [this, axis](int a, int b) {
            const Triangle& triA = (*faces)[a];
            const Triangle& triB = (*faces)[b];

            Vector3 centroidA = ((*vertices)[triA.v0] + (*vertices)[triA.v1] + (*vertices)[triA.v2]) * (1.0 / 3.0);
            Vector3 centroidB = ((*vertices)[triB.v0] + (*vertices)[triB.v1] + (*vertices)[triB.v2]) * (1.0 / 3.0);

            double valA = (axis == 0) ? centroidA.x : (axis == 1) ? centroidA.y : centroidA.z;
            double valB = (axis == 0) ? centroidB.x : (axis == 1) ? centroidB.y : centroidB.z;

            return valA < valB;
        });

    // Split in half
    size_t mid = triangleIndices.size() / 2;
    std::vector<int> leftIndices(triangleIndices.begin(), triangleIndices.begin() + mid);
    std::vector<int> rightIndices(triangleIndices.begin() + mid, triangleIndices.end());

    // Recursively build children
    node->left = buildNode(leftIndices, depth + 1);
    node->right = buildNode(rightIndices, depth + 1);

    return node;
}

RayHit AABBTree::rayCast(const Ray& ray, double maxDistance) const {
    RayHit bestHit;

    if (!root) {
        return bestHit;
    }

    rayCastRecursive(root.get(), ray, maxDistance, bestHit);
    return bestHit;
}

void AABBTree::rayCastRecursive(const Node* node, const Ray& ray,
                                double maxDistance, RayHit& bestHit) const {
    if (!node) {
        return;
    }

    // Test ray against bounding box
    double tMin, tMax;
    if (!node->bounds.intersect(ray, tMin, tMax)) {
        return; // Ray misses this node
    }

    if (tMin > maxDistance || tMin > bestHit.distance) {
        return; // Too far away
    }

    if (node->isLeaf()) {
        // Test all triangles in leaf
        for (int triIdx : node->triangleIndices) {
            const Triangle& tri = (*faces)[triIdx];
            const Vector3& v0 = (*vertices)[tri.v0];
            const Vector3& v1 = (*vertices)[tri.v1];
            const Vector3& v2 = (*vertices)[tri.v2];

            double t, u, v;
            if (intersectRayTriangle(ray, v0, v1, v2, t, u, v)) {
                if (t < bestHit.distance && t < maxDistance && t > 1e-6) {
                    bestHit.hit = true;
                    bestHit.distance = t;
                    bestHit.triangleIndex = triIdx;
                    bestHit.point = ray.at(t);
                    bestHit.normal = calculateTriangleNormal(v0, v1, v2);
                }
            }
        }
    } else {
        // Recurse into children
        rayCastRecursive(node->left.get(), ray, maxDistance, bestHit);
        rayCastRecursive(node->right.get(), ray, maxDistance, bestHit);
    }
}

} // namespace madfam::geom
