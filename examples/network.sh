#!/usr/bin/env bash
set -euo pipefail

if [[ -t 1 && "${SANTANA_EXAMPLE_RAW:-0}" != "1" ]]; then
    ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    SANTANA_BIN="${SANTANA_BIN:-$ROOT_DIR/build-mod/santana}"
    if [[ ! -x "$SANTANA_BIN" ]]; then
        echo "santana binary not found at $SANTANA_BIN" >&2
        echo "Build first: cmake -S . -B build-mod -G Ninja && cmake --build build-mod" >&2
        exit 1
    fi

    SANTANA_EXAMPLE_RAW=1 "$0" "$@" | "$SANTANA_BIN" \
        --title "Network" \
        --unit "B" \
        --rate \
        --history 180 \
        --fps 12
    exit $?
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
    IFACE="${1:-en0}"
    while true; do
        netstat -ib | awk -v iface="$IFACE" '
            $1 == iface && /Link/ {
                printf "rx=%s tx=%s\n", $7, $10
                fflush()
                exit
            }'
        sleep 0.25
    done
elif [[ -f /proc/net/dev ]]; then
    IFACE="${1:-$(ip route 2>/dev/null | awk "/^default/ { print \$5; exit }")}"
    IFACE="${IFACE:-eth0}"
    while true; do
        awk -v iface="${IFACE}:" '
            $1 == iface {
                printf "rx=%s tx=%s\n", $2, $10
                fflush()
            }' /proc/net/dev
        sleep 0.25
    done
else
    echo "Unsupported platform" >&2
    exit 1
fi
