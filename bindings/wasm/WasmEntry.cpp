#include <emscripten/bind.h>
#include "geom-core/Analyzer.hpp"
#include "geom-core/Vector3.hpp"

using namespace emscripten;
using namespace madfam::geom;

EMSCRIPTEN_BINDINGS(geom_core_module) {
    // Vector3 class
    value_object<Vector3>("Vector3")
        .field("x", &Vector3::x)
        .field("y", &Vector3::y)
        .field("z", &Vector3::z);

    // Analyzer class
    class_<Analyzer>("Analyzer")
        .constructor<>()
        .function("loadSTLFromBytes", &Analyzer::loadSTLFromBytes)
        .function("loadSTL", &Analyzer::loadSTL)
        .function("getVolume", &Analyzer::getVolume)
        .function("isWatertight", &Analyzer::isWatertight)
        .function("getBoundingBox", &Analyzer::getBoundingBox)
        .function("getVertexCount", &Analyzer::getVertexCount)
        .function("getTriangleCount", &Analyzer::getTriangleCount);
}
