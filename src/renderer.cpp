#include "renderer.hpp"
#include "chart.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <chrono>

using namespace ftxui;

Renderer::Renderer(Config cfg)
    : cfg_(std::move(cfg))
    , buffer_(static_cast<size_t>(cfg_.history))
    , buffer2_(static_cast<size_t>(cfg_.history))
    , rate_mode_(cfg_.rate_mode)
{}

Renderer::~Renderer() {
    running_ = false;
    if (reader_.joinable()) reader_.join();
}

void Renderer::reader_thread() {
    struct RateState {
        double prev_val = 0.0;
        std::chrono::steady_clock::time_point prev_time;
        bool has_prev = false;
    };
    RateState rs1, rs2;
    int line_index = 0;

    auto push_value = [&](double raw, RateState& rs, RingBuffer<double>& buf) {
        if (rate_reset_.exchange(false)) {
            rs = {};
        }
        if (rate_mode_.load()) {
            auto now = std::chrono::steady_clock::now();
            if (rs.has_prev) {
                double dt = std::chrono::duration<double>(now - rs.prev_time).count();
                if (dt > 0) buf.push((raw - rs.prev_val) / dt);
            }
            rs.prev_val  = raw;
            rs.prev_time = now;
            rs.has_prev  = true;
        } else {
            buf.push(raw);
        }
    };

    while (running_) {
        source_.poll();
        while (source_.ready()) {
            double raw = source_.next();
            if (cfg_.two_graph) {
                if (line_index % 2 == 0)
                    push_value(raw, rs1, buffer_);
                else
                    push_value(raw, rs2, buffer2_);
                ++line_index;
            } else {
                push_value(raw, rs1, buffer_);
            }
        }
        if (source_.is_eof() && !eof_seen_.load()) {
            eof_seen_ = true;
        }
    }
}

void Renderer::compute_range(double& y_min, double& y_max) const {
    auto st1 = buffer_.stats();
    double data_min = static_cast<double>(st1.min_val);
    double data_max = static_cast<double>(st1.max_val);

    if (cfg_.two_graph) {
        auto st2 = buffer2_.stats();
        data_min = std::min(data_min, static_cast<double>(st2.min_val));
        data_max = std::max(data_max, static_cast<double>(st2.max_val));
    }

    if (cfg_.auto_min) {
        y_min = data_min;
        double span = data_max - y_min;
        y_min -= span * 0.05;
    } else {
        y_min = cfg_.y_min;
    }
    if (cfg_.auto_max) {
        y_max = data_max;
        double span = y_max - data_min;
        y_max += span * 0.05;
        if (cfg_.has_soft_scale && y_max < cfg_.soft_scale)
            y_max = cfg_.soft_scale;
    } else {
        y_max = cfg_.y_max;
    }
    if (std::abs(y_max - y_min) < 1e-9) {
        y_min -= 1.0;
        y_max += 1.0;
    }
}

void Renderer::run() {
    reader_ = std::thread(&Renderer::reader_thread, this);

    auto screen = ScreenInteractive::Fullscreen();

    // Resolve element colors
    auto& ec        = cfg_.elem_colors;
    Color plot_col  = resolve_elem_color(ec.plot,      resolve_color(cfg_.color));
    Color plot2_col = resolve_elem_color(ec.plot2,     Color::White);
    Color axes_col  = resolve_elem_color(ec.axes,      Color::Default);
    Color text_col  = resolve_elem_color(ec.text_col,  Color::Default);
    Color title_col = resolve_elem_color(ec.title_col, Color::Default);
    Color max_err_col = resolve_elem_color(ec.max_err, Color::Red);
    Color min_err_col = resolve_elem_color(ec.min_err, Color::Red);

    // Ticker thread: fires a Custom event at ~fps rate
    std::thread ticker([&] {
        auto interval = std::chrono::milliseconds(1000 / std::max(1, cfg_.fps));
        while (running_) {
            std::this_thread::sleep_for(interval);
            if (running_) screen.PostEvent(Event::Custom);
        }
    });

    auto render_fn = [&]() -> Element {
        auto data  = buffer_.snapshot();
        auto stats = buffer_.stats();

        double y_min = 0.0, y_max = 1.0;
        compute_range(y_min, y_max);

        int term_w = screen.dimx();
        int term_h = screen.dimy();

        // Two-graph mode stats footer is two rows tall
        int stats_rows = cfg_.two_graph ? 2 : 1;
        int chart_h = std::max(4, term_h - 5 - stats_rows);
        int chart_w = std::max(10, term_w - 12);

        std::string title_str = cfg_.title;
        if (!cfg_.unit.empty()) title_str += "  [" + cfg_.unit + "]";
        if (rate_mode_.load())  title_str += "  (rate)";

        // Build ChartOptions
        ChartOptions opts;
        opts.plot_char    = cfg_.plot_char;
        opts.err_max_char = cfg_.err_max_char;
        opts.err_min_char = cfg_.err_min_char;
        opts.has_hard_max = cfg_.has_hard_max;
        opts.hard_max     = cfg_.hard_max;
        opts.has_hard_min = cfg_.has_hard_min;
        opts.hard_min     = cfg_.hard_min;
        opts.err_max_color = max_err_col;
        opts.err_min_color = min_err_col;
        opts.color2       = plot2_col;

        std::vector<double> data2;
        if (cfg_.two_graph) {
            data2 = buffer2_.snapshot();
            opts.data2 = &data2;
        }

        Element chart_elem;
        if (cfg_.mode == ChartMode::Spark) {
            chart_elem = vbox({
                filler(),
                make_sparkline(data, y_min, y_max, plot_col, opts),
                filler(),
            });
        } else if (cfg_.mode == ChartMode::Bar) {
            chart_elem = make_bar_chart(data, y_min, y_max, plot_col, chart_w, chart_h, opts);
        } else {
            chart_elem = make_line_chart(data, y_min, y_max, plot_col, chart_w, chart_h, opts);
        }

        Element y_axis_elem = make_y_axis(y_min, y_max, chart_h) | ftxui::color(axes_col);

        const RingBuffer<double>::Stats* stats2_ptr = nullptr;
        RingBuffer<double>::Stats stats2;
        if (cfg_.two_graph) {
            stats2 = buffer2_.stats();
            stats2_ptr = &stats2;
        }
        Element stats_elem = make_stats_bar(stats, cfg_.unit, rate_mode_.load(), stats2_ptr)
                           | ftxui::color(text_col);

        // EOF notice appended to stats row
        if (eof_seen_.load()) {
            stats_elem = hbox({
                stats_elem,
                text(" [input stream closed]") | dim | ftxui::color(Color::Yellow),
            });
        }

        Element plot_row = hbox({
            y_axis_elem | size(WIDTH, EQUAL, 9),
            separator()  | ftxui::color(axes_col),
            chart_elem   | flex,
        });

        return vbox({
            text(title_str) | bold | hcenter | ftxui::color(title_col),
            separator()     | ftxui::color(axes_col),
            plot_row        | flex,
            separator()     | ftxui::color(axes_col),
            stats_elem,
        }) | border;
    };

    auto renderer_comp = ftxui::Renderer(render_fn);

    auto component = CatchEvent(renderer_comp, [&](Event e) -> bool {
        if (e == Event::Character('q') || e == Event::Escape) {
            running_ = false;
            screen.ExitLoopClosure()();
            return true;
        }
        if (e == Event::Character('r')) {
            bool current = rate_mode_.load();
            rate_mode_.store(!current);
            rate_reset_.store(true);
            return true;
        }
        // Ctrl+L: force full redraw
        if (e.input() == "\f") {
            screen.PostEvent(Event::Custom);
            return true;
        }
        return false;
    });

    screen.Loop(component);

    running_ = false;
    if (ticker.joinable()) ticker.join();
}
