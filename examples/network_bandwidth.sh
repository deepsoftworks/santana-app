#!/usr/bin/env bash
# Monitor network RX and TX throughput for the primary interface.
# Uses dual-graph + rate mode: santana computes bytes/sec from counter deltas.
#
# Usage: ./examples/network_bandwidth.sh [iface] | ./build/santana \
#   -2 -r -m bar -t "Network Bandwidth" -u "B/s" \
#   --color red --color2 black \
#   --history 60 --fps 10
#
# Features: dual-graph (-2), rate mode (-r), bar chart,
#           auto-scaled axis, fixed history window, 100ms refresh rate.

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
      -2 -r -m bar -t "Network Bandwidth" -u "B/s" \
      --color red --color2 black \
      --history 60 --fps 10
    exit $?
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
    IFACE=${1:-en0}
    while true; do
        # netstat -ib columns: Name Mtu Network Address Ipkts Ierrs Ibytes Opkts Oerrs Obytes
        # The Link-level row carries cumulative byte counters; IP rows do not.
        netstat -ib | awk -v iface="$IFACE" \
            '$1 == iface && /Link/ { print $7; print $10; exit }'
        sleep 0.1
    done
elif [[ -f /proc/net/dev ]]; then
    IFACE=${1:-$(ip route 2>/dev/null | awk '/^default/ { print $5; exit }')}
    IFACE=${IFACE:-eth0}
    while true; do
        # /proc/net/dev after "iface:": col 2 = rx_bytes, col 10 = tx_bytes
        awk -v iface="${IFACE}:" '$1 == iface { print $2; print $10 }' /proc/net/dev
        sleep 0.1
    done
else
    echo "Unsupported platform" >&2; exit 1
fi
