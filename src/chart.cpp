#include "chart.hpp"
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace ftxui;

namespace {

double safe_range(double lo, double hi) {
    const double range = hi - lo;
    return (range < 1e-9) ? 1.0 : range;
}

int value_to_y(double value, double y_min, double y_max, int canvas_h, bool log_scale = false) {
    double frac = 0.0;
    if (log_scale) {
        const double lv   = std::log10(std::max(value, 1e-10));
        const double lmin = std::log10(std::max(y_min, 1e-10));
        const double lmax = std::log10(std::max(y_max, 1e-10));
        frac = (lv - lmin) / safe_range(lmin, lmax);
    } else {
        frac = (value - y_min) / safe_range(y_min, y_max);
    }

    frac = std::clamp(frac, 0.0, 1.0);
    return static_cast<int>((1.0 - frac) * (canvas_h - 1));
}

std::string fmt_val(double value) {
    std::ostringstream ss;
    if (std::abs(value) >= 1000.0) {
        ss << std::fixed << std::setprecision(0) << value;
    } else if (std::abs(value) >= 100.0) {
        ss << std::fixed << std::setprecision(1) << value;
    } else {
        ss << std::fixed << std::setprecision(2) << value;
    }
    return ss.str();
}

std::string fmt_metric(double value, const std::string& unit, bool rate_mode) {
    std::string result = fmt_val(value);
    if (!unit.empty()) result += " " + unit;
    if (rate_mode) result += "/s";
    return result;
}

void draw_dataset(
    Canvas& canvas,
    const std::vector<double>& data,
    double y_min,
    double y_max,
    Color color,
    int cw,
    int ch,
    bool log_scale = false)
{
    const int n = static_cast<int>(data.size());
    if (n == 0) return;

    for (int i = 1; i < n; ++i) {
        const double x0f = static_cast<double>(i - 1) / std::max(1, n - 1);
        const double x1f = static_cast<double>(i) / std::max(1, n - 1);
        const int x0 = static_cast<int>(x0f * (cw - 1));
        const int x1 = static_cast<int>(x1f * (cw - 1));
        const int y0 = value_to_y(data[i - 1], y_min, y_max, ch, log_scale);
        const int y1 = value_to_y(data[i], y_min, y_max, ch, log_scale);
        canvas.DrawPointLine(x0, y0, x1, y1, color);
    }
}

Element make_metric(const std::string& label, const std::string& value) {
    return hbox({
        text(label) | dim,
        text(value) | bold,
    });
}

}  // namespace

Element make_line_chart(
    const std::vector<double>& data,
    double y_min,
    double y_max,
    Color color,
    int width,
    int height,
    const ChartOptions& opts)
{
    const int cw = width * 2;
    const int ch = height * 4;
    Canvas canvas(cw, ch);

    if (data.empty() && opts.extra_data.empty()) return ftxui::canvas(std::move(canvas));

    for (int row = 1; row < 4; ++row) {
        const int gy = static_cast<int>(ch * row / 4.0);
        for (int x = 0; x < cw; x += 4) {
            canvas.DrawPoint(x, gy, true, Color::GrayDark);
        }
    }

    draw_dataset(canvas, data, y_min, y_max, color, cw, ch, opts.log_scale);
    for (size_t i = 0; i < opts.extra_data.size(); ++i) {
        if (opts.extra_data[i] && !opts.extra_data[i]->empty()) {
            draw_dataset(canvas, *opts.extra_data[i], y_min, y_max, opts.extra_colors[i], cw, ch, opts.log_scale);
        }
    }

    return ftxui::canvas(std::move(canvas));
}

Element make_bar_chart(
    const std::vector<double>& data,
    double y_min,
    double y_max,
    Color color,
    int width,
    int height,
    const ChartOptions& opts)
{
    const int cw = width * 2;
    const int ch = height * 4;
    Canvas canvas(cw, ch);

    if (data.empty()) return ftxui::canvas(std::move(canvas));

    const int groups = 1 + static_cast<int>(opts.extra_data.size());
    const int n = static_cast<int>(data.size());
    const double bar_width = static_cast<double>(cw) / std::max(1, n * groups);

    auto draw_bars = [&](const std::vector<double>& values, Color current_color, int offset) {
        for (int i = 0; i < static_cast<int>(values.size()); ++i) {
            const int x0 = static_cast<int>((i * groups + offset) * bar_width);
            const int x1 = static_cast<int>((i * groups + offset + 1) * bar_width) - 1;
            const int y_top = value_to_y(values[i], y_min, y_max, ch, opts.log_scale);
            for (int x = x0; x <= x1; ++x) {
                for (int y = y_top; y < ch; ++y) {
                    canvas.DrawPoint(x, y, true, current_color);
                }
            }
        }
    };

    draw_bars(data, color, 0);
    for (size_t i = 0; i < opts.extra_data.size(); ++i) {
        if (opts.extra_data[i] && !opts.extra_data[i]->empty()) {
            draw_bars(*opts.extra_data[i], opts.extra_colors[i], static_cast<int>(i) + 1);
        }
    }

    return ftxui::canvas(std::move(canvas));
}

static Element make_spark_row(
    const std::vector<double>& data,
    double y_min,
    double y_max,
    Color stream_color,
    bool log_scale = false)
{
    static const char* blocks[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    constexpr int num_blocks = 8;

    if (data.empty()) return text("");

    std::string line;
    line.reserve(data.size() * 3);
    for (double value : data) {
        double frac = 0.0;
        if (log_scale) {
            const double lv   = std::log10(std::max(value, 1e-10));
            const double lmin = std::log10(std::max(y_min, 1e-10));
            const double lmax = std::log10(std::max(y_max, 1e-10));
            frac = (lv - lmin) / safe_range(lmin, lmax);
        } else {
            frac = (value - y_min) / safe_range(y_min, y_max);
        }
        frac = std::clamp(frac, 0.0, 1.0);
        const int index = std::clamp(static_cast<int>(frac * num_blocks), 0, num_blocks);
        line += blocks[index];
    }

    return text(line) | ftxui::color(stream_color);
}

Element make_sparkline(
    const std::vector<double>& data,
    double y_min,
    double y_max,
    Color color,
    const ChartOptions& opts)
{
    Elements rows;
    rows.push_back(make_spark_row(data, y_min, y_max, color, opts.log_scale));
    for (size_t i = 0; i < opts.extra_data.size(); ++i) {
        if (opts.extra_data[i]) {
            rows.push_back(make_spark_row(*opts.extra_data[i], y_min, y_max, opts.extra_colors[i], opts.log_scale));
        }
    }

    return rows.size() == 1 ? rows.front() : vbox(std::move(rows));
}

Element make_status_panel(
    const std::vector<RingBuffer<double>::Stats>& all_stats,
    const std::vector<std::string>& labels,
    const std::vector<ftxui::Color>& colors,
    const std::string& unit,
    bool rate_mode,
    size_t max_rows,
    bool eof_seen)
{
    const size_t total_rows = all_stats.size();
    const size_t visible_rows = (max_rows == 0) ? total_rows : std::min(total_rows, max_rows);
    const size_t hidden_rows = (visible_rows < total_rows) ? total_rows - visible_rows : 0;

    size_t label_width = 8;
    for (size_t i = 0; i < visible_rows; ++i) {
        if (i < labels.size()) label_width = std::max(label_width, labels[i].size());
    }
    label_width = std::min<size_t>(label_width, 18);

    Elements rows;
    rows.push_back(hbox({
        text(" streams ") | bold | ftxui::color(Color::Cyan),
        filler(),
        text(std::to_string(total_rows) + " active") | dim,
    }));
    rows.push_back(separator());

    if (all_stats.empty()) {
        rows.push_back(text("waiting for data...") | dim);
    } else {
        for (size_t i = 0; i < visible_rows; ++i) {
            const auto stream_color = colors.empty() ? Color::Green : colors[i % colors.size()];
            const auto& stats = all_stats[i];
            const std::string label = (i < labels.size() && !labels[i].empty())
                ? labels[i]
                : "stream " + std::to_string(i + 1);

            rows.push_back(hbox({
                text("● ") | ftxui::color(stream_color),
                text(label) | bold | size(WIDTH, EQUAL, static_cast<int>(label_width)),
                text("  "),
                make_metric("now ", fmt_metric(stats.current, unit, rate_mode)),
                text("  "),
                make_metric("min ", fmt_metric(static_cast<double>(stats.min_val), unit, rate_mode)),
                text("  "),
                make_metric("max ", fmt_metric(static_cast<double>(stats.max_val), unit, rate_mode)),
                text("  "),
                make_metric("avg ", fmt_metric(stats.mean, unit, rate_mode)),
                text("  "),
                make_metric("n ", std::to_string(stats.count)),
                filler(),
            }));
        }
    }

    if (hidden_rows > 0) {
        rows.push_back(text("+" + std::to_string(hidden_rows) + " more streams") | dim);
    }
    if (eof_seen) {
        rows.push_back(text("input closed") | dim | ftxui::color(Color::Yellow));
    }

    return vbox(std::move(rows)) | border;
}

Element make_y_axis(double y_min, double y_max, int height_rows, bool log_scale) {
    const int num_ticks = std::min(height_rows, 5);
    Elements ticks;
    ticks.reserve(static_cast<size_t>(num_ticks) + 1);

    for (int i = num_ticks - 1; i >= 0; --i) {
        const double frac = static_cast<double>(i) / std::max(1, num_ticks - 1);
        double value = 0.0;
        if (log_scale) {
            const double lmin = std::log10(std::max(y_min, 1e-10));
            const double lmax = std::log10(std::max(y_max, 1e-10));
            value = std::pow(10.0, lmin + frac * (lmax - lmin));
        } else {
            value = y_min + frac * (y_max - y_min);
        }

        std::string label = fmt_val(value);
        while (static_cast<int>(label.size()) < 8) label = " " + label;
        ticks.push_back(text(label));
        if (i > 0) ticks.push_back(filler());
    }

    return vbox(std::move(ticks));
}
