#pragma once

#ifdef GC_USE_OCCT

#include <string>

// Forward declarations to avoid exposing OCCT headers
namespace madfam::geom {
    class Mesh;
}

namespace madfam::geom::brep {

/**
 * @brief Load STEP file using Open CASCADE Technology
 *
 * This function is only available when compiled with OCCT support.
 * It reads a STEP file, tessellates the geometry, and populates a Mesh object.
 *
 * @param filepath Path to the STEP file (.step or .stp)
 * @param outMesh Output mesh to populate with triangulated data
 * @param linearDeflection Tessellation precision in mm (default 0.1mm)
 * @param angularDeflection Angular precision in radians (default 0.5)
 * @return true if successful, false otherwise
 */
bool loadStepFile(const std::string& filepath, 
                  Mesh& outMesh,
                  double linearDeflection = 0.1,
                  double angularDeflection = 0.5);

} // namespace madfam::geom::brep

#endif // GC_USE_OCCT
