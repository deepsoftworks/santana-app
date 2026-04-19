use ratatui::layout::{Constraint, Direction, Layout, Rect};

use crate::app::AppState;

pub struct LayoutAreas {
    pub header: Rect,
    pub chart:  Rect,
    pub status: Rect,
    pub help:   Rect,
}

/// Compute layout areas given the inner area (inside the outer border box).
pub fn compute(inner: Rect, state: &AppState) -> LayoutAreas {
    let visible_streams = state
        .streams
        .slots
        .iter()
        .filter(|s| !s.buffer.is_empty())
        .count();

    let status_rows = if visible_streams == 0 {
        1
    } else {
        visible_streams.min(8)
    };

    // status section: 2 rows for header + separator + N stream rows
    let status_h = (2 + status_rows) as u16;
    let help_h = 1u16;
    let header_h = 1u16;

    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(header_h),
            Constraint::Min(4),
            Constraint::Length(status_h),
            Constraint::Length(help_h),
        ])
        .split(inner);

    LayoutAreas {
        header: chunks[0],
        chart:  chunks[1],
        status: chunks[2],
        help:   chunks[3],
    }
}

/// Compute a 2-column grid of chart panes for split-pane mode.
pub fn compute_grid(area: Rect, n: usize) -> Vec<Rect> {
    if n == 0 {
        return vec![];
    }
    if n <= 2 {
        // Single column
        let rows = Layout::default()
            .direction(Direction::Vertical)
            .constraints(vec![Constraint::Ratio(1, n as u32); n])
            .split(area);
        return rows.to_vec();
    }

    // Two columns
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

/// Y-axis area (9 chars) + separator (1 char) + chart fill area.
pub fn split_yaxis_chart(chart_area: Rect) -> (Rect, Rect) {
    let chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Length(9), Constraint::Min(1)])
        .split(chart_area);
    (chunks[0], chunks[1])
}
