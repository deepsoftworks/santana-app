#include "chart.hpp"
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <cmath>
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

Color resolve_elem_color(int idx, Color fallback) {
    if (idx < 0) return fallback;
    static const Color table[] = {
        Color::Black, Color::Red, Color::Green, Color::Yellow,
        Color::Blue,  Color::Magenta, Color::Cyan, Color::White,
    };
    return table[std::clamp(idx, 0, 7)];
}

// ─── Helpers ────────────────────────────────────────────────────────────────

static double safe_range(double lo, double hi) {
    double r = hi - lo;
    return (r < 1e-9) ? 1.0 : r;
}

static int value_to_y(double v, double y_min, double y_max, int canvas_h) {
    double frac = (v - y_min) / safe_range(y_min, y_max);
    frac = std::clamp(frac, 0.0, 1.0);
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

// Draw a single dataset onto the canvas (braille polyline or custom char)
static void draw_dataset(
    Canvas& canvas,
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color,
    char plot_char,
    int cw, int ch)
{
    const int n = static_cast<int>(data.size());
    if (n == 0) return;

    if (plot_char != '\0') {
        // Custom character: place at each data point's cell position
        std::string ch_str(1, plot_char);
        for (int i = 0; i < n; ++i) {
            double xf = static_cast<double>(i) / std::max(1, n - 1);
            int px = static_cast<int>(xf * (cw - 1));
            int py = value_to_y(data[i], y_min, y_max, ch);
            canvas.DrawText(px, py, ch_str, color);
        }
    } else {
        // Braille polyline
        for (int i = 1; i < n; ++i) {
            double x0f = static_cast<double>(i - 1) / std::max(1, n - 1);
            double x1f = static_cast<double>(i)     / std::max(1, n - 1);
            int x0 = static_cast<int>(x0f * (cw - 1));
            int x1 = static_cast<int>(x1f * (cw - 1));
            int y0 = value_to_y(data[i - 1], y_min, y_max, ch);
            int y1 = value_to_y(data[i],     y_min, y_max, ch);
            canvas.DrawPointLine(x0, y0, x1, y1, color);
        }
    }
}

// Draw error indicators for hard limits
static void draw_error_indicators(
    Canvas& canvas,
    const std::vector<double>& data,
    double y_min, double y_max,
    const ChartOptions& opts,
    int cw, int ch)
{
    const int n = static_cast<int>(data.size());
    for (int i = 0; i < n; ++i) {
        double xf = static_cast<double>(i) / std::max(1, n - 1);
        int px = static_cast<int>(xf * (cw - 1));
        if (opts.has_hard_max && data[i] > opts.hard_max)
            canvas.DrawText(px, 0, std::string(1, opts.err_max_char), opts.err_max_color);
        if (opts.has_hard_min && data[i] < opts.hard_min)
            canvas.DrawText(px, ch - 1, std::string(1, opts.err_min_char), opts.err_min_color);
    }
}

// ─── Line chart ─────────────────────────────────────────────────────────────

Element make_line_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color,
    int width, int height,
    const ChartOptions& opts)
{
    const int cw = width  * 2;
    const int ch = height * 4;

    Canvas canvas(cw, ch);

    if (data.empty() && opts.extra_data.empty())
        return ftxui::canvas(std::move(canvas));

    // Grid lines
    for (int g = 1; g < 4; ++g) {
        int gy = static_cast<int>(ch * g / 4.0);
        for (int x = 0; x < cw; x += 4)
            canvas.DrawPoint(x, gy, true, Color::GrayDark);
    }

    draw_dataset(canvas, data, y_min, y_max, color, opts.plot_char, cw, ch);

    for (size_t i = 0; i < opts.extra_data.size(); ++i) {
        if (opts.extra_data[i] && !opts.extra_data[i]->empty())
            draw_dataset(canvas, *opts.extra_data[i], y_min, y_max, opts.extra_colors[i], opts.plot_char, cw, ch);
    }

    draw_error_indicators(canvas, data, y_min, y_max, opts, cw, ch);
    for (size_t i = 0; i < opts.extra_data.size(); ++i) {
        if (opts.extra_data[i] && !opts.extra_data[i]->empty())
            draw_error_indicators(canvas, *opts.extra_data[i], y_min, y_max, opts, cw, ch);
    }

    return ftxui::canvas(std::move(canvas));
}

// ─── Bar chart ───────────────────────────────────────────────────────────────

Element make_bar_chart(
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color,
    int width, int height,
    const ChartOptions& opts)
{
    const int cw = width  * 2;
    const int ch = height * 4;

    Canvas canvas(cw, ch);

    if (data.empty()) return ftxui::canvas(std::move(canvas));

    const int n = static_cast<int>(data.size());
    // Interleave bars across all series
    const int groups  = 1 + static_cast<int>(opts.extra_data.size());
    const double bar_w = static_cast<double>(cw) / (n * groups);

    auto draw_bars = [&](const std::vector<double>& d, Color c, int offset) {
        for (int i = 0; i < static_cast<int>(d.size()); ++i) {
            int x0 = static_cast<int>((i * groups + offset) * bar_w);
            int x1 = static_cast<int>((i * groups + offset + 1) * bar_w) - 1;
            int y_top    = value_to_y(d[i], y_min, y_max, ch);
            int y_bottom = ch - 1;
            for (int x = x0; x <= x1; ++x)
                for (int y = y_top; y <= y_bottom; ++y)
                    canvas.DrawPoint(x, y, true, c);
        }
    };

    draw_bars(data, color, 0);
    for (size_t i = 0; i < opts.extra_data.size(); ++i) {
        if (opts.extra_data[i] && !opts.extra_data[i]->empty())
            draw_bars(*opts.extra_data[i], opts.extra_colors[i], static_cast<int>(i) + 1);
    }

    draw_error_indicators(canvas, data, y_min, y_max, opts, cw, ch);

    return ftxui::canvas(std::move(canvas));
}

// ─── Sparkline ───────────────────────────────────────────────────────────────

static Element make_spark_row(
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color)
{
    static const char* blocks[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    constexpr int num_blocks = 8;
    if (data.empty()) return text("");
    std::string line;
    line.reserve(data.size() * 3);
    for (auto v : data) {
        double frac = (v - y_min) / safe_range(y_min, y_max);
        frac = std::clamp(frac, 0.0, 1.0);
        int idx = std::clamp(static_cast<int>(frac * num_blocks), 0, num_blocks);
        line += blocks[idx];
    }
    return text(line) | ftxui::color(color);
}

Element make_sparkline(
    const std::vector<double>& data,
    double y_min, double y_max,
    Color color,
    const ChartOptions& opts)
{
    Elements rows;
    rows.push_back(make_spark_row(data, y_min, y_max, color));
    for (size_t i = 0; i < opts.extra_data.size(); ++i) {
        if (opts.extra_data[i])
            rows.push_back(make_spark_row(*opts.extra_data[i], y_min, y_max, opts.extra_colors[i]));
    }
    if (rows.size() == 1) return rows[0];
    return vbox(std::move(rows));
}

// ─── Stats bar ───────────────────────────────────────────────────────────────

Element make_stats_bar(
    const std::vector<RingBuffer<double>::Stats>& all_stats,
    const std::string& unit,
    bool rate_mode,
    const std::vector<std::string>& labels)
{
    auto lbl = [](const std::string& k, const std::string& v) {
        return hbox({ text(k) | dim, text(v) | bold });
    };

    std::string u = unit.empty() ? "" : " " + unit;
    std::string rate_tag = rate_mode ? "/s" : "";

    auto row = [&](const RingBuffer<double>::Stats& s, const std::string& label) {
        Elements elems;
        if (!label.empty())
            elems.push_back(text("[" + label + "] ") | bold);
        elems.push_back(lbl(" cur: ", fmt_val(s.current) + u + rate_tag));
        elems.push_back(text("  "));
        elems.push_back(lbl("min: ", fmt_val(static_cast<double>(s.min_val)) + u + rate_tag));
        elems.push_back(text("  "));
        elems.push_back(lbl("max: ", fmt_val(static_cast<double>(s.max_val)) + u + rate_tag));
        elems.push_back(text("  "));
        elems.push_back(lbl("avg: ", fmt_val(s.mean) + u + rate_tag));
        elems.push_back(text("  "));
        elems.push_back(lbl("n: ", std::to_string(s.count)));
        elems.push_back(filler());
        return hbox(std::move(elems));
    };

    if (all_stats.size() == 1) {
        std::string label = labels.empty() ? "" : labels[0];
        return row(all_stats[0], label);
    }

    Elements rows;
    for (size_t i = 0; i < all_stats.size(); ++i) {
        std::string label = (i < labels.size()) ? labels[i] : "";
        rows.push_back(row(all_stats[i], label));
    }
    return vbox(std::move(rows));
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
        while (static_cast<int>(label.size()) < 8) label = " " + label;
        ticks.push_back(text(label));
        if (i > 0) ticks.push_back(filler());
    }

    return vbox(std::move(ticks));
}
