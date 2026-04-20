use ratatui::{
    Frame,
    layout::{Alignment, Constraint, Direction, Layout, Rect},
    style::{Color, Style, Stylize},
    text::Span,
    widgets::{
        canvas::{Canvas, Line as CanvasLine},
        Bar, BarChart, BarGroup, Paragraph,
    },
};

use crate::app::{AppState, StreamSnapshot};
use crate::config::ChartMode;
use crate::ui::layout::{compute_grid, split_yaxis_chart};
use crate::ui::theme::{gradient_color, stream_symbol, Theme};

// ── value formatting ──────────────────────────────────────────────────────────

pub fn fmt_val(value: f64) -> String {
    if value == 0.0 {
        return "0".to_string();
    }
    let abs = value.abs();
    if abs >= 1000.0 || abs < 0.01 {
        // Scientific notation, 3 significant digits
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
            return YRange { min: y_lock_min, max: y_lock_max };
        }

        // Compute min/max from the visible (zoomed/panned) data, not full history
        let mut data_min = f64::INFINITY;
        let mut data_max = f64::NEG_INFINITY;

        for snap in snapshots.iter().filter(|s| s.visible && !s.data.is_empty()) {
            for &v in &snap.data {
                data_min = data_min.min(v);
                data_max = data_max.max(v);
            }
        }

        if !data_min.is_finite() {
            data_min = 0.0;
            data_max = 1.0;
        }

        let y_min = y_min_cfg.unwrap_or_else(|| {
            let span = (data_max - data_min).max(1e-6);
            data_min - span * 0.05
        });
        let y_max = y_max_cfg.unwrap_or_else(|| {
            let span = (data_max - data_min).max(1e-6);
            data_max + span * 0.05
        });

        let y_min = if log_scale && y_min <= 0.0 { 1e-3 } else { y_min };
        let (y_min, y_max) = if (y_max - y_min).abs() < 1e-9 {
            (y_min - 1.0, y_max + 1.0)
        } else {
            (y_min, y_max)
        };

        YRange { min: y_min, max: y_max }
    }
}

// ── Y-axis labels ─────────────────────────────────────────────────────────────

pub fn render_yaxis(frame: &mut Frame, area: Rect, yr: &YRange, log_scale: bool, theme: &Theme) {
    if area.height == 0 {
        return;
    }

    let n_ticks = (area.height as usize / 3).clamp(2, 6);

    for tick in 0..n_ticks {
        // frac=1.0 at top, frac=0.0 at bottom
        let frac = if n_ticks <= 1 { 0.5 } else { tick as f64 / (n_ticks - 1) as f64 };
        let value = if log_scale {
            let lmin = yr.min.max(1e-10).log10();
            let lmax = yr.max.max(1e-10).log10();
            10f64.powf(lmin + frac * (lmax - lmin))
        } else {
            yr.min + frac * (yr.max - yr.min)
        };

        // Position: frac=1 → row 0 (top), frac=0 → row height-1 (bottom)
        let row = ((1.0 - frac) * (area.height.saturating_sub(1)) as f64).round() as u16;
        if row >= area.height {
            continue;
        }

        let label = format!("{:>9}", fmt_val(value));
        let label_area = Rect {
            x: area.x,
            y: area.y + row,
            width: area.width,
            height: 1,
        };
        let para = Paragraph::new(Span::styled(label, Style::default().fg(theme.axis)))
            .alignment(Alignment::Right);
        frame.render_widget(para, label_area);
    }
}

// ── Braille line chart ────────────────────────────────────────────────────────

fn render_line_canvas(
    frame: &mut Frame,
    area: Rect,
    snapshots: &[StreamSnapshot],
    yr: &YRange,
    log_scale: bool,
    theme: &Theme,
) {
    let visible: Vec<&StreamSnapshot> = snapshots.iter().filter(|s| s.visible && !s.data.is_empty()).collect();
    if visible.is_empty() {
        return;
    }

    let y_lo = yr.min;
    let y_hi = yr.max;
    let transform_y = |v: f64| -> f64 {
        if log_scale {
            let lv = v.max(1e-10).log10();
            let lmin = y_lo.max(1e-10).log10();
            let lmax = y_hi.max(1e-10).log10();
            lmin + ((lv - lmin) / (lmax - lmin)) * (y_hi - y_lo) + y_lo
        } else {
            v
        }
    };

    let (y_b, y_t) = if log_scale {
        (y_lo.max(1e-10).log10(), y_hi.max(1e-10).log10())
    } else {
        (y_lo, y_hi)
    };

    let grad_start = theme.grad_start;
    let grad_end = theme.grad_end;

    let canvas = Canvas::default()
        .x_bounds([0.0, 1.0])
        .y_bounds([y_b, y_t])
        .paint(|ctx| {
            // Grid lines at 25%, 50%, 75%
            for frac in [0.25f64, 0.5, 0.75] {
                let gy = y_b + frac * (y_t - y_b);
                ctx.draw(&CanvasLine {
                    x1: 0.0,
                    y1: gy,
                    x2: 1.0,
                    y2: gy,
                    color: theme.axis,
                });
            }

            // Each stream — monochrome gradient, same for all
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
                    let y0 = transform_y(data[i - 1]);
                    let y1 = transform_y(data[i]);
                    ctx.draw(&CanvasLine {
                        x1: t0,
                        y1: y0,
                        x2: t1,
                        y2: y1,
                        color,
                    });
                }
            }
        });

    frame.render_widget(canvas, area);

    // Render stream symbol labels at the right edge of each line
    if area.width > 2 && area.height > 0 {
        let y_range = y_t - y_b;
        for snap in &visible {
            if let Some(&last_val) = snap.data.last() {
                let y_norm = if y_range.abs() < 1e-9 {
                    0.5
                } else {
                    let y_transformed = if log_scale {
                        transform_y(last_val)
                    } else {
                        last_val
                    };
                    (y_transformed - y_b) / y_range
                };
                // Map normalized y (0=bottom, 1=top) to row
                let row = ((1.0 - y_norm) * (area.height.saturating_sub(1)) as f64)
                    .round()
                    .clamp(0.0, (area.height.saturating_sub(1)) as f64) as u16;
                let sym = stream_symbol(snap.color_idx);
                let label = format!("{} {}", sym, &snap.label);
                let label_len = label.len().min(area.width as usize - 1);
                let label_area = Rect {
                    x: area.x + area.width - label_len as u16 - 1,
                    y: area.y + row,
                    width: label_len as u16 + 1,
                    height: 1,
                };
                let color = gradient_color(grad_start, grad_end, 1.0);
                let para = Paragraph::new(Span::styled(
                    label,
                    Style::default().fg(color).bold(),
                ));
                frame.render_widget(para, label_area);
            }
        }
    }
}

// ── Sparkline chart ───────────────────────────────────────────────────────────

fn render_sparklines(
    frame: &mut Frame,
    area: Rect,
    snapshots: &[StreamSnapshot],
    yr: &YRange,
    theme: &Theme,
) {
    let visible: Vec<&StreamSnapshot> = snapshots.iter().filter(|s| s.visible && !s.data.is_empty()).collect();
    if visible.is_empty() {
        return;
    }

    let n = visible.len();
    let row_constraints: Vec<Constraint> = visible.iter().map(|_| Constraint::Ratio(1, n as u32)).collect();
    let rows = Layout::default()
        .direction(Direction::Vertical)
        .constraints(row_constraints)
        .split(area);

    let range = yr.max - yr.min;
    let scale = if range < 1e-9 { 1.0 } else { 1.0 / range };

    for (i, snap) in visible.iter().enumerate() {
        let row_area = rows[i];
        let width = row_area.width as usize;
        let data = &snap.data;
        let data_len = data.len();

        // Show last `width` points
        let start = data_len.saturating_sub(width);
        let shown = &data[start..];
        let num_cols = shown.len();

        // Build one BarGroup per column with gradient
        let groups: Vec<BarGroup> = (0..num_cols)
            .map(|col| {
                let t = if num_cols <= 1 { 1.0 } else { col as f64 / (num_cols - 1) as f64 };
                let color = gradient_color(theme.grad_start, theme.grad_end, t);
                let norm = ((shown[col] - yr.min) * scale).clamp(0.0, 1.0);
                let val = (norm * 100.0) as u64;
                let bar = Bar::default()
                    .value(val)
                    .text_value(String::new())
                    .style(Style::default().fg(color));
                BarGroup::default().bars(&[bar])
            })
            .collect();

        let mut chart = BarChart::default()
            .bar_gap(0)
            .group_gap(0)
            .bar_width(1)
            .max(100);
        for group in &groups {
            chart = chart.data(group.clone());
        }
        frame.render_widget(chart, row_area);

        // Stream symbol label
        let sym = stream_symbol(snap.color_idx);
        let label_color = gradient_color(theme.grad_start, theme.grad_end, 1.0);
        if row_area.width > 4 {
            let lbl = format!("{} {}", sym, &snap.label);
            let lbl_area = Rect {
                x: row_area.x,
                y: row_area.y,
                width: (lbl.len() as u16 + 1).min(row_area.width),
                height: 1,
            };
            let para = Paragraph::new(Span::styled(
                lbl,
                Style::default().fg(label_color).bold(),
            ));
            frame.render_widget(para, lbl_area);
        }
    }
}

// ── Bar chart ─────────────────────────────────────────────────────────────────

/// Per-stream brightness offsets to differentiate bars within a group.
const STREAM_BRIGHTNESS: &[f64] = &[1.0, 0.65, 0.45, 0.80, 0.55, 0.35, 0.90, 0.50, 0.70, 0.40, 0.60, 0.75];

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
    yr: &YRange,
    theme: &Theme,
) {
    let visible: Vec<&StreamSnapshot> = snapshots.iter().filter(|s| s.visible && !s.data.is_empty()).collect();
    if visible.is_empty() {
        return;
    }

    let range = yr.max - yr.min;
    let scale = if range < 1e-9 { 1.0 } else { 1.0 / range };

    // Each group = N bars (one per stream) + 1 gap column
    let group_width = visible.len() + 1; // bars + group gap
    let max_groups = area.width as usize / group_width.max(1);
    let max_groups = max_groups.max(1);

    // Only render groups for data points that actually exist
    let actual_len = visible.iter().map(|s| s.data.len()).max().unwrap_or(0);
    let num_groups = actual_len.min(max_groups);

    let groups: Vec<BarGroup> = (0..num_groups)
        .map(|i| {
            let t = if num_groups <= 1 { 1.0 } else { i as f64 / (num_groups - 1) as f64 };

            let bars: Vec<Bar> = visible
                .iter()
                .enumerate()
                .map(|(si, snap)| {
                    let idx = if snap.data.len() > num_groups {
                        snap.data.len() - num_groups + i
                    } else if i < snap.data.len() {
                        i
                    } else {
                        return Bar::default().value(0).text_value(String::new())
                            .style(Style::default().fg(stream_bar_color(theme, t, si)));
                    };
                    let v = snap.data[idx];
                    let val = ((v - yr.min) * scale * 100.0).clamp(0.0, 100.0) as u64;
                    let color = stream_bar_color(theme, t, si);
                    Bar::default()
                        .value(val)
                        .text_value(String::new())
                        .style(Style::default().fg(color))
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

fn render_split_pane(
    frame: &mut Frame,
    area: Rect,
    snapshots: &[StreamSnapshot],
    state: &AppState,
    theme: &Theme,
) {
    let visible: Vec<&StreamSnapshot> = snapshots.iter().filter(|s| s.visible && !s.data.is_empty()).collect();
    if visible.is_empty() {
        return;
    }

    let panes = compute_grid(area, visible.len());

    for (snap, pane) in visible.iter().zip(panes.iter()) {
        // Per-stream auto Y range
        let yr = YRange {
            min: snap.stats.min - (snap.stats.max - snap.stats.min).abs() * 0.05,
            max: snap.stats.max + (snap.stats.max - snap.stats.min).abs() * 0.05,
        };
        let (yr_min, yr_max) = if (yr.max - yr.min).abs() < 1e-9 {
            (yr.min - 1.0, yr.max + 1.0)
        } else {
            (yr.min, yr.max)
        };
        let yr = YRange { min: yr_min, max: yr_max };

        let (yaxis_area, canvas_area) = split_yaxis_chart(*pane);
        render_yaxis(frame, yaxis_area, &yr, state.config.log_scale, theme);

        let grad_start = theme.grad_start;
        let grad_end = theme.grad_end;
        let data = &snap.data;
        let n = data.len();
        let canvas = Canvas::default()
            .x_bounds([0.0, 1.0])
            .y_bounds([yr.min, yr.max])
            .paint(move |ctx| {
                for frac in [0.25f64, 0.5, 0.75] {
                    let gy = yr.min + frac * (yr.max - yr.min);
                    ctx.draw(&CanvasLine { x1: 0.0, y1: gy, x2: 1.0, y2: gy, color: theme.axis });
                }
                if n >= 2 {
                    for i in 1..n {
                        let t1 = i as f64 / (n - 1) as f64;
                        let t0 = (i - 1) as f64 / (n - 1) as f64;
                        let color = gradient_color(grad_start, grad_end, t1);
                        ctx.draw(&CanvasLine {
                            x1: t0, y1: data[i - 1],
                            x2: t1, y2: data[i],
                            color,
                        });
                    }
                }
            });
        frame.render_widget(canvas, canvas_area);

        // Stream label in top-left corner
        let sym = stream_symbol(snap.color_idx);
        let label_color = gradient_color(grad_start, grad_end, 1.0);
        let label = Paragraph::new(Span::styled(
            format!("{} {}", sym, &snap.label),
            Style::default().fg(label_color).bold(),
        ));
        let label_area = Rect {
            x: canvas_area.x + 1,
            y: canvas_area.y,
            width: canvas_area.width.min(snap.label.len() as u16 + 2),
            height: 1,
        };
        frame.render_widget(label, label_area);
    }
}

// ── public entry point ────────────────────────────────────────────────────────

pub fn render(frame: &mut Frame, area: Rect, state: &AppState) {
    let snapshots = state.take_snapshots();

    match state.mode {
        ChartMode::Split => {
            render_split_pane(frame, area, &snapshots, state, &state.theme);
            return;
        }
        _ => {}
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

    render_yaxis(frame, yaxis_area, &yr, state.config.log_scale, &state.theme);

    match state.mode {
        ChartMode::Line | ChartMode::Overlay => {
            render_line_canvas(frame, canvas_area, &snapshots, &yr, state.config.log_scale, &state.theme);
        }
        ChartMode::Bar => {
            render_bars(frame, canvas_area, &snapshots, &yr, &state.theme);
        }
        ChartMode::Spark => {
            render_sparklines(frame, canvas_area, &snapshots, &yr, &state.theme);
        }
        ChartMode::Split => unreachable!(),
    }
}
