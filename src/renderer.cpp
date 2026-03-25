#include "renderer.hpp"
#include "chart.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <chrono>
#include <functional>

using namespace ftxui;

Renderer::Renderer(Config cfg)
    : cfg_(std::move(cfg))
    , buffer_(static_cast<size_t>(cfg_.history))
{}

Renderer::~Renderer() {
    running_ = false;
    if (reader_.joinable()) reader_.join();
}

void Renderer::reader_thread() {
    while (running_) {
        source_.poll();
        while (source_.ready()) {
            buffer_.push(source_.next());
        }
    }
}

void Renderer::compute_range(double& y_min, double& y_max) const {
    auto st = buffer_.stats();
    if (cfg_.auto_min) {
        y_min = static_cast<double>(st.min_val);
        double span = static_cast<double>(st.max_val) - y_min;
        y_min -= span * 0.05;
    } else {
        y_min = cfg_.y_min;
    }
    if (cfg_.auto_max) {
        y_max = static_cast<double>(st.max_val);
        double span = y_max - static_cast<double>(st.min_val);
        y_max += span * 0.05;
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

    auto screen       = ScreenInteractive::Fullscreen();
    Color chart_color = resolve_color(cfg_.color);

    // Ticker thread: fires a Custom event at ~fps rate
    std::thread ticker([&] {
        auto interval = std::chrono::milliseconds(1000 / std::max(1, cfg_.fps));
        while (running_) {
            std::this_thread::sleep_for(interval);
            if (running_) screen.PostEvent(Event::Custom);
        }
    });

    // Build FTXUI Renderer component
    auto render_fn = [&]() -> Element {
        auto data  = buffer_.snapshot();
        auto stats = buffer_.stats();

        double y_min = 0.0, y_max = 1.0;
        compute_range(y_min, y_max);

        int term_w = screen.dimx();
        int term_h = screen.dimy();

        // Subtract border (2), title (1), separator (1), stats (1), separator (1) = 6
        int chart_h = std::max(4, term_h - 6);
        // Subtract border (2), y-axis (9), separator (1) = 12
        int chart_w = std::max(10, term_w - 12);

        std::string title_str = cfg_.title;
        if (!cfg_.unit.empty()) title_str += "  [" + cfg_.unit + "]";

        Element chart_elem;
        if (cfg_.mode == ChartMode::Spark) {
            chart_elem = vbox({
                filler(),
                make_sparkline(data, y_min, y_max, chart_color),
                filler(),
            });
        } else if (cfg_.mode == ChartMode::Bar) {
            chart_elem = make_bar_chart(data, y_min, y_max, chart_color, chart_w, chart_h);
        } else {
            chart_elem = make_line_chart(data, y_min, y_max, chart_color, chart_w, chart_h);
        }

        Element y_axis_elem = make_y_axis(y_min, y_max, chart_h);
        Element stats_elem  = make_stats_bar(stats, cfg_.unit);

        Element plot_row = hbox({
            y_axis_elem | size(WIDTH, EQUAL, 9),
            separator(),
            chart_elem  | flex,
        });

        return vbox({
            text(title_str) | bold | hcenter,
            separator(),
            plot_row | flex,
            separator(),
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
        return false;
    });

    screen.Loop(component);

    running_ = false;
    if (ticker.joinable()) ticker.join();
}
