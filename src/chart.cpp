#include "chart.hpp"
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <cmath>
#include <format>
#include <sstream>
#include <iomanip>

using namespace ftxui;

// ─── Color mapping ──────────────────────────────────────────────────────────

Color resolve_color(ChartColor c) {
    switch (c) {
        case ChartColor::Cyan:   return Color::Cyan;
        case ChartColor::Yellow: return Color::Yellow;
        case ChartColor::Red:    return Color::Red;
        case ChartColor::White:  return Color::White;
        default:                 return Color::Green;
    }
}

// ─── Helpers ────────────────────────────────────────────────────────────────

static double safe_range(double lo, double hi) {
    double r = hi - lo;
    return (r < 1e-9) ? 1.0 : r;
}

static int value_to_y(double v, double y_min, double y_max, int canvas_h) {
    double frac = (v - y_min) / safe_range(y_min, y_max);
    frac = std::clamp(frac, 0.0, 1.0);
    // Canvas y=0 is top; invert
    return static_cast<int>((1.0 - frac) * (canvas_h - 1));
}

static std::string fmt_val(double v) {
    std::ostringstream ss;
    if (std::abs(v) >= 1000.0)
        ss << std::fixed << std::setprecision(0) << v;
    else if (std::abs(v) >= 100.0)
        ss << std::fixed << std::setprecision(1) << v;
    else
        ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

// ─── Line chart ─────────────────────────────────────────────────────────────

Element make_line_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color,
    int width, int height)
{
    // Canvas uses 2x4 braille cells: canvas pixel size = width*2, height*4
    const int cw = width  * 2;
    const int ch = height * 4;

    Canvas canvas(cw, ch);

    if (data.empty()) {
        return ftxui::canvas(std::move(canvas));
    }

    const int n = static_cast<int>(data.size());

    // Draw horizontal grid lines (light)
    for (int g = 1; g < 4; ++g) {
        int gy = static_cast<int>(ch * g / 4.0);
        for (int x = 0; x < cw; x += 4)
            canvas.DrawPoint(x, gy, true, Color::GrayDark);
    }

    // Draw the polyline
    for (int i = 1; i < n; ++i) {
        double x0_frac = static_cast<double>(i - 1) / std::max(1, n - 1);
        double x1_frac = static_cast<double>(i)     / std::max(1, n - 1);
        int x0 = static_cast<int>(x0_frac * (cw - 1));
        int x1 = static_cast<int>(x1_frac * (cw - 1));
        int y0 = value_to_y(data[i - 1], y_min, y_max, ch);
        int y1 = value_to_y(data[i],     y_min, y_max, ch);
        canvas.DrawPointLine(x0, y0, x1, y1, color);
    }

    return ftxui::canvas(std::move(canvas));
}

// ─── Bar chart ───────────────────────────────────────────────────────────────

Element make_bar_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color,
    int width, int height)
{
    const int cw = width  * 2;
    const int ch = height * 4;

    Canvas canvas(cw, ch);

    if (data.empty()) return ftxui::canvas(std::move(canvas));

    const int n = static_cast<int>(data.size());
    const double bar_w = static_cast<double>(cw) / n;

    for (int i = 0; i < n; ++i) {
        int x0 = static_cast<int>(i       * bar_w);
        int x1 = static_cast<int>((i + 1) * bar_w) - 1;
        int y_top    = value_to_y(data[i], y_min, y_max, ch);
        int y_bottom = ch - 1;

        for (int x = x0; x <= x1; ++x)
            for (int y = y_top; y <= y_bottom; ++y)
                canvas.DrawPoint(x, y, true, color);
    }

    return ftxui::canvas(std::move(canvas));
}

// ─── Sparkline ───────────────────────────────────────────────────────────────

Element make_sparkline(
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color)
{
    static const wchar_t blocks[] = L" ▁▂▃▄▅▆▇█";
    constexpr int num_blocks = 8;

    if (data.empty()) return text("");

    std::wstring line;
    line.reserve(data.size());
    for (auto v : data) {
        double frac = (v - y_min) / safe_range(y_min, y_max);
        frac = std::clamp(frac, 0.0, 1.0);
        int idx = static_cast<int>(frac * num_blocks);
        idx = std::clamp(idx, 0, num_blocks);
        line += blocks[idx];
    }

    return text(std::string(line.begin(), line.end())) | ftxui::color(color);
}

// ─── Stats bar ───────────────────────────────────────────────────────────────

Element make_stats_bar(const RingBuffer<double>::Stats& stats, const std::string& unit) {
    auto lbl = [](const std::string& k, const std::string& v) {
        return hbox({
            text(k) | dim,
            text(v) | bold,
        });
    };

    std::string u = unit.empty() ? "" : " " + unit;

    return hbox({
        lbl(" cur: ", fmt_val(stats.current) + u),
        text("  "),
        lbl("min: ", fmt_val(static_cast<double>(stats.min_val)) + u),
        text("  "),
        lbl("max: ", fmt_val(static_cast<double>(stats.max_val)) + u),
        text("  "),
        lbl("avg: ", fmt_val(stats.mean) + u),
        text("  "),
        lbl("n: ",   std::to_string(stats.count)),
        filler(),
    });
}

// ─── Y axis ──────────────────────────────────────────────────────────────────

Element make_y_axis(double y_min, double y_max, int height_rows) {
    const int num_ticks = std::min(height_rows, 5);
    Elements ticks;
    ticks.reserve(static_cast<size_t>(num_ticks) + 1);

    for (int i = num_ticks - 1; i >= 0; --i) {
        double frac = static_cast<double>(i) / std::max(1, num_ticks - 1);
        double v    = y_min + frac * (y_max - y_min);
        std::string label = fmt_val(v);
        // Right-align in 8 chars
        while (static_cast<int>(label.size()) < 8) label = " " + label;
        ticks.push_back(text(label));
        if (i > 0) ticks.push_back(filler());
    }

    return vbox(std::move(ticks));
}
