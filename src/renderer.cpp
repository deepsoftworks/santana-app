#include "renderer.hpp"
#include "chart.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

using namespace ftxui;

namespace {

static const Color kStreamPalette[] = {
    Color::Green,
    Color::Cyan,
    Color::Yellow,
    Color::Magenta,
    Color::Red,
    Color::Blue,
    Color::White,
    Color::GrayLight,
    Color::Green,
    Color::Cyan,
    Color::Yellow,
    Color::Magenta,
};

const Color kAxesColor = Color::GrayLight;
const Color kTitleColor = Color::White;

}  // namespace

Renderer::Renderer(Config cfg, int input_fd, std::string first_line)
    : cfg_(std::move(cfg))
    , input_fd_(input_fd)
    , source_(input_fd_, std::move(first_line))
    , rate_mode_(cfg_.rate_mode)
{}

Renderer::~Renderer() {
    running_ = false;
    if (reader_.joinable()) reader_.join();
}

size_t Renderer::ensure_stream_locked(const std::string& label) {
    const std::string key = label.empty() ? "value" : label;
    if (const auto it = stream_index_.find(key); it != stream_index_.end()) {
        return it->second;
    }

    const size_t index = streams_.size();
    streams_.push_back(StreamSlot{
        .label = key,
        .buffer = std::make_unique<RingBuffer<double>>(static_cast<size_t>(cfg_.history)),
    });
    stream_index_[key] = index;
    return index;
}

void Renderer::reader_thread() {
    while (running_) {
        source_.poll();

        if (rate_reset_.exchange(false)) {
            std::scoped_lock lock(streams_mutex_);
            for (auto& stream : streams_) {
                stream.has_prev = false;
            }
        }

        while (source_.ready()) {
            auto row = source_.next_line();
            if (row.empty()) continue;

            const auto now = std::chrono::steady_clock::now();
            for (const auto& field : row.fields) {
                std::scoped_lock lock(streams_mutex_);
                auto& stream = streams_[ensure_stream_locked(field.key)];
                if (rate_mode_.load()) {
                    if (stream.has_prev) {
                        const double dt = std::chrono::duration<double>(now - stream.prev_time).count();
                        if (dt > 0.0) {
                            stream.buffer->push((field.value - stream.prev_val) / dt);
                        }
                    }
                } else {
                    stream.buffer->push(field.value);
                }
                stream.prev_val = field.value;
                stream.prev_time = now;
                stream.has_prev = true;
            }
        }

        if (source_.is_eof()) {
            eof_seen_ = true;
            break;
        }
    }
}

void Renderer::run() {
    reader_ = std::thread(&Renderer::reader_thread, this);

    auto screen = ScreenInteractive::Fullscreen();

    std::thread ticker([&] {
        const auto interval = std::chrono::milliseconds(1000 / std::max(1, cfg_.fps));
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
        std::string title_str = cfg_.title;
        if (!cfg_.unit.empty()) title_str += "  [" + cfg_.unit + "]";
        if (rate_mode_.load()) title_str += "  [rate]";
        if (cfg_.log_scale) title_str += "  [log]";

        std::vector<std::string> labels;
        std::vector<std::vector<double>> all_data;
        std::vector<RingBuffer<double>::Stats> all_stats;
        {
            std::scoped_lock lock(streams_mutex_);
            labels.reserve(streams_.size());
            all_data.reserve(streams_.size());
            all_stats.reserve(streams_.size());
            for (const auto& stream : streams_) {
                if (stream.buffer->size() == 0) continue;
                labels.push_back(stream.label);
                all_data.push_back(stream.buffer->snapshot());
                all_stats.push_back(stream.buffer->stats());
            }
        }

        std::vector<Color> stream_colors;
        stream_colors.reserve(all_data.size());
        for (size_t i = 0; i < all_data.size(); ++i) {
            stream_colors.push_back(kStreamPalette[i % (sizeof(kStreamPalette) / sizeof(kStreamPalette[0]))]);
        }

        double y_min = cfg_.auto_min ? 0.0 : cfg_.y_min;
        double y_max = cfg_.auto_max ? 1.0 : cfg_.y_max;
        bool has_range_data = false;
        double data_min = 0.0;
        double data_max = 0.0;
        for (const auto& stats : all_stats) {
            if (stats.count == 0) continue;
            if (!has_range_data) {
                data_min = static_cast<double>(stats.min_val);
                data_max = static_cast<double>(stats.max_val);
                has_range_data = true;
            } else {
                data_min = std::min(data_min, static_cast<double>(stats.min_val));
                data_max = std::max(data_max, static_cast<double>(stats.max_val));
            }
        }
        if (cfg_.auto_min) y_min = has_range_data ? data_min : 0.0;
        if (cfg_.auto_max) y_max = has_range_data ? data_max : 1.0;
        if (cfg_.auto_min && has_range_data) {
            const double span = std::max(1e-6, data_max - data_min);
            y_min -= span * 0.05;
        }
        if (cfg_.auto_max && has_range_data) {
            const double span = std::max(1e-6, data_max - data_min);
            y_max += span * 0.05;
        }
        if (cfg_.log_scale && y_min <= 0.0) y_min = 1e-3;
        if (std::abs(y_max - y_min) < 1e-9) {
            y_min -= 1.0;
            y_max += 1.0;
        }

        const int term_w = screen.dimx();
        const int term_h = screen.dimy();
        const int max_status_rows = std::max(1, std::min(8, term_h / 4));
        const size_t visible_status_rows = all_stats.empty()
            ? 1
            : std::min(all_stats.size(), static_cast<size_t>(max_status_rows));
        const int status_panel_height = 2 + 2 + static_cast<int>(visible_status_rows)
            + ((all_stats.size() > visible_status_rows) ? 1 : 0)
            + (eof_seen_.load() ? 1 : 0);
        const int chart_h = std::max(6, term_h - status_panel_height - 5);
        const int chart_w = std::max(10, term_w - 12);

        ChartOptions opts;
        opts.log_scale = cfg_.log_scale;
        for (size_t i = 1; i < all_data.size(); ++i) {
            opts.extra_data.push_back(&all_data[i]);
            opts.extra_colors.push_back(stream_colors[i]);
        }

        Element chart_elem;
        if (cfg_.mode == ChartMode::Spark) {
            chart_elem = vbox({
                filler(),
                make_sparkline(all_data.empty() ? std::vector<double>{} : all_data[0],
                               y_min, y_max,
                               stream_colors.empty() ? Color::Green : stream_colors[0],
                               opts),
                filler(),
            });
        } else if (cfg_.mode == ChartMode::Bar) {
            chart_elem = make_bar_chart(all_data.empty() ? std::vector<double>{} : all_data[0],
                                        y_min, y_max,
                                        stream_colors.empty() ? Color::Green : stream_colors[0],
                                        chart_w, chart_h, opts);
        } else {
            chart_elem = make_line_chart(all_data.empty() ? std::vector<double>{} : all_data[0],
                                         y_min, y_max,
                                         stream_colors.empty() ? Color::Green : stream_colors[0],
                                         chart_w, chart_h, opts);
        }

        const Element y_axis_elem = make_y_axis(y_min, y_max, chart_h, cfg_.log_scale) | ftxui::color(kAxesColor);
        const Element status_elem = make_status_panel(
            all_stats,
            labels,
            stream_colors,
            cfg_.unit,
            rate_mode_.load(),
            visible_status_rows,
            eof_seen_.load());

        Element plot_row = hbox({
            y_axis_elem | size(WIDTH, EQUAL, 9),
            separator() | ftxui::color(kAxesColor),
            chart_elem | flex,
        });

        return vbox({
            text(title_str) | bold | hcenter | ftxui::color(kTitleColor),
            separator() | ftxui::color(kAxesColor),
            plot_row | flex,
            status_elem,
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
            const bool current = rate_mode_.load();
            rate_mode_.store(!current);
            rate_reset_.store(true);
            return true;
        }
        if (e.input() == "\f") {
            screen.PostEvent(Event::Custom);
            return true;
        }
        return false;
    });

    screen.Loop(component);

    running_ = false;
}
