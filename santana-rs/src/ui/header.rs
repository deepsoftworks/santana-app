use ratatui::{
    Frame,
    layout::Rect,
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::Paragraph,
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
    if state.zoom < state.config.history {
        let zoom_label = format!("{}x zoom", state.config.history / state.zoom.max(1));
        spans.push(indicator(&zoom_label, true, theme));
        spans.push(Span::raw(" "));
    }

    // Rate mode
    spans.push(indicator(
        &format!("rate:{}", if state.rate_mode { "on" } else { "off" }),
        state.rate_mode,
        theme,
    ));
    spans.push(Span::raw(" "));

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
    spans.push(Span::styled(
        fps_str,
        Style::default().fg(theme.dim),
    ));

    let line = Line::from(spans);
    let para = Paragraph::new(line);
    frame.render_widget(para, area);
}
