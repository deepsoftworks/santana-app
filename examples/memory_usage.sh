#!/usr/bin/env bash
# Monitor system memory usage as a percentage.
# Uses a fixed Y axis (0–100%) so the scale never jumps around.
#
# Usage: ./examples/memory_usage.sh | ./build/santana \
#   -t "Memory Usage" -u "%" -m line \
#   --min 0 --max 100 --color yellow \
#   --scale 50 --history 300 --fps 2
#
# Features: fixed axis (--min/--max), soft initial scale, yellow theme,
#           large history for long-term trend, high refresh rate (500ms).

set -euo pipefail

if [[ -t 1 && "${SANTANA_EXAMPLE_RAW:-0}" != "1" ]]; then
    ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    SANTANA_BIN="${SANTANA_BIN:-$ROOT_DIR/build/santana}"
    if [[ ! -x "$SANTANA_BIN" ]]; then
        echo "santana binary not found at $SANTANA_BIN" >&2
        echo "Build first: cmake -S . -B build && cmake --build build" >&2
        exit 1
    fi
    SANTANA_EXAMPLE_RAW=1 "$0" "$@" | "$SANTANA_BIN" \
      -t "Memory Usage" -u "%" -m line \
      --min 0 --max 100 --color yellow \
      --scale 50 --history 300 --fps 2
    exit $?
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
    PAGE=$(sysctl -n hw.pagesize 2>/dev/null || echo 4096)
    while true; do
        vm_stat | awk -v page="$PAGE" '
            /^Pages active/      { gsub(/\./, "", $NF); active = $NF + 0 }
            /^Pages wired/       { gsub(/\./, "", $NF); wired  = $NF + 0 }
            /^Pages occupied/    { gsub(/\./, "", $NF); comp   = $NF + 0 }
            /^Pages free/        { gsub(/\./, "", $NF); free   = $NF + 0 }
            /^Pages speculative/ { gsub(/\./, "", $NF); spec   = $NF + 0 }
            /^Pages inactive/    { gsub(/\./, "", $NF); inact  = $NF + 0 }
            END {
                used  = (active + wired + comp) * page
                total = (active + wired + comp + free + spec + inact) * page
                if (total > 0) printf "%.1f\n", used / total * 100
            }'
        sleep 0.5
    done
elif [[ -f /proc/meminfo ]]; then
    while true; do
        awk '/^MemTotal/     { total = $2 }
             /^MemAvailable/ { avail = $2 }
             END { printf "%.1f\n", (total - avail) / total * 100 }' /proc/meminfo
        sleep 0.5
    done
else
    echo "Unsupported platform" >&2; exit 1
fi
