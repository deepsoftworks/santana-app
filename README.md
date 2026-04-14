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

`Santana` is a realtime terminal data plotter for logs and numeric streams.

<p align="center">
    <a href="https://github.com/deepsoftworks/santana-app/stargazers"><img src="https://img.shields.io/github/stars/deepsoftworks/santana-app?style=flat&color=yellow" alt="Stars"></a>
    <a href="https://github.com/deepsoftworks/santana-app/commits/main"><img src="https://img.shields.io/github/last-commit/deepsoftworks/santana-app?style=flat" alt="Last Commit"></a>
    <a href="LICENSE"><img src="https://img.shields.io/github/license/deepsoftworks/santana-app?style=flat" alt="License"></a>
    <a href="#"><img src="https://img.shields.io/github/repo-size/deepsoftworks/santana"></a>
</p>

## What It Does

Pipe output like `python main.py | santana` and Santana will auto-detect fields and plot them live in the terminal.

It understands:

- single numeric values
- whitespace-separated numeric rows
- CSV rows
- JSON objects and arrays
- freeform logs with numeric fields, for example:

```text
[LOG] a=3, y=4 b:5; r 4
```

That line becomes four streams: `a`, `y`, `b`, and `r`.

## Features

- Auto field extraction from structured logs with no field-selection flags.
- Dynamic stream discovery, including fields that appear later in the input.
- Three modes: `line`, `bar`, and `spark`.
- Fixed or auto Y-range with optional `--log-scale`.
- Compact bottom status panel with per-stream current, min, max, avg, and count.
- Works well for counter-style logs via `--rate`.

## Build

```bash
# macOS
cmake -S . -B build-mod -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build-mod -j"$(sysctl -n hw.logicalcpu)"

# Linux
cmake -S . -B build-mod -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build-mod -j"$(nproc)"
```

## Usage

```bash
./build-mod/santana [options]
```

### Common pipelines

```bash
# One value per line
yes 42 | ./build-mod/santana

# Python logs with named fields
python main.py | ./build-mod/santana

# Freeform logs
printf '[LOG] a=3, y=4 b:5; r 4\n' | ./build-mod/santana

# JSON
printf '{"latency":12,"cpu":45}\n' | ./build-mod/santana

# Counter streams
./examples/network.sh | ./build-mod/santana --title "Network" --unit B --rate
```

### Options

```text
  -t,--title TEXT     Chart title
  -m,--mode TEXT      line, bar, or spark
  -u,--unit TEXT      Unit label
  --min FLOAT         Fixed Y axis minimum
  --max FLOAT         Fixed Y axis maximum
  --log-scale         Logarithmic Y axis
  --history INT       Samples to keep per stream
  --fps INT           UI refresh rate
  -r,--rate           Plot deltas per second for counters
```

### Interactive keys

- `q` / `Esc`: quit
- `r`: toggle rate mode
- `Ctrl+L`: force redraw

## Examples

Only two demos ship with Santana now:

```bash
./examples/network.sh
./examples/memory_usage.sh
```

Both scripts also support raw output, so you can pipe them into your own Santana command:

```bash
SANTANA_EXAMPLE_RAW=1 ./examples/network.sh | ./build-mod/santana --title "Network" --unit B --rate
SANTANA_EXAMPLE_RAW=1 ./examples/memory_usage.sh | ./build-mod/santana --title "Memory Usage" --unit % --min 0 --max 100
```
