#include "config.hpp"
#include "renderer.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>

int main(int argc, char** argv) {
    CLI::App app{"Santana - pipe logs or numeric streams into live terminal charts"};
    app.set_version_flag("-V,--version", "1.0.0");

    Config cfg;
    app.add_option("-t,--title", cfg.title, "Chart title");

    std::string mode_str = "line";
    app.add_option("-m,--mode", mode_str, "Chart type: line, bar, or spark")
        ->check(CLI::IsMember({"line", "bar", "spark"}));
    app.add_option("-u,--unit", cfg.unit, "Unit label (e.g. MB/s, %)");

    double opt_min = 0.0, opt_max = 0.0;
    auto* min_opt = app.add_option("--min", opt_min, "Fixed Y axis minimum");
    auto* max_opt = app.add_option("--max", opt_max, "Fixed Y axis maximum");
    app.add_flag("--log-scale", cfg.log_scale, "Logarithmic Y axis");
    app.add_option("--history", cfg.history, "Samples to keep per stream");
    app.add_option("--fps", cfg.fps, "UI refresh rate");
    app.add_flag("-r,--rate", cfg.rate_mode, "Plot deltas per second for counters");

    CLI11_PARSE(app, argc, argv);

    if      (mode_str == "bar")   cfg.mode = ChartMode::Bar;
    else if (mode_str == "spark") cfg.mode = ChartMode::Spark;
    else                          cfg.mode = ChartMode::Line;

    if (min_opt->count()) {
        cfg.auto_min = false;
        cfg.y_min = opt_min;
    }
    if (max_opt->count()) {
        cfg.auto_max = false;
        cfg.y_max = opt_max;
    }

    cfg.history = std::max(10, std::min(10000, cfg.history));
    cfg.fps = std::max(1, std::min(120, cfg.fps));

    const int input_fd = ::dup(STDIN_FILENO);
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

    std::string first_line;
    if (!::isatty(input_fd)) {
        char c;
        while (first_line.size() < 4096 && ::read(input_fd, &c, 1) == 1) {
            first_line += c;
            if (c == '\n') break;
        }
    }

    Renderer renderer(cfg, input_fd, std::move(first_line));
    renderer.run();

    return 0;
}
