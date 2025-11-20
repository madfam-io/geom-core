#include <pybind11/pybind11.h>
#include "geom-core/Analyzer.hpp"

namespace py = pybind11;

PYBIND11_MODULE(geom_core_py, m) {
    m.doc() = "geom-core: High-performance geometry analysis library";

    py::class_<MadFam::Geom::Analyzer>(m, "Analyzer")
        .def(py::init<>())
        .def("load_data", &MadFam::Geom::Analyzer::loadData,
             "Load geometry data (placeholder)",
             py::arg("data"))
        .def("get_mock_volume", &MadFam::Geom::Analyzer::getMockVolume,
             "Calculate mock volume using sphere formula: (4/3)*PI*r^3",
             py::arg("base_radius"))
        .def("add", &MadFam::Geom::Analyzer::add,
             "Add two integers (sanity check)",
             py::arg("a"), py::arg("b"));
}
