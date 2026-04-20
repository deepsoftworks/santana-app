use ratatui::layout::{Constraint, Direction, Layout, Rect};

use crate::app::AppState;

pub struct LayoutAreas {
    pub header: Rect,
    pub chart:  Rect,
    pub sep1_y: u16, // absolute y of the separator before the status panel
    pub status: Rect,
    pub sep2_y: u16, // absolute y of the separator before the help bar
    pub help:   Rect,
}

/// Compute layout areas given the inner area (inside the outer border box).
pub fn compute(inner: Rect, _outer: Rect, state: &AppState) -> LayoutAreas {
    let visible_streams = state
        .streams
        .slots
        .iter()
        .filter(|s| !s.buffer.is_empty())
        .count();

    let status_rows = if visible_streams == 0 { 1 } else { visible_streams.min(8) };
    // 1 row per stream; EOF and overflow each add 1 extra
    let extra = if state.eof { 1 } else { 0 }
        + if visible_streams > 8 { 1 } else { 0 };
    let status_h = (status_rows + extra) as u16;

    // Layout: header | chart | sep1 | status | sep2 | help
    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),           // header
            Constraint::Min(4),              // chart
            Constraint::Length(1),           // sep1 row
            Constraint::Length(status_h),    // status rows
            Constraint::Length(1),           // sep2 row
            Constraint::Length(1),           // help
        ])
        .split(inner);

    LayoutAreas {
        header: chunks[0],
        chart:  chunks[1],
        sep1_y: chunks[2].y,
        status: chunks[3],
        sep2_y: chunks[4].y,
        help:   chunks[5],
    }
}

/// Compute a 2-column grid of chart panes for split-pane mode.
pub fn compute_grid(area: Rect, n: usize) -> Vec<Rect> {
    if n == 0 {
        return vec![];
    }
    if n <= 2 {
        let rows = Layout::default()
            .direction(Direction::Vertical)
            .constraints(vec![Constraint::Ratio(1, n as u32); n])
            .split(area);
        return rows.to_vec();
    }

    let cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Ratio(1, 2), Constraint::Ratio(1, 2)])
        .split(area);

    let left_n = (n + 1) / 2;
    let right_n = n / 2;
    let mut panes = Vec::with_capacity(n);

    let left_rows = Layout::default()
        .direction(Direction::Vertical)
        .constraints(vec![Constraint::Ratio(1, left_n as u32); left_n])
        .split(cols[0]);
    panes.extend_from_slice(&left_rows);

    if right_n > 0 {
        let right_rows = Layout::default()
            .direction(Direction::Vertical)
            .constraints(vec![Constraint::Ratio(1, right_n as u32); right_n])
            .split(cols[1]);
        panes.extend_from_slice(&right_rows[..right_n]);
    }

    panes
}

/// Split an area into a narrow Y-axis column (9 chars) and the chart canvas.
pub fn split_yaxis_chart(chart_area: Rect) -> (Rect, Rect) {
    let chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Length(9), Constraint::Min(1)])
        .split(chart_area);
    (chunks[0], chunks[1])
}
