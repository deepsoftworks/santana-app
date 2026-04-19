use ratatui::{
    Frame,
    layout::{Constraint, Rect},
    style::{Style, Stylize},
    text::{Line, Span},
    widgets::{Block, BorderType, Borders, Cell, Clear, Paragraph, Row, Table},
};

use crate::ui::theme::Theme;

const KEYBINDINGS: &[(&str, &str)] = &[
    ("q / Esc", "Quit"),
    ("m", "Cycle chart mode (line→bar→spark→overlay→split)"),
    ("r", "Toggle rate mode (delta/s)"),
    ("Space", "Pause / resume data ingestion"),
    ("+  /  =", "Zoom in"),
    ("-", "Zoom out"),
    (",  /  <", "Pan left (older data)"),
    (".  /  >", "Pan right (newer data)"),
    ("↑ / ↓", "Select stream"),
    ("t", "Toggle selected stream visibility"),
    ("y", "Lock / unlock Y-axis scale"),
    ("?", "Toggle this help overlay"),
    ("Ctrl+L", "Force redraw"),
];

pub fn render(frame: &mut Frame, area: Rect, theme: &Theme) {
    let hints = [
        ("q", "quit"),
        ("m", "mode"),
        ("r", "rate"),
        ("Space", "pause"),
        ("+/-", "zoom"),
        (",/.", "pan"),
        ("↑↓", "select"),
        ("t", "toggle"),
        ("y", "y-lock"),
        ("?", "help"),
    ];

    let spans: Vec<Span> = hints
        .iter()
        .flat_map(|(k, v)| {
            vec![
                Span::styled(k.to_string(), Style::default().fg(theme.help_key).bold()),
                Span::styled(format!(":{} ", v), Style::default().fg(theme.help_desc)),
            ]
        })
        .collect();

    let line = Line::from(spans);
    let para = Paragraph::new(line);
    frame.render_widget(para, area);
}

pub fn render_overlay(frame: &mut Frame, area: Rect, theme: &Theme) {
    let width = 62u16.min(area.width.saturating_sub(4));
    let height = (KEYBINDINGS.len() as u16 + 4).min(area.height.saturating_sub(4));

    let x = area.x + (area.width.saturating_sub(width)) / 2;
    let y = area.y + (area.height.saturating_sub(height)) / 2;
    let popup_area = Rect { x, y, width, height };

    frame.render_widget(Clear, popup_area);

    let title = Line::from(vec![
        Span::styled("─┤ ", Style::default().fg(theme.border)),
        Span::styled(" Keyboard Shortcuts ", Style::default().fg(theme.title).bold()),
        Span::styled(" ├", Style::default().fg(theme.border)),
    ]);

    let block = Block::default()
        .borders(Borders::ALL)
        .border_type(BorderType::Rounded)
        .border_style(Style::default().fg(theme.border))
        .title(title);

    let inner = block.inner(popup_area);
    frame.render_widget(block, popup_area);

    let rows: Vec<Row> = KEYBINDINGS
        .iter()
        .map(|(k, v)| {
            Row::new(vec![
                Cell::from(Span::styled(
                    format!("  {:<12}", k),
                    Style::default().fg(theme.help_key).bold(),
                )),
                Cell::from(Span::styled(v.to_string(), Style::default().fg(theme.help_desc))),
            ])
        })
        .collect();

    let table = Table::new(
        rows,
        [Constraint::Length(16), Constraint::Min(10)],
    );
    frame.render_widget(table, inner);
}
