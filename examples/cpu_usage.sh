#!/usr/bin/env bash
# Streams CPU usage percentage for testing santana
# Usage: ./examples/cpu_usage.sh | ./build/santana -t "CPU %" -u "%" --max 100

if [[ "$(uname -s)" == "Darwin" ]]; then
    top -l 0 -s 1 -n 0 | grep -E "^CPU" | awk '{ gsub(/%/,""); print 100 - $7; fflush() }'
elif command -v vmstat &>/dev/null; then
    vmstat 1 | awk 'NR>2 { idle=$15; print 100 - idle; fflush() }'
else
    echo "vmstat or top not found" >&2
    exit 1
fi
