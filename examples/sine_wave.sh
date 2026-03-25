#!/usr/bin/env bash
# Generates a continuous sine wave stream for testing santana
# Usage: ./examples/sine_wave.sh | ./build/santana -t "Sine Wave" -u "amplitude"

i=0
while true; do
    python3 -c "import math; print(f'{math.sin($i * 0.2):.4f}')"
    i=$((i + 1))
    sleep 0.1
done
