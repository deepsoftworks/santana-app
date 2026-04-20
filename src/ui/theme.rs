use ratatui::style::Color;
use crate::config::ThemeName;

/// Symbols used to differentiate streams (instead of colors).
pub const STREAM_SYMBOLS: &[char] = &['●', '■', '▲', '◆', '★', '▼', '◇', '○', '□', '△', '◈', '⬟'];

#[derive(Debug, Clone)]
pub struct Theme {
    pub border:        Color,
    pub title:         Color,
    pub axis:          Color,
    pub separator:     Color,
    pub status_header: Color,
    pub help_key:      Color,
    pub help_desc:     Color,
    pub selected_bg:   Color,
    pub eof_color:     Color,
    pub dim:           Color,
    pub grad_start:    (u8, u8, u8), // monochrome gradient: dim end
    pub grad_end:      (u8, u8, u8), // monochrome gradient: bright end
}

pub fn gradient_color(start: (u8, u8, u8), end: (u8, u8, u8), t: f64) -> Color {
    let t = t.clamp(0.0, 1.0);
    let r = (start.0 as f64 + t * (end.0 as f64 - start.0 as f64)).round() as u8;
    let g = (start.1 as f64 + t * (end.1 as f64 - start.1 as f64)).round() as u8;
    let b = (start.2 as f64 + t * (end.2 as f64 - start.2 as f64)).round() as u8;
    Color::Rgb(r, g, b)
}

/// Returns the symbol for a given stream index.
pub fn stream_symbol(idx: usize) -> char {
    STREAM_SYMBOLS[idx % STREAM_SYMBOLS.len()]
}

// ── Dark theme ────────────────────────────────────────────────────────────────

fn dark_theme() -> Theme {
    Theme {
        border:        Color::Rgb(0x55, 0x6d, 0x59),
        title:         Color::Rgb(0xcc, 0xcc, 0xcc),
        axis:          Color::Rgb(0x50, 0x50, 0x50),
        separator:     Color::Rgb(0x44, 0x44, 0x44),
        status_header: Color::Rgb(0xcc, 0xcc, 0xcc),
        help_key:      Color::Rgb(0xcb, 0xc0, 0x6c),
        help_desc:     Color::Rgb(0x88, 0x88, 0x88),
        selected_bg:   Color::Rgb(0x26, 0x32, 0x26),
        eof_color:     Color::Rgb(0xcb, 0xc0, 0x6c),
        dim:           Color::Rgb(0x55, 0x55, 0x55),
        grad_start:    (0x30, 0x30, 0x30),
        grad_end:      (0xdd, 0xdd, 0xdd),
    }
}

// ── Light theme ───────────────────────────────────────────────────────────────

fn light_theme() -> Theme {
    Theme {
        border:        Color::Rgb(0x70, 0x90, 0x78),
        title:         Color::Rgb(0x22, 0x22, 0x22),
        axis:          Color::Rgb(0xaa, 0xaa, 0xaa),
        separator:     Color::Rgb(0xcc, 0xcc, 0xcc),
        status_header: Color::Rgb(0x22, 0x22, 0x22),
        help_key:      Color::Rgb(0x80, 0x60, 0x00),
        help_desc:     Color::Rgb(0x55, 0x55, 0x55),
        selected_bg:   Color::Rgb(0xd8, 0xf0, 0xd8),
        eof_color:     Color::Rgb(0x88, 0x50, 0x00),
        dim:           Color::Rgb(0xaa, 0xaa, 0xaa),
        grad_start:    (0xbb, 0xbb, 0xbb),
        grad_end:      (0x22, 0x22, 0x22),
    }
}

// ── Solarized dark ────────────────────────────────────────────────────────────

fn solarized_theme() -> Theme {
    Theme {
        border:        Color::Rgb(0x26, 0x52, 0x6a),
        title:         Color::Rgb(0x93, 0xa1, 0xa1),
        axis:          Color::Rgb(0x28, 0x43, 0x52),
        separator:     Color::Rgb(0x20, 0x38, 0x48),
        status_header: Color::Rgb(0x93, 0xa1, 0xa1),
        help_key:      Color::Rgb(0xb5, 0x89, 0x00),
        help_desc:     Color::Rgb(0x65, 0x7b, 0x83),
        selected_bg:   Color::Rgb(0x07, 0x36, 0x42),
        eof_color:     Color::Rgb(0xcb, 0x4b, 0x16),
        dim:           Color::Rgb(0x35, 0x4a, 0x55),
        grad_start:    (0x15, 0x2a, 0x30),
        grad_end:      (0x93, 0xa1, 0xa1),
    }
}

// ── Nord ──────────────────────────────────────────────────────────────────────

fn nord_theme() -> Theme {
    Theme {
        border:        Color::Rgb(0x3b, 0x52, 0x5e),
        title:         Color::Rgb(0xd8, 0xde, 0xe9),
        axis:          Color::Rgb(0x34, 0x44, 0x50),
        separator:     Color::Rgb(0x2e, 0x38, 0x44),
        status_header: Color::Rgb(0xd8, 0xde, 0xe9),
        help_key:      Color::Rgb(0xeb, 0xcb, 0x8b),
        help_desc:     Color::Rgb(0x60, 0x70, 0x80),
        selected_bg:   Color::Rgb(0x22, 0x30, 0x3c),
        eof_color:     Color::Rgb(0xd0, 0x87, 0x70),
        dim:           Color::Rgb(0x40, 0x50, 0x60),
        grad_start:    (0x2e, 0x3a, 0x46),
        grad_end:      (0xd8, 0xde, 0xe9),
    }
}

// ── factory ───────────────────────────────────────────────────────────────────

pub fn make_theme(name: ThemeName) -> Theme {
    match name {
        ThemeName::Dark      => dark_theme(),
        ThemeName::Light     => light_theme(),
        ThemeName::Solarized => solarized_theme(),
        ThemeName::Nord      => nord_theme(),
    }
}
