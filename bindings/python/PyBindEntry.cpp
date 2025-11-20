#include <pybind11/pybind11.h>
#include "geom-core/Analyzer.hpp"
#include "geom-core/Vector3.hpp"

namespace py = pybind11;

PYBIND11_MODULE(geom_core_py, m) {
    m.doc() = "geom-core: High-performance geometry analysis library";

    // Vector3 class
    py::class_<madfam::geom::Vector3>(m, "Vector3")
        .def(py::init<>())
        .def(py::init<double, double, double>())
        .def_readwrite("x", &madfam::geom::Vector3::x)
        .def_readwrite("y", &madfam::geom::Vector3::y)
        .def_readwrite("z", &madfam::geom::Vector3::z)
        .def("length", &madfam::geom::Vector3::length, "Get vector length/magnitude")
        .def("norm", &madfam::geom::Vector3::norm, "Get vector norm (alias for length)")
        .def("__repr__", [](const madfam::geom::Vector3& v) {
            return "Vector3(" + std::to_string(v.x) + ", " +
                   std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        });

    // Analyzer class
    py::class_<madfam::geom::Analyzer>(m, "Analyzer")
        .def(py::init<>())

        // Real mesh analysis methods (Milestone 2)
        .def("load_stl", &madfam::geom::Analyzer::loadSTL,
             "Load a mesh from binary STL file",
             py::arg("filepath"))
        .def("get_volume", &madfam::geom::Analyzer::getVolume,
             "Calculate the volume of the loaded mesh")
        .def("is_watertight", &madfam::geom::Analyzer::isWatertight,
             "Check if the loaded mesh is watertight (manifold)")
        .def("get_bounding_box", &madfam::geom::Analyzer::getBoundingBox,
             "Get bounding box dimensions as Vector3(width, height, depth)")
        .def("get_vertex_count", &madfam::geom::Analyzer::getVertexCount,
             "Get number of vertices in the loaded mesh")
        .def("get_triangle_count", &madfam::geom::Analyzer::getTriangleCount,
             "Get number of triangles in the loaded mesh")

        // Legacy methods (for backward compatibility)
        .def("load_data", &madfam::geom::Analyzer::loadData,
             "Load geometry data (placeholder - deprecated)",
             py::arg("data"))
        .def("get_mock_volume", &madfam::geom::Analyzer::getMockVolume,
             "Calculate mock volume using sphere formula: (4/3)*PI*r^3 (deprecated)",
             py::arg("base_radius"))
        .def("add", &madfam::geom::Analyzer::add,
             "Add two integers (sanity check)",
             py::arg("a"), py::arg("b"));
}
