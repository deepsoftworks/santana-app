#!/usr/bin/env bash
# Monitor network RX and TX throughput for the primary interface.
# Uses dual-graph + rate mode: santana computes bytes/sec from counter deltas.
#
# Usage: ./examples/network_bandwidth.sh [iface] | ./build/santana \
#   -2 -r -m bar -t "Network Bandwidth" -u "B/s" \
#   --color cyan --hard-max 125000000 -e "!" \
#   --history 60 --fps 2
#
# Features: dual-graph (-2), rate mode (-r), bar chart, hard-max error indicator,
#           custom error char, fixed history window, low fps for stable bars.
# Note: hard-max 125000000 = 1 Gbps (125 MB/s). Adjust for your link speed.

set -euo pipefail

if [[ "$(uname -s)" == "Darwin" ]]; then
    IFACE=${1:-en0}
    while true; do
        # netstat -ib columns: Name Mtu Network Address Ipkts Ierrs Ibytes Opkts Oerrs Obytes
        # The Link-level row carries cumulative byte counters; IP rows do not.
        netstat -ib | awk -v iface="$IFACE" \
            '$1 == iface && /Link/ { print $7; print $10; exit }'
        sleep 1
    done
elif [[ -f /proc/net/dev ]]; then
    IFACE=${1:-$(ip route 2>/dev/null | awk '/^default/ { print $5; exit }')}
    IFACE=${IFACE:-eth0}
    while true; do
        # /proc/net/dev after "iface:": col 2 = rx_bytes, col 10 = tx_bytes
        awk -v iface="${IFACE}:" '$1 == iface { print $2; print $10 }' /proc/net/dev
        sleep 1
    done
else
    echo "Unsupported platform" >&2; exit 1
fi
