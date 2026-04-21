use ratatui::{
    layout::{Alignment, Margin, Rect},
    style::{Color, Style, Stylize},
    text::{Line, Span},
    widgets::{
        canvas::{Canvas, Line as CanvasLine},
        Bar, BarChart, BarGroup, Block, Borders, Paragraph,
    },
    Frame,
};

use crate::app::{AppState, StreamSnapshot};
use crate::config::ChartMode;
use crate::ui::layout::{compute_grid, split_yaxis_chart};
use crate::ui::theme::{gradient_color, stream_marker, stream_marker_color, Theme};

// ── value formatting ──────────────────────────────────────────────────────────

pub fn fmt_val(value: f64) -> String {
    if value == 0.0 {
        return "0".to_string();
    }
    let abs = value.abs();
    if abs >= 1000.0 || abs < 0.01 {
        format!("{:.2e}", value)
    } else if abs >= 100.0 {
        format!("{:.1}", value)
    } else if abs >= 10.0 {
        format!("{:.2}", value)
    } else {
        format!("{:.3}", value)
    }
}

pub fn fmt_metric(value: f64, unit: &str, rate_mode: bool) -> String {
    let mut s = fmt_val(value);
    if !unit.is_empty() {
        s.push(' ');
        s.push_str(unit);
    }
    if rate_mode {
        s.push_str("/s");
    }
    s
}

// ── Y-range helpers ───────────────────────────────────────────────────────────

pub struct YRange {
    pub min: f64,
    pub max: f64,
}

struct AxisTick {
    value: f64,
    label: String,
}

struct AxisTicks {
    plot_min: f64,
    plot_max: f64,
    ticks: Vec<AxisTick>,
}

impl YRange {
    pub fn compute(
        snapshots: &[StreamSnapshot],
        y_min_cfg: Option<f64>,
        y_max_cfg: Option<f64>,
        log_scale: bool,
        y_locked: bool,
        y_lock_min: f64,
        y_lock_max: f64,
    ) -> Self {
        if y_locked {
            return YRange {
                min: y_lock_min,
                max: y_lock_max,
            };
        }

        if log_scale {
            let mut data_min = f64::INFINITY;
            let mut data_max = f64::NEG_INFINITY;

            for snap in snapshots.iter().filter(|s| s.visible && !s.data.is_empty()) {
                data_max = data_max.max(snap.stats.max);
                for &value in &snap.data {
                    if value > 0.0 {
                        data_min = data_min.min(value);
                    }
                }
            }

            if !data_min.is_finite() || data_max <= 0.0 {
                return YRange {
                    min: 1e-3,
                    max: 1.0,
                };
            }

            let y_min = y_min_cfg.unwrap_or(data_min);
            let y_max = y_max_cfg.unwrap_or(data_max);
            let y_min = y_min.max(1e-10);
            let y_max = y_max.max(y_min * 10.0);
            return YRange {
                min: y_min,
                max: y_max,
            };
        }

        let mut data_min = f64::INFINITY;
        let mut data_max = f64::NEG_INFINITY;

        for snap in snapshots.iter().filter(|s| s.visible && !s.data.is_empty()) {
            data_min = data_min.min(snap.stats.min);
            data_max = data_max.max(snap.stats.max);
        }

        if !data_min.is_finite() {
            data_min = 0.0;
            data_max = 1.0;
        }

        let y_min = y_min_cfg.unwrap_or(data_min);
        let y_max = y_max_cfg.unwrap_or(data_max);
        let (y_min, y_max) = if (y_max - y_min).abs() < 1e-9 {
            (y_min - 1.0, y_max + 1.0)
        } else {
            (y_min, y_max)
        };

        YRange {
            min: y_min,
            max: y_max,
        }
    }
}

fn nice_num(range: f64, round: bool) -> f64 {
    let range = range.abs().max(1e-12);
    let exponent = range.log10().floor();
    let fraction = range / 10f64.powf(exponent);
    let nice_fraction = if round {
        if fraction < 1.5 {
            1.0
        } else if fraction < 3.0 {
            2.0
        } else if fraction < 7.0 {
            5.0
        } else {
            10.0
        }
    } else if fraction <= 1.0 {
        1.0
    } else if fraction <= 2.0 {
        2.0
    } else if fraction <= 5.0 {
        5.0
    } else {
        10.0
    };
    nice_fraction * 10f64.powf(exponent)
}

fn project_y(value: f64, log_scale: bool) -> f64 {
    if log_scale {
        value.max(1e-10).log10()
    } else {
        value
    }
}

impl AxisTicks {
    fn compute(yr: &YRange, target_ticks: usize, log_scale: bool) -> Self {
        if log_scale {
            Self::logarithmic(yr, target_ticks)
        } else {
            Self::linear(yr, target_ticks)
        }
    }

    fn linear(yr: &YRange, target_ticks: usize) -> Self {
        let target_ticks = target_ticks.max(2);
        let plot_min = yr.min;
        let plot_max = yr.max;
        let mut step = nice_num(
            (plot_max - plot_min) / (target_ticks.saturating_sub(1).max(1) as f64),
            true,
        );
        let mut ticks = Vec::new();

        for _ in 0..8 {
            ticks.clear();

            let epsilon = step.abs() * 1e-6;
            let mut value = (plot_min / step).ceil() * step;
            if value - plot_min > step {
                value -= step;
            }

            while value <= plot_max + epsilon {
                if value < plot_min - epsilon {
                    value += step;
                    continue;
                }
                let shown = if value.abs() < epsilon { 0.0 } else { value };
                ticks.push(AxisTick {
                    value: shown,
                    label: fmt_val(shown),
                });
                value += step;
            }

            if ticks.len() <= target_ticks + 1 {
                break;
            }

            step = nice_num(step * 1.5, true);
        }

        if ticks.len() < 2 {
            ticks = vec![
                AxisTick {
                    value: plot_min,
                    label: fmt_val(plot_min),
                },
                AxisTick {
                    value: plot_max,
                    label: fmt_val(plot_max),
                },
            ];
        }

        Self {
            plot_min,
            plot_max,
            ticks,
        }
    }

    fn logarithmic(yr: &YRange, target_ticks: usize) -> Self {
        let target_ticks = target_ticks.max(2);
        let min = yr.min.max(1e-10);
        let max = yr.max.max(min * 10.0);
        let start_exp = min.log10().floor() as i32;
        let end_exp = max.log10().ceil() as i32;
        let total_ticks = (end_exp - start_exp + 1).max(2) as usize;
        let stride = (total_ticks as f64 / target_ticks as f64).ceil().max(1.0) as i32;

        let mut ticks = Vec::new();
        let mut exp = start_exp;
        while exp <= end_exp {
            let value = 10f64.powi(exp);
            ticks.push(AxisTick {
                value,
                label: fmt_val(value),
            });
            exp += stride;
        }

        let plot_max = 10f64.powi(end_exp);
        if ticks.last().map(|tick| tick.value).unwrap_or(0.0) < plot_max {
            ticks.push(AxisTick {
                value: plot_max,
                label: fmt_val(plot_max),
            });
        }

        Self {
            plot_min: 10f64.powi(start_exp),
            plot_max,
            ticks,
        }
    }
}

// ── Y-axis labels ─────────────────────────────────────────────────────────────

fn axis_row(area: Rect, axis_ticks: &AxisTicks, value: f64, log_scale: bool) -> u16 {
    if area.height <= 1 {
        return 0;
    }

    let min = project_y(axis_ticks.plot_min, log_scale);
    let max = project_y(axis_ticks.plot_max, log_scale);
    let span = (max - min).abs().max(1e-9);
    let frac = ((project_y(value, log_scale) - min) / span).clamp(0.0, 1.0);

    ((1.0 - frac) * area.height.saturating_sub(1) as f64)
        .round()
        .clamp(0.0, area.height.saturating_sub(1) as f64) as u16
}

fn render_yaxis(
    frame: &mut Frame,
    area: Rect,
    axis_ticks: &AxisTicks,
    log_scale: bool,
    theme: &Theme,
) {
    if area.height == 0 {
        return;
    }

    let mut used_rows = Vec::new();
    for tick in &axis_ticks.ticks {
        let row = axis_row(area, axis_ticks, tick.value, log_scale);
        if used_rows.contains(&row) {
            continue;
        }
        used_rows.push(row);

        let label_area = Rect {
            x: area.x,
            y: area.y + row,
            width: area.width,
            height: 1,
        };
        let para = Paragraph::new(Span::styled(
            format!("{:>width$}", tick.label, width = area.width as usize),
            Style::default().fg(theme.axis),
        ))
        .alignment(Alignment::Right);
        frame.render_widget(para, label_area);
    }
}

fn render_plot_box(frame: &mut Frame, area: Rect, theme: &Theme) -> Rect {
    if area.width < 2 || area.height < 2 {
        return area;
    }

    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(theme.axis));
    let inner = block.inner(area);
    frame.render_widget(block, area);
    inner.inner(Margin {
        vertical: 0,
        horizontal: 0,
    })
}

// ── Line chart ────────────────────────────────────────────────────────────────

fn render_line_canvas(
    frame: &mut Frame,
    area: Rect,
    snapshots: &[StreamSnapshot],
    axis_ticks: &AxisTicks,
    log_scale: bool,
    theme: &Theme,
) {
    let visible: Vec<&StreamSnapshot> = snapshots
        .iter()
        .filter(|s| s.visible && !s.data.is_empty())
        .collect();
    if visible.is_empty() || area.width == 0 || area.height == 0 {
        return;
    }

    let y_b = project_y(axis_ticks.plot_min, log_scale);
    let y_t = project_y(axis_ticks.plot_max, log_scale);
    let grad_start = theme.grad_start;
    let grad_end = theme.grad_end;

    let canvas = Canvas::default()
        .x_bounds([0.0, 1.0])
        .y_bounds([y_b, y_t])
        .paint(|ctx| {
            for snap in &visible {
                let data = &snap.data;
                let n = data.len();
                if n < 2 {
                    continue;
                }
                for i in 1..n {
                    let t0 = (i - 1) as f64 / (n - 1) as f64;
                    let t1 = i as f64 / (n - 1) as f64;
                    let color = gradient_color(grad_start, grad_end, t1);
                    ctx.draw(&CanvasLine {
                        x1: t0,
                        y1: project_y(data[i - 1], log_scale),
                        x2: t1,
                        y2: project_y(data[i], log_scale),
                        color,
                    });
                }
            }
        });

    frame.render_widget(canvas, area);

    if area.width > 2 {
        let y_range = (y_t - y_b).abs().max(1e-9);
        for snap in &visible {
            if let Some(&last_val) = snap.data.last() {
                let y_norm = ((project_y(last_val, log_scale) - y_b) / y_range).clamp(0.0, 1.0);
                let row = ((1.0 - y_norm) * area.height.saturating_sub(1) as f64)
                    .round()
                    .clamp(0.0, area.height.saturating_sub(1) as f64)
                    as u16;
                let label = snap.label.clone();
                let label_len = (label.len() + 2).min(area.width as usize - 1);
                let label_area = Rect {
                    x: area.x + area.width - label_len as u16 - 1,
                    y: area.y + row,
                    width: label_len as u16 + 1,
                    height: 1,
                };
                let para = Paragraph::new(Line::from(vec![
                    Span::styled(
                        format!("{} ", stream_marker()),
                        Style::default()
                            .fg(stream_marker_color(theme, snap.color_idx))
                            .bold(),
                    ),
                    Span::styled(
                        label,
                        Style::default()
                            .fg(gradient_color(grad_start, grad_end, 1.0))
                            .bold(),
                    ),
                ]));
                frame.render_widget(para, label_area);
            }
        }
    }
}

// ── Bar chart ─────────────────────────────────────────────────────────────────

/// Per-stream brightness offsets to differentiate bars within a group.
const STREAM_BRIGHTNESS: &[f64] = &[
    1.0, 0.65, 0.45, 0.80, 0.55, 0.35, 0.90, 0.50, 0.70, 0.40, 0.60, 0.75,
];

fn stream_bar_color(theme: &Theme, time_t: f64, stream_idx: usize) -> Color {
    let base = gradient_color(theme.grad_start, theme.grad_end, time_t);
    let brightness = STREAM_BRIGHTNESS[stream_idx % STREAM_BRIGHTNESS.len()];
    if let Color::Rgb(r, g, b) = base {
        Color::Rgb(
            (r as f64 * brightness) as u8,
            (g as f64 * brightness) as u8,
            (b as f64 * brightness) as u8,
        )
    } else {
        base
    }
}

fn render_bars(
    frame: &mut Frame,
    area: Rect,
    snapshots: &[StreamSnapshot],
    axis_ticks: &AxisTicks,
    log_scale: bool,
    theme: &Theme,
) {
    let visible: Vec<&StreamSnapshot> = snapshots
        .iter()
        .filter(|s| s.visible && !s.data.is_empty())
        .collect();
    if visible.is_empty() || area.width == 0 || area.height == 0 {
        return;
    }

    let min = project_y(axis_ticks.plot_min, log_scale);
    let max = project_y(axis_ticks.plot_max, log_scale);
    let scale = 1.0 / (max - min).abs().max(1e-9);

    let group_width = visible.len() + 1;
    let max_groups = (area.width as usize / group_width.max(1)).max(1);
    let actual_len = visible.iter().map(|s| s.data.len()).max().unwrap_or(0);
    let num_groups = actual_len.min(max_groups);

    let groups: Vec<BarGroup> = (0..num_groups)
        .map(|i| {
            let t = if num_groups <= 1 {
                1.0
            } else {
                i as f64 / (num_groups - 1) as f64
            };

            let bars: Vec<Bar> = visible
                .iter()
                .enumerate()
                .map(|(si, snap)| {
                    let idx = if snap.data.len() > num_groups {
                        snap.data.len() - num_groups + i
                    } else if i < snap.data.len() {
                        i
                    } else {
                        return Bar::default()
                            .value(0)
                            .text_value(String::new())
                            .style(Style::default().fg(stream_bar_color(theme, t, si)));
                    };

                    let value = snap.data[idx];
                    let projected = project_y(value, log_scale);
                    let val = ((projected - min) * scale * 100.0).clamp(0.0, 100.0) as u64;
                    Bar::default()
                        .value(val)
                        .text_value(String::new())
                        .style(Style::default().fg(stream_bar_color(theme, t, si)))
                })
                .collect();

            BarGroup::default().bars(&bars)
        })
        .collect();

    let mut chart = BarChart::default().bar_gap(0).group_gap(1).max(100);
    for group in &groups {
        chart = chart.data(group.clone());
    }
    frame.render_widget(chart, area);
}

// ── Split-pane ────────────────────────────────────────────────────────────────

fn split_range(snap: &StreamSnapshot, log_scale: bool) -> YRange {
    if log_scale {
        let mut min_positive = f64::INFINITY;
        for &value in &snap.data {
            if value > 0.0 {
                min_positive = min_positive.min(value);
            }
        }
        if !min_positive.is_finite() || snap.stats.max <= 0.0 {
            return YRange {
                min: 1e-3,
                max: 1.0,
            };
        }

        return YRange {
            min: min_positive,
            max: snap.stats.max.max(min_positive * 10.0),
        };
    }

    let min = snap.stats.min;
    let max = snap.stats.max;
    if (max - min).abs() < 1e-9 {
        YRange {
            min: min - 1.0,
            max: max + 1.0,
        }
    } else {
        YRange { min, max }
    }
}

fn render_split_pane(
    frame: &mut Frame,
    area: Rect,
    snapshots: &[StreamSnapshot],
    state: &AppState,
    theme: &Theme,
) {
    let visible: Vec<&StreamSnapshot> = snapshots
        .iter()
        .filter(|s| s.visible && !s.data.is_empty())
        .collect();
    if visible.is_empty() {
        return;
    }

    let panes = compute_grid(area, visible.len());

    for (snap, pane) in visible.iter().zip(panes.iter()) {
        let (yaxis_area, canvas_area) = split_yaxis_chart(*pane);
        let yr = split_range(snap, state.config.log_scale);
        let axis_ticks = AxisTicks::compute(
            &yr,
            (canvas_area.height as usize / 3).clamp(2, 6),
            state.config.log_scale,
        );
        render_yaxis(
            frame,
            yaxis_area,
            &axis_ticks,
            state.config.log_scale,
            theme,
        );

        let plot_area = render_plot_box(frame, canvas_area, theme);
        if plot_area.width == 0 || plot_area.height == 0 {
            continue;
        }

        let grad_start = theme.grad_start;
        let grad_end = theme.grad_end;
        let data = &snap.data;
        let n = data.len();
        let y_b = project_y(axis_ticks.plot_min, state.config.log_scale);
        let y_t = project_y(axis_ticks.plot_max, state.config.log_scale);

        let canvas = Canvas::default()
            .x_bounds([0.0, 1.0])
            .y_bounds([y_b, y_t])
            .paint(move |ctx| {
                if n >= 2 {
                    for i in 1..n {
                        let t0 = (i - 1) as f64 / (n - 1) as f64;
                        let t1 = i as f64 / (n - 1) as f64;
                        ctx.draw(&CanvasLine {
                            x1: t0,
                            y1: project_y(data[i - 1], state.config.log_scale),
                            x2: t1,
                            y2: project_y(data[i], state.config.log_scale),
                            color: gradient_color(grad_start, grad_end, t1),
                        });
                    }
                }
            });
        frame.render_widget(canvas, plot_area);

        let label = Paragraph::new(Line::from(vec![
            Span::styled(
                format!("{} ", stream_marker()),
                Style::default()
                    .fg(stream_marker_color(theme, snap.color_idx))
                    .bold(),
            ),
            Span::styled(
                snap.label.clone(),
                Style::default()
                    .fg(gradient_color(grad_start, grad_end, 1.0))
                    .bold(),
            ),
        ]));
        let label_area = Rect {
            x: plot_area.x + 1.min(plot_area.width.saturating_sub(1)),
            y: plot_area.y,
            width: plot_area
                .width
                .saturating_sub(1)
                .min((snap.label.len() + 3) as u16),
            height: 1,
        };
        frame.render_widget(label, label_area);
    }
}

// ── public entry point ────────────────────────────────────────────────────────

pub fn render(frame: &mut Frame, area: Rect, state: &AppState) {
    let snapshots = state.take_snapshots();

    if state.mode == ChartMode::Split {
        render_split_pane(frame, area, &snapshots, state, &state.theme);
        return;
    }

    let (yaxis_area, canvas_area) = split_yaxis_chart(area);
    let yr = YRange::compute(
        &snapshots,
        state.config.y_min,
        state.config.y_max,
        state.config.log_scale,
        state.y_locked,
        state.y_lock_min,
        state.y_lock_max,
    );
    let axis_ticks = AxisTicks::compute(
        &yr,
        (canvas_area.height as usize / 3).clamp(2, 6),
        state.config.log_scale,
    );

    render_yaxis(
        frame,
        yaxis_area,
        &axis_ticks,
        state.config.log_scale,
        &state.theme,
    );
    let plot_area = render_plot_box(frame, canvas_area, &state.theme);

    match state.mode {
        ChartMode::Line => {
            render_line_canvas(
                frame,
                plot_area,
                &snapshots,
                &axis_ticks,
                state.config.log_scale,
                &state.theme,
            );
        }
        ChartMode::Bar => {
            render_bars(
                frame,
                plot_area,
                &snapshots,
                &axis_ticks,
                state.config.log_scale,
                &state.theme,
            );
        }
        ChartMode::Split => unreachable!(),
    }
}
