#pragma once
#include <string>

enum class ChartMode { Line, Bar, Spark };
enum class ChartColor { Green, Cyan, Yellow, Red, White };

struct Config {
    std::string title    = "santana";
    ChartMode   mode     = ChartMode::Line;
    std::string unit     = "";
    double      y_min    = 0.0;
    double      y_max    = 0.0;
    bool        auto_min = true;
    bool        auto_max = true;
    bool        scroll   = false;
    int         history  = 120;
    int         fps      = 16;
    ChartColor  color    = ChartColor::Green;
};
