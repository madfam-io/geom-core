#include "geom-core/Analyzer.hpp"
#include <iostream>
#include <cmath>

namespace madfam::geom {

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
