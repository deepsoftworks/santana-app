use ratatui::{
    Frame,
    layout::{Constraint, Rect},
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::{Cell, Row, Table},
};

use crate::app::AppState;
use crate::ui::charts::fmt_metric;
use crate::ui::widgets::draw_section_title;

pub fn render(frame: &mut Frame, area: Rect, state: &AppState) {
    if area.height == 0 {
        return;
    }

    let streams = &state.streams;
    let visible_slots: Vec<_> = streams
        .slots
        .iter()
        .enumerate()
        .filter(|(_, s)| !s.buffer.is_empty())
        .collect();

    let total = visible_slots.len();
    let header_title = format!("streams {}  active", total);

    // Draw section header in first row
    if area.height >= 1 {
        let title_area = Rect { height: 1, ..area };
        draw_section_title(frame, title_area, &header_title, &state.theme);
    }

    if area.height < 2 {
        return;
    }

    // Table area below header
    let table_area = Rect {
        y: area.y + 1,
        height: area.height - 1,
        ..area
    };

    if visible_slots.is_empty() {
        let waiting = Line::from(Span::styled(
            "  waiting for data…",
            Style::default().fg(state.theme.dim),
        ));
        let para = ratatui::widgets::Paragraph::new(waiting);
        frame.render_widget(para, table_area);
        return;
    }

    let max_rows = table_area.height as usize;
    let shown: Vec<_> = visible_slots.iter().take(max_rows).collect();
    let hidden = total.saturating_sub(max_rows);

    let rows: Vec<Row> = shown
        .iter()
        .map(|(slot_idx, slot)| {
            let sc = &state.theme.streams[slot.color_idx % state.theme.streams.len()];
            let is_selected = *slot_idx == state.selected_idx;
            let bullet = if slot.visible { "● " } else { "○ " };
            let row_style = if is_selected {
                Style::default().bg(state.theme.selected_bg)
            } else {
                Style::default()
            };

            let stats = slot.buffer.stats();
            let unit = &state.config.unit;
            let rate = state.rate_mode;

            let label_style = if slot.visible {
                Style::default().fg(sc.bullet).bold()
            } else {
                Style::default().fg(state.theme.dim)
            };

            Row::new(vec![
                Cell::from(Span::styled(
                    format!("{}{}", bullet, &slot.label),
                    label_style,
                )),
                Cell::from(Span::styled(
                    format!("now: {}", fmt_metric(stats.current, unit, rate)),
                    Style::default().fg(state.theme.title),
                )),
                Cell::from(Span::styled(
                    format!("min: {}", fmt_metric(stats.min, unit, rate)),
                    Style::default().fg(state.theme.dim),
                )),
                Cell::from(Span::styled(
                    format!("max: {}", fmt_metric(stats.max, unit, rate)),
                    Style::default().fg(state.theme.dim),
                )),
                Cell::from(Span::styled(
                    format!("avg: {}", fmt_metric(stats.mean, unit, rate)),
                    Style::default().fg(state.theme.dim),
                )),
                Cell::from(Span::styled(
                    format!("n={}", stats.count),
                    Style::default().fg(state.theme.dim),
                )),
            ])
            .style(row_style)
        })
        .collect();

    let mut rows = rows;

    if hidden > 0 {
        rows.push(Row::new(vec![Cell::from(Span::styled(
            format!("  +{} more streams", hidden),
            Style::default().fg(state.theme.dim),
        ))]));
    }

    if state.eof {
        rows.push(Row::new(vec![Cell::from(Span::styled(
            "  input closed",
            Style::default().fg(state.theme.eof_color),
        ))]));
    }

    let table = Table::new(
        rows,
        [
            Constraint::Min(16),
            Constraint::Min(14),
            Constraint::Min(14),
            Constraint::Min(14),
            Constraint::Min(14),
            Constraint::Min(8),
        ],
    );

    frame.render_widget(table, table_area);
}
