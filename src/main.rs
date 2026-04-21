mod app;
mod buffer;
mod config;
mod parser;
mod source;
mod ui;

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

    /// Chart type: line, bar, split
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
    #[arg(short, long, default_value_t = false)]
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
        title: cli.title,
        mode: cli.mode,
        unit: cli.unit,
        y_min: cli.min,
        y_max: cli.max,
        log_scale: cli.log_scale,
        history: cli.history.clamp(10, 10_000),
        fps: cli.fps.clamp(1, 120),
        rate_mode: cli.rate,
        filter,
        theme: cli.theme,
    };

    // Duplicate the piped stdin fd so the reader thread can consume data from it,
    // while crossterm (with use-dev-tty feature) opens /dev/tty directly for input.
    let data_fd = unsafe { libc::dup(libc::STDIN_FILENO) };
    if data_fd < 0 {
        bail!("Failed to duplicate stdin fd");
    }

    app::run(config, data_fd)
}
