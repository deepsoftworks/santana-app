#!/usr/bin/env bash
# Count HTTP 2xx (success) and 5xx (error) responses per second.
# - If stdin is piped: parse Combined Log Format lines (nginx/apache default).
# - If run directly in a TTY: generate synthetic traffic so the demo is standalone.
#
# Usage:
#   ./examples/http_requests.sh
#   tail -f /var/log/nginx/access.log | ./examples/http_requests.sh
#
# Features: dual-graph (-2), per-element color scheme (-C dark1),
#           custom plot char (-c "|"), auto-scaled axis,
#           high refresh rate (500ms) for smooth rendering of bursty request counts.
#
# Tip: generate test traffic with: while true; do curl -s localhost; done

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
      -2 -t "HTTP Traffic" -u "req/s" \
      -m line -C dark1 -c "|" \
      --history 120 --fps 2
    exit $?
fi

if [[ -t 0 ]]; then
    # Standalone mode: emit synthetic 2xx and 5xx counts every 500ms.
    while true; do
        ok=$((80 + RANDOM % 120))
        err=$((RANDOM % 8))
        echo "$ok"
        echo "$err"
        sleep 0.5
    done
fi

awk '
BEGIN { ok = 0; err = 0; t = systime() }
{
    # Combined Log Format: host ident user [date] "METHOD path proto" status size ...
    # Status code is field 9.
    code = $9 + 0
    if (code >= 200 && code < 300) ok++
    if (code >= 500 && code < 600) err++

    now = systime()
    if (now != t) {
        print ok; print err
        fflush()
        ok = 0; err = 0; t = now
    }
}
'
