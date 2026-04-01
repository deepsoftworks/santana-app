<p align="center">
  <img src="santana.png" alt="Santana" width="100">
</p>

```
 ░▒▓███████▓▒░░▒▓██████▓▒░░▒▓███████▓▒░▒▓████████▓▒░▒▓██████▓▒░░▒▓███████▓▒░ ░▒▓██████▓▒░
░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▒░  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░
░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▒░  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░
 ░▒▓██████▓▒░░▒▓████████▓▒░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▒░  ░▒▓████████▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓████████▓▒░
       ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▒░  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░
       ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▒░  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░
░▒▓███████▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▒░  ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░
```

*Work in progress-PRs welcome*

`Santana` is a highly customizable real-time terminal data visualization interface.

<img src="examples/network.gif" width="49%"/> <img src="examples/sine.gif" width="49%"/>
<img src="examples/load.gif" width="49%"/> <img src="examples/http.gif" width="49%"/>


## Features

- **Zero-config multi-series** — pipe whitespace or CSV rows and streams are detected automatically
- **First-class JSON** — flat objects, nested paths, arrays; field selection and `--jq` dot-paths
- **Three chart types**: line (braille-dot polyline), bar, sparkline
- **Auto-scaling** Y axis with optional fixed min/max and `--log-scale`
- **Stats footer + legend**: current | min | max | mean | count, with color-coded legend for multi-stream
- **Streaming window control**: `--window N`, `--no-scroll`, `h` key to zoom
- **Color themes**: green, cyan, yellow, red, white + named schemes
- **Responsive**: adapts to terminal resize instantly
- **ttyplot-compatible**: pipe any newline-delimited numeric stream

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)   # Linux
cmake --build build -j$(sysctl -n hw.logicalcpu)  # macOS
```

Requires: CMake ≥ 3.20, C++20 compiler.
Dependencies (FTXUI v5.0.0, CLI11 v2.4.1, nlohmann/json v3.11.3) are fetched automatically.


## Usage

```
santana [OPTIONS] [fields...]
```

### Input formats (auto-detected)

```bash
# Single value per line (original behavior)
echo 42 | santana

# Whitespace-separated — N values → N streams, auto-labeled s1 s2 s3
echo "1.2 3.4 5.6" | santana

# CSV — same but comma-delimited
echo "1.2,3.4,5.6" | santana

# JSON object — keys become stream labels
echo '{"latency":12,"cpu":45}' | santana

# JSON object — extract specific fields in order
echo '{"latency":12,"cpu":45,"mem":80}' | santana latency mem

# JSON object — navigate a nested path (single stream)
echo '{"metrics":{"latency":12}}' | santana --jq .metrics.latency

# JSON array
echo '[10, 20, 30]' | santana
```

### Options

```
  -h,--help                   Print this help message and exit
  -V,--version                Display program version information and exit

Input:
  fields                      JSON field names to extract (positional)
  --jq TEXT                   JSON dot-path to extract, e.g. .metrics.latency
  -n,--streams INT            Number of interleaved streams, 1-10 (overrides auto-detect)
  -2                          Shorthand for -n 2

Chart:
  -t,--title TEXT             Chart title
  -m,--mode TEXT:{line,bar,spark}
                              Chart type (default: line)
  -u,--unit TEXT              Unit label, e.g. MB/s
  --color TEXT:{green,cyan,yellow,red,white}
                              Primary stream color (default: green)
  --color2 TEXT               Second stream color
  -C,--colors TEXT            Per-element colors: plot[,axes,text,title,max_err,min_err] (0-7)
                                Named schemes: dark1, dark2, light1, light2, vampire
  -c,--char TEXT              Plot character (default: braille)
  --labels TEXT               Comma-separated stream labels, e.g. cpu,mem,net

Y axis:
  --min,--y-min FLOAT         Fixed Y axis minimum
  --max,--y-max FLOAT         Fixed Y axis maximum
  --log-scale                 Logarithmic Y axis
  --scale FLOAT               Initial soft scale (autoscale can exceed this)
  --hard-min FLOAT            Hard minimum — draws error indicator if breached
  --hard-max FLOAT            Hard maximum — draws error indicator if breached
  -e,--error-max-char TEXT    Overflow indicator character (default: e)
  -E,--error-min-char TEXT    Underflow indicator character (default: v)

Window:
  --history,--window INT      Sliding buffer size in data points (default: 120)
  --no-scroll                 Fixed-frame mode (shows [FIXED] in title)
  --fps INT                   Refresh rate (default: 16)

Modes:
  -r,--rate                   Rate mode: values divided by elapsed time (for counters)
```

### Interactive keys

| Key | Action |
|-----|--------|
| `q` / `Esc` | Quit |
| `r` | Toggle rate mode |
| `h` | Toggle zoom (last 20 points) |
| `Ctrl+L` | Force redraw |

## Examples

```bash
./examples/exp.sh            # standalone demo signal
./examples/exp.sh load       # system load (1m/5m)
./examples/exp.sh memory     # memory usage %
./examples/exp.sh network    # rx/tx throughput
./examples/exp.sh http       # synthetic HTTP traffic
```

