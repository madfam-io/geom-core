/**
 * @file STLReader.cpp
 * @brief STL file reader (binary and ASCII)
 */

#include "geom-core/cad/Types.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace madfam::geom::io {

using namespace madfam::geom::cad;

namespace {

// Check if file is binary STL
bool isBinarySTL(std::ifstream& file) {
    char header[80];
    file.read(header, 80);

    uint32_t numTriangles;
    file.read(reinterpret_cast<char*>(&numTriangles), 4);

    // Check if file size matches expected binary size
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Binary STL: 80 byte header + 4 byte count + (50 bytes * triangles)
    auto expectedSize = 84 + (numTriangles * 50);
    return static_cast<size_t>(fileSize) == expectedSize;
}

Result<MeshData> readBinarySTL(std::ifstream& file) {
    MeshData mesh;

    // Skip header
    file.seekg(80);

    // Read triangle count
    uint32_t numTriangles;
    file.read(reinterpret_cast<char*>(&numTriangles), 4);

    // Pre-allocate
    mesh.reserve(numTriangles * 3, numTriangles);

    // Read triangles
    for (uint32_t i = 0; i < numTriangles; ++i) {
        // Normal (we'll recompute)
        float normal[3];
        file.read(reinterpret_cast<char*>(normal), 12);

        // Vertices
        for (int v = 0; v < 3; ++v) {
            float vertex[3];
            file.read(reinterpret_cast<char*>(vertex), 12);

            mesh.positions.push_back(vertex[0]);
            mesh.positions.push_back(vertex[1]);
            mesh.positions.push_back(vertex[2]);

            mesh.normals.push_back(normal[0]);
            mesh.normals.push_back(normal[1]);
            mesh.normals.push_back(normal[2]);

            mesh.indices.push_back(i * 3 + v);
        }

        // Attribute byte count (ignored)
        uint16_t attr;
        file.read(reinterpret_cast<char*>(&attr), 2);
    }

    mesh.vertexCount = numTriangles * 3;
    mesh.triangleCount = numTriangles;
    mesh.computeByteSize();

    return Result<MeshData>::ok(std::move(mesh));
}

Result<MeshData> readASCIISTL(std::ifstream& file) {
    MeshData mesh;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "facet") {
            // Read normal
            std::string normalKeyword;
            float nx, ny, nz;
            iss >> normalKeyword >> nx >> ny >> nz;

            // Read vertices
            std::getline(file, line); // outer loop

            for (int v = 0; v < 3; ++v) {
                std::getline(file, line);
                std::istringstream viss(line);
                std::string vertexKeyword;
                float x, y, z;
                viss >> vertexKeyword >> x >> y >> z;

                mesh.positions.push_back(x);
                mesh.positions.push_back(y);
                mesh.positions.push_back(z);

                mesh.normals.push_back(nx);
                mesh.normals.push_back(ny);
                mesh.normals.push_back(nz);

                mesh.indices.push_back(static_cast<uint32_t>(mesh.indices.size()));
            }

            std::getline(file, line); // endloop
            std::getline(file, line); // endfacet
        }
    }

    mesh.vertexCount = static_cast<uint32_t>(mesh.positions.size() / 3);
    mesh.triangleCount = mesh.vertexCount / 3;
    mesh.computeByteSize();

    return Result<MeshData>::ok(std::move(mesh));
}

}  // namespace

/**
 * @brief Read STL file (auto-detects binary vs ASCII)
 */
Result<MeshData> readSTL(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return Result<MeshData>::fail("IO_ERROR", "Failed to open file: " + filepath);
    }

    if (isBinarySTL(file)) {
        file.seekg(0);
        return readBinarySTL(file);
    } else {
        file.close();
        file.open(filepath);
        return readASCIISTL(file);
    }
}

/**
 * @brief Read STL from memory buffer
 */
Result<MeshData> readSTLFromMemory(const uint8_t* data, size_t size) {
    if (size < 84) {
        return Result<MeshData>::fail("INVALID_DATA", "STL data too small");
    }

    // Check for ASCII
    const char* header = reinterpret_cast<const char*>(data);
    if (strncmp(header, "solid", 5) == 0) {
        // Might be ASCII, but could also be binary with "solid" in header
        uint32_t numTriangles;
        memcpy(&numTriangles, data + 80, 4);
        auto expectedSize = 84 + (numTriangles * 50);

        if (size != expectedSize) {
            // Treat as ASCII
            std::string str(reinterpret_cast<const char*>(data), size);
            std::istringstream stream(str);
            // Parse ASCII... (simplified, use file version for production)
            return Result<MeshData>::fail("NOT_IMPLEMENTED", "ASCII STL from memory not yet implemented");
        }
    }

    // Binary STL
    MeshData mesh;

    uint32_t numTriangles;
    memcpy(&numTriangles, data + 80, 4);

    mesh.reserve(numTriangles * 3, numTriangles);

    const uint8_t* ptr = data + 84;

    for (uint32_t i = 0; i < numTriangles; ++i) {
        float normal[3];
        memcpy(normal, ptr, 12);
        ptr += 12;

        for (int v = 0; v < 3; ++v) {
            float vertex[3];
            memcpy(vertex, ptr, 12);
            ptr += 12;

            mesh.positions.push_back(vertex[0]);
            mesh.positions.push_back(vertex[1]);
            mesh.positions.push_back(vertex[2]);

            mesh.normals.push_back(normal[0]);
            mesh.normals.push_back(normal[1]);
            mesh.normals.push_back(normal[2]);

            mesh.indices.push_back(i * 3 + v);
        }

        ptr += 2; // Skip attribute
    }

    mesh.vertexCount = numTriangles * 3;
    mesh.triangleCount = numTriangles;
    mesh.computeByteSize();

    return Result<MeshData>::ok(std::move(mesh));
}

}  // namespace madfam::geom::io
