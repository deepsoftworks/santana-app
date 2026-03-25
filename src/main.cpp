#include "config.hpp"
#include "renderer.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    CLI::App app{"Santana — real-time terminal data visualizer"};
    app.set_version_flag("-V,--version", "1.0.0");

    Config cfg;

    app.add_option("-t,--title",   cfg.title,   "Chart title");

    std::string mode_str = "line";
    app.add_option("-m,--mode", mode_str,
        "Chart type: line (default), bar, spark")
        ->check(CLI::IsMember({"line", "bar", "spark"}));

    app.add_option("-u,--unit", cfg.unit, "Unit label (e.g. MB/s, %)");

    double opt_min = 0.0, opt_max = 0.0;
    auto* min_opt = app.add_option("--min", opt_min, "Fixed Y axis minimum");
    auto* max_opt = app.add_option("--max", opt_max, "Fixed Y axis maximum");

    app.add_flag("-s,--scroll", cfg.scroll, "Scroll mode");
    app.add_option("--history", cfg.history, "Number of data points to keep (default: 120)");
    app.add_option("--fps",     cfg.fps,     "Target refresh rate (default: 16)");

    std::string color_str = "green";
    app.add_option("--color", color_str,
        "Chart color: green, cyan, yellow, red, white")
        ->check(CLI::IsMember({"green", "cyan", "yellow", "red", "white"}));

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

    // Fixed range
    if (min_opt->count()) { cfg.auto_min = false; cfg.y_min = opt_min; }
    if (max_opt->count()) { cfg.auto_max = false; cfg.y_max = opt_max; }

    // Clamp fps/history
    cfg.fps     = std::max(1,  std::min(120, cfg.fps));
    cfg.history = std::max(10, std::min(10000, cfg.history));

    Renderer renderer(cfg);
    renderer.run();

    return 0;
}
