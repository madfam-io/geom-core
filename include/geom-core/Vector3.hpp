#pragma once
#include <cmath>
#include <functional>

namespace madfam::geom {

/**
 * @brief Simple 3D vector class for geometry operations
 *
 * Provides basic vector arithmetic for mesh analysis.
 * Zero external dependencies - uses only C++17 standard library.
 */
struct Vector3 {
    double x, y, z;

    // Constructors
    Vector3() : x(0.0), y(0.0), z(0.0) {}
    Vector3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    // Vector addition
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    // Vector subtraction
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    // Scalar multiplication
    Vector3 operator*(double scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    // Dot product (operator*)
    double operator*(const Vector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    // Cross product (operator%)
    Vector3 operator%(const Vector3& other) const {
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    // Vector length/magnitude
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    // Alias for length
    double norm() const {
        return length();
    }

    // Normalize (return unit vector)
    Vector3 normalized() const {
        double len = length();
        if (len < 1e-10) return Vector3(0, 0, 0);
        return Vector3(x / len, y / len, z / len);
    }

    // Equality comparison (with epsilon tolerance)
    bool operator==(const Vector3& other) const {
        const double epsilon = 1e-9;
        return std::abs(x - other.x) < epsilon &&
               std::abs(y - other.y) < epsilon &&
               std::abs(z - other.z) < epsilon;
    }

    bool operator!=(const Vector3& other) const {
        return !(*this == other);
    }

    // Less-than operator for use in std::map (lexicographic ordering)
    bool operator<(const Vector3& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

} // namespace madfam::geom

// Hash function for Vector3 to use in std::unordered_map
namespace std {
    template<>
    struct hash<madfam::geom::Vector3> {
        size_t operator()(const madfam::geom::Vector3& v) const {
            // Combine hashes of x, y, z
            size_t h1 = hash<double>{}(v.x);
            size_t h2 = hash<double>{}(v.y);
            size_t h3 = hash<double>{}(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}
