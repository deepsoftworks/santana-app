#!/usr/bin/env bash
set -euo pipefail

# Synthetic data demo — shows off multiple streams with interesting patterns.
# Outputs key=value lines with three series:
#   wave   — smooth sine oscillation
#   pulse  — periodic spikes on a calm baseline
#   drift  — slow random walk with momentum

if [[ -t 1 && "${SANTANA_EXAMPLE_RAW:-0}" != "1" ]]; then
    ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    SANTANA_BIN="${SANTANA_BIN:-$ROOT_DIR/target/debug/santana}"
    if [[ ! -x "$SANTANA_BIN" ]]; then
        echo "santana binary not found at $SANTANA_BIN" >&2
        echo "Build first: cargo build --debug" >&2
        exit 1
    fi

    SANTANA_EXAMPLE_RAW=1 "$0" "$@" | "$SANTANA_BIN" \
        --title "Demo" \
        --history 240 \
        --fps 12 \
        --theme light
    exit $?
fi

# ── generator (pure awk, no dependencies) ────────────────────────────────────

awk 'BEGIN {
    srand()
    pi    = atan2(0, -1)
    t     = 0
    drift = 50
    vel   = 0

    while (1) {
        # wave: sine with slowly shifting frequency
        wave = 50 + 40 * sin(2 * pi * t / 80 + sin(t / 200))

        # pulse: calm baseline with periodic spikes
        phase = t % 60
        if (phase >= 28 && phase <= 32)
            pulse = 70 + rand() * 25
        else
            pulse = 10 + rand() * 5

        # drift: random walk with mean-reversion
        vel   = vel * 0.95 + (rand() - 0.5) * 4 + (50 - drift) * 0.02
        drift = drift + vel
        if (drift < 0)  drift = 0
        if (drift > 100) drift = 100

        printf "wave=%.1f pulse=%.1f drift=%.1f\n", wave, pulse, drift
        fflush()

        # ~20 samples/sec — santana decimates to --fps
        system("sleep 0.05")
        t++
    }
}'
