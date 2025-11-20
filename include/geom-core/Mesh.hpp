#pragma once
#include "Vector3.hpp"
#include <vector>
#include <string>

namespace madfam::geom {

/**
 * @brief Triangle face defined by 3 vertex indices
 */
struct Triangle {
    int v0, v1, v2;

    Triangle() : v0(0), v1(0), v2(0) {}
    Triangle(int a, int b, int c) : v0(a), v1(b), v2(c) {}
};

/**
 * @brief Triangular mesh data structure
 *
 * Stores vertices and faces for efficient topology analysis.
 * Supports loading from STL files and computing geometric properties.
 */
class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;

    /**
     * @brief Load mesh from binary STL file
     * @param filepath Path to the binary STL file
     * @return true if successful, false otherwise
     *
     * Automatically deduplicates vertices to create a proper connected mesh.
     */
    bool loadFromSTL(const std::string& filepath);

    /**
     * @brief Load mesh from binary STL data in memory
     * @param buffer Pointer to binary STL data
     * @param size Size of the buffer in bytes
     * @return true if successful, false otherwise
     *
     * Automatically deduplicates vertices to create a proper connected mesh.
     * This method is used for WASM where there's no file system.
     */
    bool loadFromSTLBuffer(const char* buffer, size_t size);

    /**
     * @brief Calculate the volume of the mesh using signed tetrahedron method
     * @return Volume in cubic units (mm³ if input is in mm)
     *
     * Formula: For each triangle (p1, p2, p3), compute:
     *   V += (1/6) * dot(p1, cross(p2, p3))
     */
    double getVolume() const;

    /**
     * @brief Check if the mesh is watertight (manifold)
     * @return true if every edge is shared by exactly 2 faces
     *
     * A manifold mesh has no holes or non-manifold edges.
     * This is a requirement for most 3D printing applications.
     */
    bool isWatertight() const;

    /**
     * @brief Get bounding box dimensions
     * @return Vector3 containing (width, height, depth)
     */
    Vector3 getBoundingBox() const;

    /**
     * @brief Get the number of vertices in the mesh
     */
    size_t getVertexCount() const { return vertices.size(); }

    /**
     * @brief Get the number of triangles in the mesh
     */
    size_t getTriangleCount() const { return faces.size(); }

    /**
     * @brief Clear all mesh data
     */
    void clear();

    /**
     * @brief Get const reference to vertex array (for spatial indexing)
     */
    const std::vector<Vector3>& getVertices() const { return vertices; }

    /**
     * @brief Get const reference to face array (for spatial indexing)
     */
    const std::vector<Triangle>& getFaces() const { return faces; }

private:
    std::vector<Vector3> vertices;
    std::vector<Triangle> faces;
};

} // namespace madfam::geom
