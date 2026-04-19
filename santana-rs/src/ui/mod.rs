pub mod charts;
pub mod header;
pub mod help;
pub mod layout;
pub mod status;
pub mod theme;
pub mod widgets;

use ratatui::{layout::Margin, Frame};

use crate::app::AppState;
use widgets::draw_btop_box;

pub fn render(frame: &mut Frame, state: &AppState) {
    let area = frame.area();

    // Outer btop-style rounded box with title
    draw_btop_box(frame, area, Some(&state.config.title), &state.theme);

    // Inner area (inside the border)
    let inner = area.inner(Margin { vertical: 1, horizontal: 1 });

    // Compute layout areas
    let areas = layout::compute(inner, state);

    // Render each section
    header::render(frame, areas.header, state);
    charts::render(frame, areas.chart, state);
    status::render(frame, areas.status, state);
    help::render(frame, areas.help, &state.theme);

    // Help overlay on top of everything
    if state.show_help {
        help::render_overlay(frame, area, &state.theme);
    }
}
