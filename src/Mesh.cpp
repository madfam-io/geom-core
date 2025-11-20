#include "geom-core/Mesh.hpp"
#include <fstream>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <cstring>
#include <cstdint>
#include <iostream>

namespace madfam::geom {

bool Mesh::loadFromSTL(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open STL file: " << filepath << std::endl;
        return false;
    }

    // Clear existing data
    clear();

    // Read 80-byte header (ignore it)
    char header[80];
    file.read(header, 80);
    if (!file.good()) {
        std::cerr << "Error: Failed to read STL header" << std::endl;
        return false;
    }

    // Read triangle count (4 bytes, little-endian unsigned int)
    uint32_t triangleCount;
    file.read(reinterpret_cast<char*>(&triangleCount), 4);
    if (!file.good()) {
        std::cerr << "Error: Failed to read triangle count" << std::endl;
        return false;
    }

    // Map to deduplicate vertices: vertex -> index
    std::map<Vector3, int> vertexMap;
    int nextVertexIndex = 0;

    // Reserve space for efficiency
    faces.reserve(triangleCount);

    // Read each triangle
    for (uint32_t i = 0; i < triangleCount; ++i) {
        // Read normal vector (12 bytes) - we'll skip it
        float normal[3];
        file.read(reinterpret_cast<char*>(normal), 12);
        if (!file.good()) {
            std::cerr << "Error: Failed to read normal for triangle " << i << std::endl;
            break;
        }

        // Read 3 vertices (12 bytes each = 3 floats per vertex)
        int indices[3];
        for (int j = 0; j < 3; ++j) {
            float coords[3];
            file.read(reinterpret_cast<char*>(coords), 12);
            if (!file.good()) {
                std::cerr << "Error: Failed to read vertex " << j << " of triangle " << i << std::endl;
                break;
            }

            Vector3 vertex(coords[0], coords[1], coords[2]);

            // Check if vertex already exists in map
            auto it = vertexMap.find(vertex);
            if (it != vertexMap.end()) {
                // Vertex exists, reuse index
                indices[j] = it->second;
            } else {
                // New vertex, add to map and vertices list
                indices[j] = nextVertexIndex;
                vertexMap[vertex] = nextVertexIndex;
                vertices.push_back(vertex);
                nextVertexIndex++;
            }
        }

        // Create triangle from indices
        faces.emplace_back(indices[0], indices[1], indices[2]);

        // Read attribute byte count (2 bytes) - skip it
        uint16_t attributeByteCount;
        file.read(reinterpret_cast<char*>(&attributeByteCount), 2);
        if (!file.good() && i < triangleCount - 1) {
            std::cerr << "Error: Failed to read attribute count for triangle " << i << std::endl;
            break;
        }
    }

    file.close();

    std::cout << "Loaded STL: " << vertices.size() << " vertices, "
              << faces.size() << " triangles" << std::endl;

    return true;
}

double Mesh::getVolume() const {
    if (faces.empty()) {
        return 0.0;
    }

    double volume = 0.0;

    // For each triangle, calculate signed tetrahedron volume
    // Formula: V = (1/6) * dot(p1, cross(p2, p3))
    for (const auto& face : faces) {
        const Vector3& p1 = vertices[face.v0];
        const Vector3& p2 = vertices[face.v1];
        const Vector3& p3 = vertices[face.v2];

        // Calculate cross product: p2 x p3
        Vector3 crossProduct = p2 % p3;

        // Calculate dot product: p1 · (p2 x p3)
        double dotProduct = p1 * crossProduct;

        // Add signed volume contribution
        volume += dotProduct;
    }

    // Divide by 6 and take absolute value
    return std::abs(volume / 6.0);
}

bool Mesh::isWatertight() const {
    if (faces.empty()) {
        return false;
    }

    // Edge map: (vertex_i, vertex_j) -> count
    // We use min/max to ensure consistent edge representation
    std::map<std::pair<int, int>, int> edgeCount;

    // Iterate over all faces and count edges
    for (const auto& face : faces) {
        // Each triangle has 3 edges
        int edges[3][2] = {
            {face.v0, face.v1},
            {face.v1, face.v2},
            {face.v2, face.v0}
        };

        for (int i = 0; i < 3; ++i) {
            int a = edges[i][0];
            int b = edges[i][1];

            // Normalize edge representation (smaller index first)
            std::pair<int, int> edge = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);

            // Increment counter
            edgeCount[edge]++;
        }
    }

    // Check if all edges have exactly 2 faces
    for (const auto& entry : edgeCount) {
        if (entry.second != 2) {
            // Found non-manifold edge or boundary edge
            return false;
        }
    }

    return true;
}

Vector3 Mesh::getBoundingBox() const {
    if (vertices.empty()) {
        return Vector3(0, 0, 0);
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double minZ = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    double maxZ = std::numeric_limits<double>::lowest();

    for (const auto& vertex : vertices) {
        minX = std::min(minX, vertex.x);
        minY = std::min(minY, vertex.y);
        minZ = std::min(minZ, vertex.z);
        maxX = std::max(maxX, vertex.x);
        maxY = std::max(maxY, vertex.y);
        maxZ = std::max(maxZ, vertex.z);
    }

    return Vector3(maxX - minX, maxY - minY, maxZ - minZ);
}

void Mesh::clear() {
    vertices.clear();
    faces.clear();
}

} // namespace madfam::geom
