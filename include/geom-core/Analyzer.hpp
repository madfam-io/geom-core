#pragma once
#include <string>
#include <memory>
#include "Mesh.hpp"
#include "Vector3.hpp"
#include "Spatial.hpp"

namespace madfam::geom {

    /**
     * @brief Printability analysis report for 3D printing
     */
    struct PrintabilityReport {
        double overhangArea;         // mm² of surface requiring support
        double overhangPercentage;   // % of total surface area
        int thinWallVertexCount;     // Number of vertices with walls too thin
        double score;                // Overall printability score (0-100)
        double totalSurfaceArea;     // Total mesh surface area (mm²)

        PrintabilityReport()
            : overhangArea(0.0)
            , overhangPercentage(0.0)
            , thinWallVertexCount(0)
            , score(100.0)
            , totalSurfaceArea(0.0) {}
    };

    /**
     * @brief Auto-orientation optimization result (Milestone 5)
     */
    struct OrientationResult {
        Vector3 optimalUpVector;        // The direction that should point "Up"
        double originalOverhangArea;    // Overhang area before optimization (mm²)
        double optimizedOverhangArea;   // Overhang area after optimization (mm²)
        double improvementPercent;      // Percentage improvement (0-100)

        OrientationResult()
            : optimalUpVector(0, 0, 1)
            , originalOverhangArea(0.0)
            , optimizedOverhangArea(0.0)
            , improvementPercent(0.0) {}
    };

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
        // Printability Analysis (Milestone 4)
        // ========================================

        /**
         * @brief Build spatial acceleration structure for ray queries
         *
         * Call this after loading a mesh and before running printability analysis.
         * Required for wall thickness checks.
         */
        void buildSpatialIndex();

        /**
         * @brief Analyze printability for 3D printing
         *
         * @param criticalAngleDegrees Overhang angle threshold (typically 45°)
         * @param minWallThicknessMM Minimum wall thickness in mm (typically 0.8-2.0)
         * @return Printability report with overhang and thickness analysis
         *
         * The score is calculated as:
         * - Start at 100.0
         * - Subtract (overhangPercentage * 0.5) for support requirements
         * - Subtract (thinWallVertexCount / totalVertices * 50) for thin walls
         */
        PrintabilityReport getPrintabilityReport(double criticalAngleDegrees,
                                                 double minWallThicknessMM);

        // ========================================
        // Auto-Orientation (Milestone 5)
        // ========================================

        /**
         * @brief Find optimal mesh orientation to minimize overhang area
         *
         * @param sampleResolution Number of test orientations (default 26)
         * @param criticalAngleDegrees Overhang angle threshold (default 45°)
         * @return OrientationResult with optimal up vector and improvement metrics
         *
         * Uses discrete sphere sampling to test multiple orientations without
         * rotating the mesh vertices. Tests cardinals + 45° offsets.
         */
        OrientationResult autoOrient(int sampleResolution = 26,
                                     double criticalAngleDegrees = 45.0);

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
        std::unique_ptr<AABBTree> spatialTree;

        /**
         * @brief Calculate overhang area for a given up vector
         * @param upVector The direction considered "up" for this analysis
         * @param criticalAngleDegrees Overhang angle threshold
         * @param outTotalArea Output parameter for total surface area
         * @return Overhang area in mm²
         */
        double analyzeOverhangs(const Vector3& upVector,
                               double criticalAngleDegrees,
                               double& outTotalArea) const;
    };
}
