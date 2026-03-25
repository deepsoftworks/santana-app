#pragma once
#include <string>

enum class ChartMode  { Line, Bar, Spark };
enum class ChartColor { Green, Cyan, Yellow, Red, White };

// Per-element terminal colors (0=black..7=white, -1 = use default)
struct ElementColors {
    int plot      = -1;   // chart line / bars
    int plot2     = -1;   // second graph in two-graph mode (default: White)
    int axes      = -1;   // Y axis labels + separator
    int text_col  = -1;   // stats footer
    int title_col = -1;   // title
    int max_err   =  1;   // error indicator > hard_max (default: red)
    int min_err   =  1;   // error indicator < hard_min (default: red)
};

struct Config {
    // Existing fields
    std::string  title          = "santana";
    ChartMode    mode           = ChartMode::Line;
    std::string  unit           = "";
    double       y_min          = 0.0;
    double       y_max          = 0.0;
    bool         auto_min       = true;
    bool         auto_max       = true;
    bool         scroll         = false;
    int          history        = 120;
    int          fps            = 16;
    ChartColor   color          = ChartColor::Green;

    // New fields
    bool         two_graph      = false;    // -2: dual plot mode
    bool         rate_mode      = false;    // -r: counter rate (val/dt)
    char         plot_char      = '\0';     // -c: custom plot char ('\0' = braille)
    char         err_max_char   = 'e';      // -e: char drawn when > hard_max
    char         err_min_char   = 'v';      // -E: char drawn when < hard_min
    double       hard_max       = 0.0;      // --hard-max: fixed upper limit + error indicator
    bool         has_hard_max   = false;
    double       hard_min       = 0.0;      // --hard-min: fixed lower limit + error indicator
    bool         has_hard_min   = false;
    double       soft_scale     = 0.0;      // --scale: initial soft Y-axis scale
    bool         has_soft_scale = false;
    ElementColors elem_colors;              // -C: per-element color control
};
