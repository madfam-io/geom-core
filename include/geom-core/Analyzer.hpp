#pragma once
#include <string>

namespace MadFam::Geom {
    class Analyzer {
    public:
        Analyzer() = default;
        ~Analyzer() = default;

        // Placeholder for "load_model"
        bool loadData(const std::string& data);

        // Placeholder for "get_volume" - returns mock value
        double getMockVolume(double base_radius);

        // Sanity check function
        int add(int a, int b);
    };
}
