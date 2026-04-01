#pragma once
#include <string>
#include <vector>

enum class ChartMode  { Line, Bar, Spark };
enum class ChartColor { Green, Cyan, Yellow, Red, White };

// Per-element terminal colors (0=black..7=white, -1 = use default)
struct ElementColors {
    int plot      = -1;   // chart line / bars
    int plot2     = -1;   // stream 2 color override (default: White)
    int axes      = -1;   // Y axis labels + separator
    int text_col  = -1;   // stats footer
    int title_col = -1;   // title
    int max_err   =  1;   // error indicator > hard_max (default: red)
    int min_err   =  1;   // error indicator < hard_min (default: red)
};

struct Config {
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

    int          num_streams      = 1;     // -n: interleaved input streams (1-10)
    bool         explicit_streams = false; // true when -n/-2 was given explicitly
    std::vector<std::string> stream_labels;   // optional label per stream
    std::vector<std::string> field_selectors; // JSON fields to extract (positional args)
    std::string  jq_path;                     // JSON dot-path, e.g. ".metrics.latency"

    bool         rate_mode      = false;
    char         plot_char      = '\0';
    char         err_max_char   = 'e';
    char         err_min_char   = 'v';
    double       hard_max       = 0.0;
    bool         has_hard_max   = false;
    double       hard_min       = 0.0;
    bool         has_hard_min   = false;
    double       soft_scale     = 0.0;
    bool         has_soft_scale = false;
    ElementColors elem_colors;
};
