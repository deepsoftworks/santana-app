#!/usr/bin/env bash
# Monitor system memory usage as a percentage.
# Uses a fixed Y axis (0–100%) so the scale never jumps around.
#
# Usage: ./examples/memory_usage.sh | ./build/santana \
#   -t "Memory Usage" -u "%" -m line \
#   --min 0 --max 100 --color yellow \
#   --scale 50 --history 300 --fps 1
#
# Features: fixed axis (--min/--max), soft initial scale, yellow theme,
#           large history for long-term trend, low fps (sampling every 2s).

set -euo pipefail

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
        sleep 2
    done
elif [[ -f /proc/meminfo ]]; then
    while true; do
        awk '/^MemTotal/     { total = $2 }
             /^MemAvailable/ { avail = $2 }
             END { printf "%.1f\n", (total - avail) / total * 100 }' /proc/meminfo
        sleep 2
    done
else
    echo "Unsupported platform" >&2; exit 1
fi
