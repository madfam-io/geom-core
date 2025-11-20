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
