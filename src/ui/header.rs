use ratatui::{
    layout::Rect,
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::Paragraph,
    Frame,
};

use crate::app::AppState;
use crate::ui::theme::Theme;

fn indicator(label: &str, active: bool, theme: &Theme) -> Span<'static> {
    let text = format!("[{}]", label);
    if active {
        Span::styled(text, Style::default().fg(theme.status_header).bold())
    } else {
        Span::styled(text, Style::default().fg(theme.dim))
    }
}

pub fn render(frame: &mut Frame, area: Rect, state: &AppState) {
    let theme = &state.theme;
    let mut spans = Vec::new();

    // Mode
    spans.push(indicator(state.mode.label(), true, theme));
    spans.push(Span::raw(" "));

    // Zoom
    let base_window = state
        .streams
        .slots
        .iter()
        .map(|slot| slot.buffer.len())
        .max()
        .unwrap_or(state.config.history)
        .max(1);
    if state.zoom < base_window {
        let zoom_factor = base_window as f64 / state.zoom.max(1) as f64;
        let zoom_label = if (zoom_factor - zoom_factor.round()).abs() < 0.05 {
            format!("{}x zoom", zoom_factor.round() as usize)
        } else {
            format!("{zoom_factor:.1}x zoom")
        };
        spans.push(indicator(&zoom_label, true, theme));
        spans.push(Span::raw(" "));
    }

    // Rate mode
    if state.rate_mode {
        spans.push(indicator("rate:on", true, theme));
        spans.push(Span::raw(" "));
    }

    // Log scale
    if state.config.log_scale {
        spans.push(indicator("log", true, theme));
        spans.push(Span::raw(" "));
    }

    // Paused
    if state.paused {
        spans.push(Span::styled(
            "[PAUSED]",
            Style::default().fg(theme.eof_color).bold(),
        ));
        spans.push(Span::raw(" "));
    }

    // Idle (no data for 2+ seconds)
    if state.idle && !state.eof && !state.paused {
        spans.push(Span::styled(
            "[WAITING]",
            Style::default().fg(theme.eof_color).bold(),
        ));
        spans.push(Span::raw(" "));
    }

    // EOF
    if state.eof {
        spans.push(Span::styled(
            "[EOF]",
            Style::default().fg(theme.eof_color).bold(),
        ));
        spans.push(Span::raw(" "));
    }

    // Y-locked
    if state.y_locked {
        spans.push(indicator("y-locked", true, theme));
        spans.push(Span::raw(" "));
    }

    // Unit
    if !state.config.unit.is_empty() {
        spans.push(Span::styled(
            format!("[{}]", state.config.unit),
            Style::default().fg(theme.dim),
        ));
        spans.push(Span::raw(" "));
    }

    // FPS (right-aligned via filler)
    let fps_str = format!("{:.0}fps", state.fps_display);
    // We'll just append it at the end (right-justified requires split layout, keep simple)
    spans.push(Span::styled(fps_str, Style::default().fg(theme.dim)));

    let line = Line::from(spans);
    let para = Paragraph::new(line);
    frame.render_widget(para, area);
}
