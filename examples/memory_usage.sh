#!/usr/bin/env bash
set -euo pipefail

if [[ -t 1 && "${SANTANA_EXAMPLE_RAW:-0}" != "1" ]]; then
    ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    SANTANA_BIN="${SANTANA_BIN:-$ROOT_DIR/target/debug/santana}"
    if [[ ! -x "$SANTANA_BIN" ]]; then
        echo "santana binary not found at $SANTANA_BIN" >&2
        echo "Build first: cargo build --debug" >&2
        exit 1
    fi

    SANTANA_EXAMPLE_RAW=1 "$0" "$@" | "$SANTANA_BIN" \
        --title "Memory Usage" \
        --unit "%" \
        --min 0 \
        --max 100 \
        --history 180 \
        --fps 2
    exit $?
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
    PAGE_SIZE=$(sysctl -n hw.pagesize 2>/dev/null || echo 4096)
    while true; do
        vm_stat | awk -v page_size="$PAGE_SIZE" '
            /^Pages active/       { gsub(/\./, "", $NF); active = $NF + 0 }
            /^Pages wired/        { gsub(/\./, "", $NF); wired = $NF + 0 }
            /^Pages occupied/     { gsub(/\./, "", $NF); compressed = $NF + 0 }
            /^Pages free/         { gsub(/\./, "", $NF); free_pages = $NF + 0 }
            /^Pages speculative/  { gsub(/\./, "", $NF); speculative = $NF + 0 }
            /^Pages inactive/     { gsub(/\./, "", $NF); inactive = $NF + 0 }
            END {
                used = active + wired + compressed
                cache = inactive + speculative
                total = used + cache + free_pages
                if (total > 0) {
                    printf "used=%.1f cache=%.1f free=%.1f\n",
                        used / total * 100.0,
                        cache / total * 100.0,
                        free_pages / total * 100.0
                    fflush()
                }
            }'
        sleep 0.5
    done
elif [[ -f /proc/meminfo ]]; then
    while true; do
        awk '
            /^MemTotal:/     { total = $2 }
            /^MemAvailable:/ { available = $2 }
            /^MemFree:/      { free_kb = $2 }
            /^Cached:/       { cached = $2 }
            /^Buffers:/      { buffers = $2 }
            END {
                cache = cached + buffers
                used = total - available
                if (total > 0) {
                    printf "used=%.1f cache=%.1f free=%.1f\n",
                        used / total * 100.0,
                        cache / total * 100.0,
                        free_kb / total * 100.0
                    fflush()
                }
            }' /proc/meminfo
        sleep 0.5
    done
else
    echo "Unsupported platform" >&2
    exit 1
fi
