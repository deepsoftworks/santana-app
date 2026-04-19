use ratatui::{
    Frame,
    layout::Rect,
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::{Block, BorderType, Borders},
};

use crate::ui::theme::Theme;

/// Draw a btop-style rounded box with an optional title embedded in the top border.
/// Title renders as: ╭─┤ Title ├─╮
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

/// Draw a section divider block (separator with title) — used inside the outer box.
pub fn draw_section_title(frame: &mut Frame, area: Rect, title: &str, theme: &Theme) {
    let title_line = Line::from(vec![
        Span::styled("─┤ ", Style::default().fg(theme.separator)),
        Span::styled(title.to_string(), Style::default().fg(theme.status_header).bold()),
        Span::styled(" ├", Style::default().fg(theme.separator)),
    ]);
    let block = Block::default()
        .borders(Borders::TOP)
        .border_style(Style::default().fg(theme.separator))
        .title(title_line);
    frame.render_widget(block, area);
}
