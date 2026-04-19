use std::collections::HashMap;
use std::io;
use std::time::{Duration, Instant};

use anyhow::Result;
use crossbeam_channel::{Receiver, TryRecvError};
use crossterm::{
    event::{
        self, DisableMouseCapture, EnableMouseCapture, Event, KeyCode, KeyEvent, KeyModifiers,
        MouseButton, MouseEventKind,
    },
    execute,
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
};
use ratatui::{backend::CrosstermBackend, Terminal};

use crate::buffer::{RingBuffer, Stats};
use crate::config::{ChartMode, Config};
use crate::parser::ParsedRow;
use crate::source::spawn_reader;
use crate::ui::theme::{make_theme, Theme};

// ── Stream bank ───────────────────────────────────────────────────────────────

pub struct StreamSlot {
    pub label:     String,
    pub buffer:    RingBuffer,
    pub color_idx: usize,
    pub visible:   bool,
    pub prev_val:  Option<f64>,
    pub prev_time: Option<Instant>,
}

pub struct StreamBank {
    pub slots: Vec<StreamSlot>,
    index:     HashMap<String, usize>,
}

impl StreamBank {
    fn new() -> Self {
        Self { slots: Vec::new(), index: HashMap::new() }
    }

    pub fn ensure_slot(&mut self, label: &str, history: usize) -> usize {
        let key = if label.is_empty() { "value" } else { label };
        if let Some(&idx) = self.index.get(key) {
            return idx;
        }
        let idx = self.slots.len();
        self.slots.push(StreamSlot {
            label:     key.to_string(),
            buffer:    RingBuffer::new(history),
            color_idx: idx,
            visible:   true,
            prev_val:  None,
            prev_time: None,
        });
        self.index.insert(key.to_string(), idx);
        idx
    }
}

// ── Snapshot for rendering ────────────────────────────────────────────────────

pub struct StreamSnapshot {
    pub label:     String,
    pub data:      Vec<f64>,
    pub stats:     Stats,
    pub color_idx: usize,
    pub visible:   bool,
}

// ── AppState ──────────────────────────────────────────────────────────────────

pub struct AppState {
    pub config:       Config,
    pub streams:      StreamBank,
    pub paused:       bool,
    pub selected_idx: usize,
    pub zoom:         usize, // number of samples to show
    pub pan:          usize, // offset from right edge
    pub y_locked:     bool,
    pub y_lock_min:   f64,
    pub y_lock_max:   f64,
    pub mode:         ChartMode,
    pub rate_mode:    bool,
    pub show_help:    bool,
    pub eof:          bool,
    pub fps_display:  f64,
    pub last_render:  Instant,
    pub theme:        Theme,
    pub rx:           Receiver<Option<ParsedRow>>,
}

impl AppState {
    fn new(config: Config, rx: Receiver<Option<ParsedRow>>) -> Self {
        let zoom = config.history;
        let rate_mode = config.rate_mode;
        let mode = config.mode;
        let theme = make_theme(config.theme);
        Self {
            streams: StreamBank::new(),
            paused: false,
            selected_idx: 0,
            zoom,
            pan: 0,
            y_locked: false,
            y_lock_min: 0.0,
            y_lock_max: 1.0,
            mode,
            rate_mode,
            show_help: false,
            eof: false,
            fps_display: 0.0,
            last_render: Instant::now(),
            theme,
            config,
            rx,
        }
    }

    /// Take a snapshot of all stream data for rendering (zero-copy snapshot via Vec clone).
    pub fn take_snapshots(&self) -> Vec<StreamSnapshot> {
        self.streams
            .slots
            .iter()
            .map(|slot| {
                let data = slot.buffer.snapshot_with_zoom(self.zoom, self.pan);
                let stats = slot.buffer.stats();
                StreamSnapshot {
                    label:     slot.label.clone(),
                    data,
                    stats,
                    color_idx: slot.color_idx,
                    visible:   slot.visible,
                }
            })
            .collect()
    }
}

// ── Channel drain ─────────────────────────────────────────────────────────────

fn drain_channel(state: &mut AppState) {
    if state.eof {
        return;
    }
    let now = Instant::now();
    let history = state.config.history;

    for _ in 0..10_000 {
        match state.rx.try_recv() {
            Ok(Some(row)) => {
                for field in row.fields {
                    let idx = state.streams.ensure_slot(&field.key, history);
                    let slot = &mut state.streams.slots[idx];
                    if state.rate_mode {
                        if let (Some(prev), Some(prev_t)) = (slot.prev_val, slot.prev_time) {
                            let dt = now.duration_since(prev_t).as_secs_f64();
                            if dt > 0.0 {
                                slot.buffer.push((field.value - prev) / dt);
                            }
                        }
                    } else {
                        slot.buffer.push(field.value);
                    }
                    slot.prev_val = Some(field.value);
                    slot.prev_time = Some(now);
                }
            }
            Ok(None) => {
                state.eof = true;
                break;
            }
            Err(TryRecvError::Empty) => break,
            Err(TryRecvError::Disconnected) => {
                state.eof = true;
                break;
            }
        }
    }
}

// ── Key handler ───────────────────────────────────────────────────────────────

/// Returns true if the app should quit.
fn handle_key(state: &mut AppState, key: KeyEvent) -> bool {
    match key.code {
        KeyCode::Char('q') | KeyCode::Esc => return true,
        KeyCode::Char('?') => state.show_help = !state.show_help,
        KeyCode::Char('m') => state.mode = state.mode.cycle(),
        KeyCode::Char('r') => {
            state.rate_mode = !state.rate_mode;
            // Reset rate state for all streams
            for slot in &mut state.streams.slots {
                slot.prev_val = None;
                slot.prev_time = None;
            }
        }
        KeyCode::Char(' ') => state.paused = !state.paused,
        KeyCode::Char('+') | KeyCode::Char('=') => {
            state.zoom = (state.zoom / 2).max(10);
        }
        KeyCode::Char('-') => {
            state.zoom = (state.zoom * 2).min(state.config.history);
        }
        KeyCode::Char(',') | KeyCode::Char('<') => {
            let max_pan = state
                .streams
                .slots
                .iter()
                .map(|s| s.buffer.len().saturating_sub(state.zoom))
                .max()
                .unwrap_or(0);
            state.pan = (state.pan + state.zoom / 4).min(max_pan);
        }
        KeyCode::Char('.') | KeyCode::Char('>') => {
            state.pan = state.pan.saturating_sub(state.zoom / 4);
        }
        KeyCode::Up => {
            state.selected_idx = state.selected_idx.saturating_sub(1);
        }
        KeyCode::Down => {
            let n = state.streams.slots.len();
            if n > 0 {
                state.selected_idx = (state.selected_idx + 1).min(n - 1);
            }
        }
        KeyCode::Char('t') => {
            if let Some(slot) = state.streams.slots.get_mut(state.selected_idx) {
                slot.visible = !slot.visible;
            }
        }
        KeyCode::Char('y') => {
            if state.y_locked {
                state.y_locked = false;
            } else {
                // Compute current auto range and lock it
                let mut lo = f64::INFINITY;
                let mut hi = f64::NEG_INFINITY;
                for slot in &state.streams.slots {
                    if slot.visible && !slot.buffer.is_empty() {
                        let s = slot.buffer.stats();
                        lo = lo.min(s.min);
                        hi = hi.max(s.max);
                    }
                }
                if lo.is_finite() {
                    let span = (hi - lo).max(1e-6);
                    state.y_lock_min = lo - span * 0.05;
                    state.y_lock_max = hi + span * 0.05;
                    state.y_locked = true;
                }
            }
        }
        KeyCode::Char('l') if key.modifiers.contains(KeyModifiers::CONTROL) => {
            // Ctrl+L: no-op; ratatui redraws every frame
        }
        _ => {}
    }
    false
}

fn handle_mouse(state: &mut AppState, mouse: crossterm::event::MouseEvent) {
    match mouse.kind {
        MouseEventKind::ScrollDown => {
            state.zoom = (state.zoom * 2).min(state.config.history);
        }
        MouseEventKind::ScrollUp => {
            state.zoom = (state.zoom / 2).max(10);
        }
        MouseEventKind::Down(MouseButton::Left) => {
            // Click inside status panel to select a stream
            // We don't have easy layout info here; skip for now
        }
        _ => {}
    }
}

// ── Main run loop ─────────────────────────────────────────────────────────────

pub fn run(config: Config, data_fd: i32) -> Result<()> {
    // Channel for reader thread → main thread
    let (tx, rx) = crossbeam_channel::bounded(100_000);

    // Spawn reader thread
    let filter = config.filter.clone();
    let _reader = spawn_reader(data_fd, tx, filter);

    // Build state
    let fps = config.fps;
    let mut state = AppState::new(config, rx);

    // Set up panic hook for terminal cleanup
    let original_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        let _ = disable_raw_mode();
        let _ = execute!(io::stdout(), LeaveAlternateScreen, DisableMouseCapture);
        original_hook(info);
    }));

    // Initialize terminal
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    let tick_duration = Duration::from_millis(1000 / fps as u64);
    let mut last_tick = Instant::now();

    loop {
        // Drain incoming data
        if !state.paused {
            drain_channel(&mut state);
        }

        // Render
        terminal.draw(|f| crate::ui::render(f, &state))?;

        // FPS tracking
        let now = Instant::now();
        let dt = now.duration_since(state.last_render).as_secs_f64();
        state.fps_display = if dt > 0.0 { 1.0 / dt } else { state.fps_display };
        state.last_render = now;

        // Event polling with timeout to next tick
        let timeout = tick_duration.saturating_sub(last_tick.elapsed());
        if event::poll(timeout)? {
            match event::read()? {
                Event::Key(key) => {
                    if handle_key(&mut state, key) {
                        break;
                    }
                }
                Event::Mouse(mouse) => handle_mouse(&mut state, mouse),
                Event::Resize(_, _) => {}
                _ => {}
            }
        }

        if last_tick.elapsed() >= tick_duration {
            last_tick = Instant::now();
        }
    }

    // Cleanup
    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen, DisableMouseCapture)?;
    Ok(())
}
