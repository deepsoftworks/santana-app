#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SANTANA_BIN="${SANTANA_BIN:-$ROOT_DIR/build/santana}"

if [[ ! -x "$SANTANA_BIN" ]]; then
    echo "santana binary not found at $SANTANA_BIN" >&2
    echo "Build first: cmake -S . -B build && cmake --build build" >&2
    exit 1
fi

mode="${1:-demo}"

case "$mode" in
  demo)
    awk 'BEGIN {
      t=0;
      while (1) {
        v = 50 + 35 * sin(t / 8.0) + (rand() * 8 - 4);
        printf "%.2f\n", v;
        fflush();
        t++;
        system("sleep 0.2");
      }
    }' | "$SANTANA_BIN" -t "Santana Demo" -u "units" -m line --min 0 --max 100 --history 240 --fps 15
    ;;
  load)
    exec "$ROOT_DIR/examples/load_average.sh"
    ;;
  mem|memory)
    exec "$ROOT_DIR/examples/memory_usage.sh"
    ;;
  net|network)
    exec "$ROOT_DIR/examples/network_bandwidth.sh" "${2:-}"
    ;;
  http)
    exec "$ROOT_DIR/examples/http_requests.sh"
    ;;
  *)
    echo "Usage: $0 [demo|load|memory|network [iface]|http]" >&2
    exit 1
    ;;
esac
