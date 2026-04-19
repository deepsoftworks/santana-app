use ratatui::{
    Frame,
    layout::{Alignment, Constraint, Direction, Layout, Rect},
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::{
        canvas::{Canvas, Line as CanvasLine},
        Bar, BarChart, BarGroup, Paragraph, Sparkline,
    },
};

use crate::app::{AppState, StreamSnapshot};
use crate::config::ChartMode;
use crate::ui::layout::{compute_grid, split_yaxis_chart};
use crate::ui::theme::{gradient_color, Theme};

// ── value formatting ──────────────────────────────────────────────────────────

pub fn fmt_val(value: f64) -> String {
    if value.abs() >= 1000.0 {
        format!("{:.0}", value)
    } else if value.abs() >= 100.0 {
        format!("{:.1}", value)
    } else {
        format!("{:.2}", value)
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
    let n_ticks = (area.height as usize).min(5).max(2);
    let mut lines = Vec::with_capacity(n_ticks * 2);

    for i in (0..n_ticks).rev() {
        let frac = i as f64 / (n_ticks - 1).max(1) as f64;
        let value = if log_scale {
            let lmin = yr.min.max(1e-10).log10();
            let lmax = yr.max.max(1e-10).log10();
            10f64.powf(lmin + frac * (lmax - lmin))
        } else {
            yr.min + frac * (yr.max - yr.min)
        };

        let label = format!("{:>8}", fmt_val(value));
        lines.push(Line::from(Span::styled(label, Style::default().fg(theme.axis))));
        if i > 0 {
            lines.push(Line::from("")); // spacer
        }
    }

    let para = Paragraph::new(lines).alignment(Alignment::Right);
    frame.render_widget(para, area);
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

            // Each stream
            for snap in &visible {
                let data = &snap.data;
                let n = data.len();
                if n < 2 {
                    continue;
                }
                let sc = &theme.streams[snap.color_idx % theme.streams.len()];
                for i in 1..n {
                    let t0 = (i - 1) as f64 / (n - 1) as f64;
                    let t1 = i as f64 / (n - 1) as f64;
                    let color = gradient_color(sc.grad_start, sc.grad_end, t1);
                    let y0 = transform_y(data[i - 1]);
                    let y1 = transform_y(data[i]);
                    ctx.draw(&CanvasLine {
                        x1: t0,
                        y1: if log_scale { y0 } else { y0 },
                        x2: t1,
                        y2: if log_scale { y1 } else { y1 },
                        color,
                    });
                }
            }
        });

    frame.render_widget(canvas, area);
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
        let sc = &theme.streams[snap.color_idx % theme.streams.len()];
        let spark_data: Vec<u64> = snap
            .data
            .iter()
            .map(|&v| {
                let norm = ((v - yr.min) * scale).clamp(0.0, 1.0);
                (norm * 100.0) as u64
            })
            .collect();

        let spark = Sparkline::default()
            .data(&spark_data)
            .max(100)
            .style(Style::default().fg(sc.bullet));
        frame.render_widget(spark, rows[i]);
    }
}

// ── Bar chart ─────────────────────────────────────────────────────────────────

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

    // Show only the last `area.width` samples across all streams
    let max_bars = area.width as usize / visible.len().max(1);
    let max_bars = max_bars.max(1);

    let groups: Vec<BarGroup> = (0..max_bars)
        .map(|i| {
            let bars: Vec<Bar> = visible
                .iter()
                .map(|snap| {
                    let idx = if snap.data.len() > max_bars {
                        snap.data.len() - max_bars + i
                    } else {
                        i.min(snap.data.len().saturating_sub(1))
                    };
                    let v = snap.data.get(idx).cloned().unwrap_or(0.0);
                    let val = ((v - yr.min) * scale * 100.0).clamp(0.0, 100.0) as u64;
                    let sc = &theme.streams[snap.color_idx % theme.streams.len()];
                    Bar::default().value(val).style(Style::default().fg(sc.bullet))
                })
                .collect();
            BarGroup::default().bars(&bars)
        })
        .collect();

    let mut chart = BarChart::default().bar_gap(0).max(100);
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

        let sc = &theme.streams[snap.color_idx % theme.streams.len()];
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
                        let color = gradient_color(sc.grad_start, sc.grad_end, t1);
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
        let label = Paragraph::new(Span::styled(
            snap.label.clone(),
            Style::default().fg(sc.bullet).bold(),
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
