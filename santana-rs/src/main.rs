mod app;
mod buffer;
mod config;
mod parser;
mod source;
mod ui;

use std::ffi::CString;

use anyhow::{bail, Result};
use clap::Parser;

use config::{ChartMode, Config, ThemeName};

// ── CLI definition ─────────────────────────────────────────────────────────────

#[derive(Parser, Debug)]
#[command(
    name = "santana",
    version = "2.0.0",
    about = "Live terminal data visualization — pipe anything, chart everything"
)]
struct Cli {
    /// Chart title
    #[arg(short, long, default_value = "santana")]
    title: String,

    /// Chart type: line, bar, spark, overlay, split
    #[arg(short, long, value_enum, default_value = "line")]
    mode: ChartMode,

    /// Unit label (e.g. MB/s, %)
    #[arg(short, long, default_value = "")]
    unit: String,

    /// Fixed Y-axis minimum
    #[arg(long)]
    min: Option<f64>,

    /// Fixed Y-axis maximum
    #[arg(long)]
    max: Option<f64>,

    /// Logarithmic Y axis
    #[arg(long)]
    log_scale: bool,

    /// Samples to keep per stream (10–10000)
    #[arg(long, default_value = "120")]
    history: usize,

    /// UI refresh rate in Hz (1–120)
    #[arg(long, default_value = "16")]
    fps: u8,

    /// Plot deltas per second (rate mode)
    #[arg(short, long)]
    rate: bool,

    /// Only capture fields whose keys match this regex
    #[arg(long)]
    filter: Option<String>,

    /// Color theme: dark, light, solarized, nord
    #[arg(long, value_enum, default_value = "dark")]
    theme: ThemeName,
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    // Build config
    let filter = cli
        .filter
        .as_deref()
        .map(|s| regex::Regex::new(s))
        .transpose()
        .map_err(|e| anyhow::anyhow!("Invalid filter regex: {}", e))?;

    let config = Config {
        title:     cli.title,
        mode:      cli.mode,
        unit:      cli.unit,
        y_min:     cli.min,
        y_max:     cli.max,
        log_scale: cli.log_scale,
        history:   cli.history.clamp(10, 10_000),
        fps:       cli.fps.clamp(1, 120),
        rate_mode: cli.rate,
        filter,
        theme:     cli.theme,
    };

    // ── TTY dup dance (must happen before crossterm initializes) ──────────────
    // 1. Duplicate the piped stdin fd so the reader thread can use it
    let data_fd = unsafe { libc::dup(libc::STDIN_FILENO) };
    if data_fd < 0 {
        bail!("Failed to duplicate stdin fd");
    }

    // 2. Re-open /dev/tty as stdin so crossterm can read keyboard events
    if unsafe { libc::isatty(libc::STDIN_FILENO) } == 0 {
        let tty_path = CString::new("/dev/tty").unwrap();
        let tty_fd = unsafe { libc::open(tty_path.as_ptr(), libc::O_RDONLY) };
        if tty_fd < 0 {
            unsafe { libc::close(data_fd) };
            bail!("Failed to open /dev/tty for interactive input");
        }
        if unsafe { libc::dup2(tty_fd, libc::STDIN_FILENO) } < 0 {
            unsafe {
                libc::close(tty_fd);
                libc::close(data_fd);
            }
            bail!("Failed to redirect /dev/tty to stdin");
        }
        unsafe { libc::close(tty_fd) };
    }

    // 3. Run the app
    app::run(config, data_fd)
}
