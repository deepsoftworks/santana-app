use ratatui::{
    Frame,
    layout::{Constraint, Rect},
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::{Cell, Paragraph, Row, Table},
};

use crate::app::AppState;
use crate::ui::charts::fmt_metric;
use crate::ui::theme::stream_symbol;

pub fn render(frame: &mut Frame, area: Rect, state: &AppState) {
    if area.height == 0 {
        return;
    }

    let visible_slots: Vec<_> = state
        .streams
        .slots
        .iter()
        .enumerate()
        .filter(|(_, s)| !s.buffer.is_empty())
        .collect();

    if visible_slots.is_empty() {
        let para = Paragraph::new(Line::from(Span::styled(
            "  waiting for data…",
            Style::default().fg(state.theme.dim),
        )));
        frame.render_widget(para, area);
        return;
    }

    let max_rows = area.height as usize;
    let shown: Vec<_> = visible_slots.iter().take(max_rows).collect();
    let hidden = visible_slots.len().saturating_sub(max_rows);

    let mut rows: Vec<Row> = shown
        .iter()
        .map(|(slot_idx, slot)| {
            let is_selected = *slot_idx == state.selected_idx;
            let sym = stream_symbol(slot.color_idx);
            let bullet = if slot.visible {
                format!("{} ", sym)
            } else {
                format!("({}) ", sym)
            };
            let row_style = if is_selected {
                Style::default().bg(state.theme.selected_bg)
            } else {
                Style::default()
            };
            let label_style = if slot.visible {
                Style::default().fg(state.theme.title).bold()
            } else {
                Style::default().fg(state.theme.dim)
            };

            let stats = slot.buffer.stats();
            let unit = &state.config.unit;
            let rate = state.rate_mode;

            Row::new(vec![
                Cell::from(Span::styled(format!("{}{}", bullet, &slot.label), label_style)),
                Cell::from(Span::styled(
                    format!("now {:>10}", fmt_metric(stats.current, unit, rate)),
                    Style::default().fg(state.theme.title),
                )),
                Cell::from(Span::styled(
                    format!("min {:>10}", fmt_metric(stats.min, unit, rate)),
                    Style::default().fg(state.theme.dim),
                )),
                Cell::from(Span::styled(
                    format!("max {:>10}", fmt_metric(stats.max, unit, rate)),
                    Style::default().fg(state.theme.dim),
                )),
                Cell::from(Span::styled(
                    format!("avg {:>10}", fmt_metric(stats.mean, unit, rate)),
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
            Constraint::Min(15),
            Constraint::Min(15),
            Constraint::Min(15),
            Constraint::Min(15),
            Constraint::Min(8),
        ],
    );
    frame.render_widget(table, area);
}
