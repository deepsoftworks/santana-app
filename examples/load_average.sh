#!/usr/bin/env bash
# Monitor 1-minute and 5-minute system load averages side by side.
# The 1m line reacts fast; the 5m line shows the sustained trend.
#
# Usage: ./examples/load_average.sh | ./build/santana \
#   -2 -t "Load Average (1m / 5m)" -m line \
#   -C dark2 --scale 4 --history 360 --fps 1
#
# Features: dual-graph (-2), per-element color scheme (-C dark2),
#           soft initial scale (--scale matches typical CPU core count),
#           large history window (360 samples × 5s = 30 min of trend),
#           spark mode alternative: replace "-m line" with "-m spark".

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
    sleep 5
done
