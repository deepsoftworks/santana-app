pub mod charts;
pub mod help;
pub mod layout;
pub mod status;
pub mod theme;
pub mod widgets;

use ratatui::{layout::Margin, Frame};

use crate::app::AppState;
use widgets::{draw_bottom_right_hint, draw_btop_box, draw_separator};

pub fn render(frame: &mut Frame, state: &AppState) {
    let area = frame.area();

    // Outer btop-style rounded box with title
    draw_btop_box(frame, area, Some(&state.config.title), &state.theme);

    // Inner area (inside the border)
    let inner = area.inner(Margin { vertical: 1, horizontal: 1 });

    // Compute layout
    let areas = layout::compute(inner, area, state);

    // Content
    charts::render(frame, areas.chart, state);

    // ├─┤ streams N active ├────────────┤
    let n_active = state.streams.slots.iter().filter(|s| !s.buffer.is_empty()).count();
    let sep1_title = format!("streams {}  active", n_active);
    draw_separator(frame.buffer_mut(), area, areas.sep1_y, Some(&sep1_title), &state.theme);

    status::render(frame, areas.status, state);

    draw_bottom_right_hint(frame, area, "?:help", &state.theme);

    // Help overlay on top of everything
    if state.show_help {
        help::render_overlay(frame, area, &state.theme);
    }
}
