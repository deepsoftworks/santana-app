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

// Default colors for streams 2-10
static const Color kStreamPalette[] = {
    Color::White,
    Color::Cyan,
    Color::Yellow,
    Color::Magenta,
    Color::Red,
    Color::Blue,
    Color::GrayLight,
    Color::GrayDark,
    Color::Default,
};

Renderer::Renderer(Config cfg, int input_fd, std::string first_line, LineFormat fmt,
                   std::vector<std::string> field_selectors, std::string jq_path)
    : cfg_(std::move(cfg))
    , input_fd_(input_fd)
    , source_(input_fd_, std::move(first_line), fmt,
              std::move(field_selectors), std::move(jq_path))
    , rate_mode_(cfg_.rate_mode)
{
    cfg_.num_streams = std::max(1, std::min(10, cfg_.num_streams));
    for (int i = 0; i < cfg_.num_streams; ++i)
        buffers_.push_back(std::make_unique<RingBuffer<double> >(static_cast<size_t>(cfg_.history)));
}

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
    std::vector<RateState> rs(static_cast<size_t>(cfg_.num_streams));
    int line_index = 0;

    auto push_value = [&](double raw, RateState& state, RingBuffer<double>& buf) {
        if (rate_reset_.exchange(false)) {
            state = {};
        }
        if (rate_mode_.load()) {
            auto now = std::chrono::steady_clock::now();
            if (state.has_prev) {
                double dt = std::chrono::duration<double>(now - state.prev_time).count();
                if (dt > 0) buf.push((raw - state.prev_val) / dt);
            }
            state.prev_val  = raw;
            state.prev_time = now;
            state.has_prev  = true;
        } else {
            buf.push(raw);
        }
    };

    while (running_) {
        source_.poll();
        while (source_.ready()) {
            auto row = source_.next_line();
            if (cfg_.explicit_streams) {
                // Legacy round-robin: -n N flag, one value per line cycles through streams
                int idx = line_index % cfg_.num_streams;
                if (!row.empty())
                    push_value(row[0], rs[static_cast<size_t>(idx)],
                               *buffers_[static_cast<size_t>(idx)]);
                ++line_index;
            } else {
                // Auto-detect: distribute each column to its stream
                for (size_t i = 0; i < row.size() && i < buffers_.size(); ++i)
                    push_value(row[i], rs[i], *buffers_[i]);
            }
        }
        if (source_.is_eof()) {
            eof_seen_ = true;
            break;
        }
    }
}

void Renderer::compute_range(double& y_min, double& y_max) const {
    auto st0 = buffers_[0]->stats();
    double data_min = static_cast<double>(st0.min_val);
    double data_max = static_cast<double>(st0.max_val);

    for (size_t i = 1; i < buffers_.size(); ++i) {
        auto st = buffers_[i]->stats();
        data_min = std::min(data_min, static_cast<double>(st.min_val));
        data_max = std::max(data_max, static_cast<double>(st.max_val));
    }

    if (cfg_.auto_min) {
        y_min = data_min;
        double span = data_max - y_min;
        y_min -= span * 0.05;
        if (cfg_.log_scale && y_min <= 0.0) y_min = 1e-3;
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
    Color axes_col  = resolve_elem_color(ec.axes,      Color::Default);
    Color text_col  = resolve_elem_color(ec.text_col,  Color::Default);
    Color title_col = resolve_elem_color(ec.title_col, Color::Default);
    Color max_err_col = resolve_elem_color(ec.max_err, Color::Red);
    Color min_err_col = resolve_elem_color(ec.min_err, Color::Red);

    // Resolve per-stream plot colors
    std::vector<Color> stream_colors;
    stream_colors.push_back(resolve_elem_color(ec.plot, resolve_color(cfg_.color)));
    for (int i = 1; i < cfg_.num_streams; ++i) {
        if (i == 1 && ec.plot2 >= 0)
            stream_colors.push_back(resolve_elem_color(ec.plot2, Color::White));
        else
            stream_colors.push_back(kStreamPalette[(i - 1) % 9]);
    }

    // Ticker thread: fires a Custom event at ~fps rate.
    std::thread ticker([&] {
        auto interval = std::chrono::milliseconds(1000 / std::max(1, cfg_.fps));
        while (running_) {
            std::this_thread::sleep_for(interval);
            if (running_) screen.PostEvent(Event::Custom);
        }
    });
    struct TickerJoiner {
        std::thread& thread;
        std::atomic<bool>& running;
        ~TickerJoiner() {
            running = false;
            if (thread.joinable()) thread.join();
        }
    } ticker_joiner{ticker, running_};

    auto render_fn = [&]() -> Element {
        double y_min = 0.0, y_max = 1.0;
        compute_range(y_min, y_max);

        int term_w = screen.dimx();
        int term_h = screen.dimy();

        int stats_rows = cfg_.num_streams;
        int chart_h = std::max(4, term_h - 5 - stats_rows);
        int chart_w = std::max(10, term_w - 12);

        std::string title_str = cfg_.title;
        if (!cfg_.unit.empty())       title_str += "  [" + cfg_.unit + "]";
        if (rate_mode_.load())        title_str += "  (rate)";
        if (cfg_.no_scroll)           title_str += "  [FIXED]";
        if (history_view_.load())     title_str += "  [zoom]";

        // Snapshot all streams
        std::vector<std::vector<double> > all_data(static_cast<size_t>(cfg_.num_streams));
        std::vector<RingBuffer<double>::Stats> all_stats(static_cast<size_t>(cfg_.num_streams));
        for (int i = 0; i < cfg_.num_streams; ++i) {
            all_data[static_cast<size_t>(i)]  = buffers_[static_cast<size_t>(i)]->snapshot();
            all_stats[static_cast<size_t>(i)] = buffers_[static_cast<size_t>(i)]->stats();
        }

        // history_view: zoom into the last 20 data points
        if (history_view_.load()) {
            constexpr size_t kZoomWindow = 20;
            for (auto& d : all_data) {
                if (d.size() > kZoomWindow)
                    d.erase(d.begin(), d.end() - static_cast<std::ptrdiff_t>(kZoomWindow));
            }
        }

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
        opts.log_scale     = cfg_.log_scale;

        for (int i = 1; i < cfg_.num_streams; ++i) {
            opts.extra_data.push_back(&all_data[static_cast<size_t>(i)]);
            opts.extra_colors.push_back(stream_colors[static_cast<size_t>(i)]);
        }

        Element chart_elem;
        if (cfg_.mode == ChartMode::Spark) {
            chart_elem = vbox({
                filler(),
                make_sparkline(all_data[0], y_min, y_max, stream_colors[0], opts),
                filler(),
            });
        } else if (cfg_.mode == ChartMode::Bar) {
            chart_elem = make_bar_chart(all_data[0], y_min, y_max, stream_colors[0], chart_w, chart_h, opts);
        } else {
            chart_elem = make_line_chart(all_data[0], y_min, y_max, stream_colors[0], chart_w, chart_h, opts);
        }

        Element y_axis_elem = make_y_axis(y_min, y_max, chart_h, cfg_.log_scale) | ftxui::color(axes_col);

        Element stats_elem = make_stats_bar(all_stats, cfg_.unit, rate_mode_.load(), cfg_.stream_labels)
                           | ftxui::color(text_col);

        // EOF notice appended to stats row
        if (eof_seen_.load()) {
            stats_elem = hbox({
                stats_elem,
                text(" [input stream closed]") | dim | ftxui::color(Color::Yellow),
            });
        }

        const bool has_legend = cfg_.num_streams > 1 && !cfg_.stream_labels.empty();

        Element plot_row = hbox({
            y_axis_elem | size(WIDTH, EQUAL, 9),
            separator()  | ftxui::color(axes_col),
            chart_elem   | flex,
            has_legend ? hbox({
                separator() | ftxui::color(axes_col),
                make_legend(cfg_.stream_labels, stream_colors),
            }) : filler() | size(WIDTH, EQUAL, 0),
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
        if (e == Event::Character('h')) {
            history_view_.store(!history_view_.load());
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
}
