#pragma once
#include "config.hpp"
#include "ring_buffer.hpp"
#include <ftxui/screen/color.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

// Map ChartColor enum to ftxui Color
ftxui::Color resolve_color(ChartColor c);

// Map terminal color index (0-7, -1=unset) to ftxui Color with a fallback
ftxui::Color resolve_elem_color(int idx, ftxui::Color fallback);

// Options shared by chart drawing functions
struct ChartOptions {
    char plot_char      = '\0';             // '\0' = braille polyline
    char err_max_char   = 'e';
    char err_min_char   = 'v';
    bool has_hard_max   = false;
    double hard_max     = 0.0;
    bool has_hard_min   = false;
    double hard_min     = 0.0;
    ftxui::Color err_max_color = ftxui::Color::Red;
    ftxui::Color err_min_color = ftxui::Color::Red;
    // Optional second dataset (two-graph mode)
    const std::vector<double>* data2 = nullptr;
    ftxui::Color color2 = ftxui::Color::White;
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

// Build the stats footer text (one or two graphs)
ftxui::Element make_stats_bar(
    const RingBuffer<double>::Stats& stats,
    const std::string& unit,
    bool rate_mode = false,
    const RingBuffer<double>::Stats* stats2 = nullptr);

// Build Y-axis tick labels
ftxui::Element make_y_axis(double y_min, double y_max, int height_rows);
