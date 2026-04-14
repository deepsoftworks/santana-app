#pragma once
#include "config.hpp"
#include "ring_buffer.hpp"
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <string>
#include <vector>

struct ChartOptions {
    bool log_scale = false;
    std::vector<const std::vector<double>*> extra_data;
    std::vector<ftxui::Color> extra_colors;
};

// Draw a line chart onto a canvas
ftxui::Element make_line_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    ftxui::Color color,
    int width, int height,
    const ChartOptions& opts = {});

// Draw a bar chart onto a canvas
ftxui::Element make_bar_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    ftxui::Color color,
    int width, int height,
    const ChartOptions& opts = {});

// Draw a sparkline (single row), with optional second row for two-graph
ftxui::Element make_sparkline(
    const std::vector<double>& data,
    double y_min, double y_max,
    ftxui::Color color,
    const ChartOptions& opts = {});

// Build the bottom status panel with inline legend + stats.
ftxui::Element make_status_panel(
    const std::vector<RingBuffer<double>::Stats>& all_stats,
    const std::vector<std::string>& labels,
    const std::vector<ftxui::Color>& colors,
    const std::string& unit,
    bool rate_mode = false,
    size_t max_rows = 0,
    bool eof_seen = false);

// Build Y-axis tick labels
ftxui::Element make_y_axis(double y_min, double y_max, int height_rows, bool log_scale = false);
