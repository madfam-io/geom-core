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

/**
 * @brief 3x3 rotation matrix for vector transformations
 *
 * Provides rotation operations without modifying mesh vertices.
 * Used for auto-orientation optimization.
 */
struct Matrix3 {
    double m[3][3];

    // Default constructor (identity matrix)
    Matrix3() {
        m[0][0] = 1.0; m[0][1] = 0.0; m[0][2] = 0.0;
        m[1][0] = 0.0; m[1][1] = 1.0; m[1][2] = 0.0;
        m[2][0] = 0.0; m[2][1] = 0.0; m[2][2] = 1.0;
    }

    // Constructor from array
    Matrix3(double m00, double m01, double m02,
            double m10, double m11, double m12,
            double m20, double m21, double m22) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }

    // Matrix-vector multiplication
    Vector3 operator*(const Vector3& v) const {
        return Vector3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }

    // Matrix-matrix multiplication
    Matrix3 operator*(const Matrix3& other) const {
        Matrix3 result;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                result.m[i][j] = 0.0;
                for (int k = 0; k < 3; k++) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }

    // Create rotation matrix from axis and angle (Rodrigues' rotation formula)
    static Matrix3 rotation(const Vector3& axis, double angleRadians) {
        Vector3 k = axis.normalized();
        double c = std::cos(angleRadians);
        double s = std::sin(angleRadians);
        double t = 1.0 - c;

        return Matrix3(
            t * k.x * k.x + c,       t * k.x * k.y - s * k.z, t * k.x * k.z + s * k.y,
            t * k.x * k.y + s * k.z, t * k.y * k.y + c,       t * k.y * k.z - s * k.x,
            t * k.x * k.z - s * k.y, t * k.y * k.z + s * k.x, t * k.z * k.z + c
        );
    }

    // Transpose (inverse for rotation matrices)
    Matrix3 transpose() const {
        return Matrix3(
            m[0][0], m[1][0], m[2][0],
            m[0][1], m[1][1], m[2][1],
            m[0][2], m[1][2], m[2][2]
        );
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
