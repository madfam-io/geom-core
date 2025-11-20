#pragma once
#include <string>
#include <memory>
#include "Mesh.hpp"
#include "Vector3.hpp"

namespace madfam::geom {
    /**
     * @brief High-level geometry analysis interface
     *
     * Provides convenient methods for loading and analyzing 3D models.
     * Wraps the lower-level Mesh class for ease of use in bindings.
     */
    class Analyzer {
    public:
        Analyzer();
        ~Analyzer();

        // ========================================
        // Real Mesh Analysis Methods (Milestone 2)
        // ========================================

        /**
         * @brief Load a mesh from an STL file
         * @param filepath Path to binary STL file
         * @return true if successful, false otherwise
         */
        bool loadSTL(const std::string& filepath);

        /**
         * @brief Load a mesh from STL data in memory (for WASM)
         * @param data Binary STL data as a string
         * @return true if successful, false otherwise
         *
         * In JavaScript/WASM, you can pass a Uint8Array or binary string.
         * Emscripten will automatically convert it to std::string.
         */
        bool loadSTLFromBytes(const std::string& data);

        /**
         * @brief Calculate the volume of the loaded mesh
         * @return Volume in cubic units (0.0 if no mesh loaded)
         */
        double getVolume() const;

        /**
         * @brief Check if the loaded mesh is watertight (manifold)
         * @return true if watertight, false otherwise
         */
        bool isWatertight() const;

        /**
         * @brief Get bounding box dimensions
         * @return Vector3 containing (width, height, depth)
         */
        Vector3 getBoundingBox() const;

        /**
         * @brief Get number of vertices in the loaded mesh
         */
        size_t getVertexCount() const;

        /**
         * @brief Get number of triangles in the loaded mesh
         */
        size_t getTriangleCount() const;

        // ========================================
        // Legacy Methods (for backward compatibility)
        // ========================================

        /**
         * @brief Legacy placeholder method
         * @deprecated Use loadSTL() instead
         */
        bool loadData(const std::string& data);

        /**
         * @brief Legacy mock volume calculation
         * @deprecated Use loadSTL() + getVolume() instead
         */
        double getMockVolume(double base_radius);

        /**
         * @brief Sanity check function
         */
        int add(int a, int b);

    private:
        std::unique_ptr<Mesh> mesh;
    };
}
