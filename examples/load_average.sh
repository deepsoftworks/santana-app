#!/usr/bin/env bash
# Monitor 1-minute and 5-minute system load averages side by side.
# The 1m line reacts fast; the 5m line shows the sustained trend.
#
# Usage: ./examples/load_average.sh | ./build/santana \
#   -2 -t "Load Average (1m / 5m)" -m line \
#   -C vampire --scale 4 --history 360 --fps 2
#
# Features: dual-graph (-2), vampire color scheme for light backgrounds (-C vampire),
#           soft initial scale (--scale matches typical CPU core count),
#           large history window (360 samples × 500ms = 3 min of trend),
#           spark mode alternative: replace "-m line" with "-m spark".

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
      -2 -t "Load Average (1m / 5m)" -m line \
      -C vampire --scale 2 --history 360 --fps 2
    exit $?
fi

while true; do
    if [[ "$(uname -s)" == "Darwin" ]]; then
        # vm.loadavg → "{ 1m 5m 15m }" — print 1m then 5m
        sysctl -n vm.loadavg | awk '{ print $2; print $3 }'
    elif [[ -f /proc/loadavg ]]; then
        # /proc/loadavg → "1m 5m 15m ..." — print 1m then 5m
        awk '{ print $1; print $2 }' /proc/loadavg
    else
        echo "Unsupported platform" >&2; exit 1
    fi
    sleep 0.5
done
