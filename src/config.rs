use regex::Regex;

#[derive(Debug, Clone, Copy, PartialEq, Eq, clap::ValueEnum)]
pub enum ChartMode {
    Line,
    Bar,
    Spark,
    Overlay,
    Split,
}

impl ChartMode {
    pub fn cycle(self) -> Self {
        match self {
            ChartMode::Line    => ChartMode::Bar,
            ChartMode::Bar     => ChartMode::Spark,
            ChartMode::Spark   => ChartMode::Overlay,
            ChartMode::Overlay => ChartMode::Split,
            ChartMode::Split   => ChartMode::Line,
        }
    }

    pub fn label(self) -> &'static str {
        match self {
            ChartMode::Line    => "line",
            ChartMode::Bar     => "bar",
            ChartMode::Spark   => "spark",
            ChartMode::Overlay => "overlay",
            ChartMode::Split   => "split",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, clap::ValueEnum)]
pub enum ThemeName {
    Dark,
    Light,
    Solarized,
    Nord,
}

#[derive(Debug)]
pub struct Config {
    pub title:     String,
    pub mode:      ChartMode,
    pub unit:      String,
    pub y_min:     Option<f64>,
    pub y_max:     Option<f64>,
    pub log_scale: bool,
    pub history:   usize,
    pub fps:       u8,
    pub rate_mode: bool,
    pub filter:    Option<Regex>,
    pub theme:     ThemeName,
}
