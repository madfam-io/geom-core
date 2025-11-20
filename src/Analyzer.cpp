#include "geom-core/Analyzer.hpp"
#include <iostream>
#include <cmath>

namespace madfam::geom {

// Constructor
Analyzer::Analyzer() : mesh(std::make_unique<Mesh>()) {}

// Destructor
Analyzer::~Analyzer() = default;

// ========================================
// Real Mesh Analysis Methods (Milestone 2)
// ========================================

bool Analyzer::loadSTL(const std::string& filepath) {
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }
    return mesh->loadFromSTL(filepath);
}

bool Analyzer::loadSTLFromBytes(const std::string& data) {
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }
    return mesh->loadFromSTLBuffer(data.data(), data.size());
}

double Analyzer::getVolume() const {
    if (!mesh) return 0.0;
    return mesh->getVolume();
}

bool Analyzer::isWatertight() const {
    if (!mesh) return false;
    return mesh->isWatertight();
}

Vector3 Analyzer::getBoundingBox() const {
    if (!mesh) return Vector3(0, 0, 0);
    return mesh->getBoundingBox();
}

size_t Analyzer::getVertexCount() const {
    if (!mesh) return 0;
    return mesh->getVertexCount();
}

size_t Analyzer::getTriangleCount() const {
    if (!mesh) return 0;
    return mesh->getTriangleCount();
}

// ========================================
// Printability Analysis (Milestone 4)
// ========================================

void Analyzer::buildSpatialIndex() {
    if (!mesh || mesh->getVertexCount() == 0) {
        std::cerr << "Error: Cannot build spatial index - no mesh loaded" << std::endl;
        return;
    }

    spatialTree = std::make_unique<AABBTree>();
    spatialTree->build(mesh->getVertices(), mesh->getFaces());
    std::cout << "Built spatial index for " << mesh->getTriangleCount() << " triangles" << std::endl;
}

PrintabilityReport Analyzer::getPrintabilityReport(double criticalAngleDegrees,
                                                    double minWallThicknessMM) {
    PrintabilityReport report;

    if (!mesh || mesh->getVertexCount() == 0) {
        std::cerr << "Error: No mesh loaded" << std::endl;
        return report;
    }

    const double PI = 3.14159265358979323846;
    const double criticalAngleRad = criticalAngleDegrees * PI / 180.0;
    const double cosThreshold = std::cos(criticalAngleRad);

    const auto& vertices = mesh->getVertices();
    const auto& faces = mesh->getFaces();

    // ==================================
    // 1. Overhang Analysis
    // ==================================

    Vector3 upVector(0, 0, 1); // Z-up coordinate system
    double totalSurfaceArea = 0.0;
    double overhangArea = 0.0;

    for (const auto& face : faces) {
        const Vector3& v0 = vertices[face.v0];
        const Vector3& v1 = vertices[face.v1];
        const Vector3& v2 = vertices[face.v2];

        // Calculate face normal
        Vector3 normal = calculateTriangleNormal(v0, v1, v2);

        // Calculate face area
        double area = calculateTriangleArea(v0, v1, v2);
        totalSurfaceArea += area;

        // Check if this is an overhang (facing downward)
        // Dot product with up vector: negative means facing down
        double dotProduct = normal * upVector;

        // If dot < -cos(angle), it's an overhang
        if (dotProduct < -cosThreshold) {
            overhangArea += area;
        }
    }

    report.totalSurfaceArea = totalSurfaceArea;
    report.overhangArea = overhangArea;
    report.overhangPercentage = (totalSurfaceArea > 0.0)
        ? (overhangArea / totalSurfaceArea * 100.0)
        : 0.0;

    // ==================================
    // 2. Wall Thickness Analysis
    // ==================================

    if (spatialTree && spatialTree->isBuilt()) {
        int thinWallCount = 0;

        // Sample vertices for thickness check (checking every vertex can be slow)
        // For large meshes, sample every N vertices
        size_t sampleRate = (vertices.size() > 10000) ? 10 : 1;

        for (size_t i = 0; i < vertices.size(); i += sampleRate) {
            const Vector3& vertex = vertices[i];

            // Compute vertex normal (average of adjacent face normals)
            Vector3 vertexNormal(0, 0, 0);
            int faceCount = 0;

            for (const auto& face : faces) {
                if (face.v0 == static_cast<int>(i) ||
                    face.v1 == static_cast<int>(i) ||
                    face.v2 == static_cast<int>(i)) {
                    const Vector3& v0 = vertices[face.v0];
                    const Vector3& v1 = vertices[face.v1];
                    const Vector3& v2 = vertices[face.v2];
                    vertexNormal = vertexNormal + calculateTriangleNormal(v0, v1, v2);
                    faceCount++;
                }
            }

            if (faceCount > 0) {
                vertexNormal = vertexNormal.normalized();

                // Cast ray inward (negative normal direction)
                const double epsilon = 0.001; // Offset to avoid self-intersection
                Ray ray(vertex + vertexNormal * epsilon, vertexNormal * -1.0);

                // Cast ray to find opposite wall
                RayHit hit = spatialTree->rayCast(ray, minWallThicknessMM * 10.0); // Search up to 10x min thickness

                if (hit.hit && hit.distance < minWallThicknessMM) {
                    thinWallCount++;
                }
            }
        }

        report.thinWallVertexCount = thinWallCount;
    } else {
        std::cerr << "Warning: Spatial index not built - skipping wall thickness analysis" << std::endl;
        std::cerr << "Call buildSpatialIndex() before getPrintabilityReport()" << std::endl;
    }

    // ==================================
    // 3. Calculate Overall Score
    // ==================================

    double score = 100.0;

    // Penalty for overhangs (up to 50 points)
    score -= std::min(report.overhangPercentage * 0.5, 50.0);

    // Penalty for thin walls (up to 50 points)
    if (vertices.size() > 0) {
        double thinWallRatio = static_cast<double>(report.thinWallVertexCount) / vertices.size();
        score -= std::min(thinWallRatio * 50.0, 50.0);
    }

    report.score = std::max(0.0, score);

    return report;
}

// ========================================
// Legacy Methods (for backward compatibility)
// ========================================

bool Analyzer::loadData(const std::string& data) {
    std::cout << "Loading data: " << data << std::endl;
    return true;
}

double Analyzer::getMockVolume(double base_radius) {
    // Volume of sphere: (4/3) * PI * r^3
    const double PI = 3.14159265358979323846;
    return (4.0 / 3.0) * PI * std::pow(base_radius, 3);
}

int Analyzer::add(int a, int b) {
    return a + b;
}

} // namespace madfam::geom
