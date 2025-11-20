#include <pybind11/pybind11.h>
#include "geom-core/Analyzer.hpp"

namespace py = pybind11;

PYBIND11_MODULE(geom_core_py, m) {
    m.doc() = "geom-core: High-performance geometry analysis library";

    py::class_<madfam::geom::Analyzer>(m, "Analyzer")
        .def(py::init<>())
        .def("load_data", &madfam::geom::Analyzer::loadData,
             "Load geometry data (placeholder)",
             py::arg("data"))
        .def("get_mock_volume", &madfam::geom::Analyzer::getMockVolume,
             "Calculate mock volume using sphere formula: (4/3)*PI*r^3",
             py::arg("base_radius"))
        .def("add", &madfam::geom::Analyzer::add,
             "Add two integers (sanity check)",
             py::arg("a"), py::arg("b"));
}
