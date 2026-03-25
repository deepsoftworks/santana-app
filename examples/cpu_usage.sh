#!/usr/bin/env bash
# Streams CPU usage percentage for testing santana
# Usage: ./examples/cpu_usage.sh | ./build/santana -t "CPU %" -u "%" --max 100

if command -v vmstat &>/dev/null; then
    # Linux/macOS vmstat: skip header lines, print idle column (15th), compute usage
    vmstat 1 | awk 'NR>2 { idle=$15; print 100 - idle; fflush() }'
elif command -v top &>/dev/null; then
    # macOS fallback using top
    top -l 0 -s 1 | grep -E "^CPU" | awk '{ gsub(/%/,""); print $3; fflush() }'
else
    echo "vmstat or top not found" >&2
    exit 1
fi
