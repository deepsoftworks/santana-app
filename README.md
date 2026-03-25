# Santana

Real-time terminal data visualization utility.

## Features

- **Three chart types**: line (braille-dot polyline), bar, sparkline
- **Auto-scaling** Y axis with optional fixed min/max
- **Stats footer**: current | min | max | mean | sample count
- **Color themes**: green, cyan, yellow, red, white
- **Responsive**: adapts to terminal resize instantly
- **ttyplot-compatible**: pipe any newline-delimited numeric stream

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)   # Linux
cmake --build build -j$(sysctl -n hw.logicalcpu)  # macOS
```

Requires: CMake ≥ 3.20, C++ (std 20) compiler. 
Dependencies (FTXUI v5.0.0, CLI11 v2.4.1) are fetched automatically.

## Usage

```
santana [OPTIONS]

Options:
  -t, --title TEXT        Chart title
  -m, --mode TEXT         Chart type: line (default), bar, spark
  -u, --unit TEXT         Unit label shown on Y axis (e.g. "MB/s", "%")
  --min FLOAT             Fixed Y axis minimum (default: auto)
  --max FLOAT             Fixed Y axis maximum (default: auto)
  -s, --scroll            Scroll mode (new data enters from right)
  --history INT           Number of data points to keep (default: 120)
  --fps INT               Target refresh rate (default: 16)
  --color TEXT            Chart color: green (default), cyan, yellow, red, white
  -h, --help              Show help
  -V, --version           Show version
```

## Key bindings

| Key | Action |
|-----|--------|
| `q` / `Esc` | Quit |
