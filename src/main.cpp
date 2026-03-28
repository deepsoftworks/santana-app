#include "config.hpp"
#include "renderer.hpp"
#include <CLI/CLI.hpp>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

// Parse -C color spec: named scheme or comma-separated terminal color indices (0-7)
static ElementColors parse_color_spec(const std::string& s) {
    ElementColors ec;
    if (s == "dark1")   { ec.plot=4; ec.axes=6; ec.text_col=3; ec.title_col=3; return ec; }
    if (s == "dark2")   { ec.plot=5; ec.axes=3; ec.text_col=2; ec.title_col=3; return ec; }
    if (s == "light1")  { ec.plot=2; ec.axes=4; ec.text_col=1; ec.title_col=2; return ec; }
    if (s == "light2")  { ec.plot=4; ec.axes=2; ec.text_col=3; ec.title_col=4; return ec; }
    if (s == "vampire") { ec.plot=1; ec.plot2=5; ec.axes=0; ec.text_col=0; ec.title_col=1; return ec; }

    // plot[,axes,text,title,max_err,min_err]
    int* fields[] = {
        &ec.plot, &ec.axes, &ec.text_col, &ec.title_col, &ec.max_err, &ec.min_err
    };
    std::istringstream ss(s);
    std::string token;
    std::size_t idx = 0;
    while (std::getline(ss, token, ',') && idx < 6) {
        try { *fields[idx] = std::clamp(std::stoi(token), 0, 7); } catch (...) {}
        ++idx;
    }
    return ec;
}

int main(int argc, char** argv) {
    CLI::App app{"Santana — real-time terminal data visualizer"};
    app.set_version_flag("-V,--version", "1.0.0");

    Config cfg;

    app.add_option("-t,--title", cfg.title, "Chart title");

    std::string mode_str = "line";
    app.add_option("-m,--mode", mode_str,
        "Chart type: line (default), bar, spark")
        ->check(CLI::IsMember({"line", "bar", "spark"}));

    app.add_option("-u,--unit", cfg.unit, "Unit label (e.g. MB/s, %)");

    double opt_min = 0.0, opt_max = 0.0;
    auto* min_opt = app.add_option("--min", opt_min, "Fixed Y axis minimum (no error indicator)");
    auto* max_opt = app.add_option("--max", opt_max, "Fixed Y axis maximum (no error indicator)");

    app.add_flag("-s,--scroll", cfg.scroll, "Scroll mode");
    app.add_option("--history", cfg.history, "Number of data points to keep (default: 120)");
    app.add_option("--fps",     cfg.fps,     "Target refresh rate (default: 16)");

    std::string color_str = "green";
    app.add_option("--color", color_str,
        "Chart color: green, cyan, yellow, red, white")
        ->check(CLI::IsMember({"green", "cyan", "yellow", "red", "white"}));

    std::string color2_str;
    app.add_option("--color2", color2_str,
        "Second graph color (two-graph mode): black, red, green, yellow, blue, magenta, cyan, white")
        ->check(CLI::IsMember({"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"}));

    // ── New flags ────────────────────────────────────────────────────────────
    app.add_flag("-2", cfg.two_graph,
        "Read two values and draw two plots (second in contrasting color)");

    app.add_flag("-r,--rate", cfg.rate_mode,
        "Rate mode: divide value by elapsed time between samples (for counters)");

    std::string plot_char_str;
    app.add_option("-c,--char", plot_char_str,
        "Character to use for plot line, e.g. @ # % . (default: braille)");

    std::string err_max_str, err_min_str;
    app.add_option("-e,--error-max-char", err_max_str,
        "Character for error indicator when value exceeds hard max (default: e)");
    app.add_option("-E,--error-min-char", err_min_str,
        "Character for error indicator when value is below hard min (default: v)");

    double hard_max_val = 0.0, hard_min_val = 0.0;
    auto* hard_max_opt = app.add_option("--hard-max", hard_max_val,
        "Hard maximum: if exceeded draws error line and fixes upper scale");
    auto* hard_min_opt = app.add_option("--hard-min", hard_min_val,
        "Hard minimum: if below draws error symbol and fixes lower scale");

    double scale_val = 0.0;
    auto* scale_opt = app.add_option("--scale", scale_val,
        "Initial soft Y axis scale (autoscale can exceed this)");

    std::string color_spec;
    app.add_option("-C,--colors", color_spec,
        "Per-element colors: plot[,axes,text,title,max_err,min_err] (0-7)\n"
        "  or named scheme: dark1, dark2, light1, light2, vampire\n"
        "  Colors: 0=black 1=red 2=green 3=yellow 4=blue 5=magenta 6=cyan 7=white");

    CLI11_PARSE(app, argc, argv);

    // Resolve mode
    if      (mode_str == "bar")   cfg.mode = ChartMode::Bar;
    else if (mode_str == "spark") cfg.mode = ChartMode::Spark;
    else                          cfg.mode = ChartMode::Line;

    // Resolve color
    if      (color_str == "cyan")   cfg.color = ChartColor::Cyan;
    else if (color_str == "yellow") cfg.color = ChartColor::Yellow;
    else if (color_str == "red")    cfg.color = ChartColor::Red;
    else if (color_str == "white")  cfg.color = ChartColor::White;
    else                            cfg.color = ChartColor::Green;

    // Fixed range (axis-only, no error indicator)
    if (min_opt->count()) { cfg.auto_min = false; cfg.y_min = opt_min; }
    if (max_opt->count()) { cfg.auto_max = false; cfg.y_max = opt_max; }

    // Hard limits (fix scale AND draw error indicator)
    if (hard_max_opt->count()) {
        cfg.has_hard_max = true;
        cfg.hard_max     = hard_max_val;
        cfg.auto_max     = false;
        cfg.y_max        = hard_max_val;
    }
    if (hard_min_opt->count()) {
        cfg.has_hard_min = true;
        cfg.hard_min     = hard_min_val;
        cfg.auto_min     = false;
        cfg.y_min        = hard_min_val;
    }

    // Soft initial scale
    if (scale_opt->count()) {
        cfg.has_soft_scale = true;
        cfg.soft_scale     = scale_val;
    }

    // Plot / error chars
    if (!plot_char_str.empty()) cfg.plot_char    = plot_char_str[0];
    if (!err_max_str.empty())   cfg.err_max_char = err_max_str[0];
    if (!err_min_str.empty())   cfg.err_min_char = err_min_str[0];

    // Per-element colors
    if (!color_spec.empty()) cfg.elem_colors = parse_color_spec(color_spec);

    // Second graph color (applied after -C so it takes precedence)
    if (!color2_str.empty()) {
        static const std::pair<const char*, int> color2_map[] = {
            {"black",   0}, {"red",     1}, {"green", 2}, {"yellow", 3},
            {"blue",    4}, {"magenta", 5}, {"cyan",  6}, {"white",  7},
        };
        for (auto& [name, idx] : color2_map)
            if (color2_str == name) { cfg.elem_colors.plot2 = idx; break; }
    }

    cfg.fps     = std::max(1,  std::min(120, cfg.fps));
    cfg.history = std::max(10, std::min(10000, cfg.history));

    int input_fd = ::dup(STDIN_FILENO);
    if (input_fd < 0) {
        std::cerr << "Failed to duplicate stdin\n";
        return 1;
    }

    if (!::isatty(STDIN_FILENO)) {
        int tty_fd = ::open("/dev/tty", O_RDONLY);
        if (tty_fd < 0 || ::dup2(tty_fd, STDIN_FILENO) < 0) {
            if (tty_fd >= 0) ::close(tty_fd);
            std::cerr << "Failed to open /dev/tty for interactive input\n";
            ::close(input_fd);
            return 1;
        }
        ::close(tty_fd);
    }

    Renderer renderer(cfg, input_fd);
    renderer.run();

    return 0;
}
