use ratatui::{
    Frame,
    buffer::Buffer,
    layout::Rect,
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::{Block, BorderType, Borders},
};

use crate::ui::theme::Theme;

/// Draw a btop-style rounded box with an optional title embedded in the top border.
/// Renders as: ╭─┤ Title ├──────────────────╮
pub fn draw_btop_box(frame: &mut Frame, area: Rect, title: Option<&str>, theme: &Theme) {
    let block = match title {
        Some(t) => {
            let title_line = Line::from(vec![
                Span::styled("─┤ ", Style::default().fg(theme.border)),
                Span::styled(t.to_string(), Style::default().fg(theme.title).bold()),
                Span::styled(" ├", Style::default().fg(theme.border)),
            ]);
            Block::default()
                .borders(Borders::ALL)
                .border_type(BorderType::Rounded)
                .border_style(Style::default().fg(theme.border))
                .title(title_line)
        }
        None => Block::default()
            .borders(Borders::ALL)
            .border_type(BorderType::Rounded)
            .border_style(Style::default().fg(theme.border)),
    };
    frame.render_widget(block, area);
}

/// Draw a btop-style section separator directly into the buffer.
///
/// Produces: ├──────────────────────────────────┤
///       or: ├─┤ Title ├───────────────────────-┤
///
/// `outer` is the full outer box area (including its border).
/// `y` is the absolute row to draw on — must be inside the outer box.
pub fn draw_separator(buf: &mut Buffer, outer: Rect, y: u16, title: Option<&str>, theme: &Theme) {
    if outer.width < 2 {
        return;
    }
    let x0 = outer.x;
    let border_style = Style::default().fg(theme.border);

    // Full ├──────────────────┤ line
    let fill: String = std::iter::once('├')
        .chain(std::iter::repeat('─').take((outer.width - 2) as usize))
        .chain(std::iter::once('┤'))
        .collect();
    buf.set_string(x0, y, &fill, border_style);

    // Overlay title if provided: ├─┤ Title ├──────────────────┤
    if let Some(t) = title {
        // "─┤ " prefix keeps the ├ at x0 visible, then title, then " ├"
        let label = format!("─┤ {} ├", t);
        buf.set_string(x0 + 1, y, &label, Style::default().fg(theme.status_header).bold());
    }
}
