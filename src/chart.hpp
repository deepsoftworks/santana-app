#pragma once
#include "config.hpp"
#include "ring_buffer.hpp"
#include <ftxui/screen/color.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

ftxui::Color resolve_color(ChartColor c);

// Draw a line chart onto a canvas
ftxui::Element make_line_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    ftxui::Color color,
    int width, int height);

// Draw a bar chart onto a canvas
ftxui::Element make_bar_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    ftxui::Color color,
    int width, int height);

// Draw a sparkline (single row)
ftxui::Element make_sparkline(
    const std::vector<double>& data,
    double y_min, double y_max,
    ftxui::Color color);

// Build the stats footer text
ftxui::Element make_stats_bar(
    const RingBuffer<double>::Stats& stats,
    const std::string& unit);

// Build Y-axis tick labels
ftxui::Element make_y_axis(double y_min, double y_max, int height_rows);
