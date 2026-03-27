#!/usr/bin/env bash
# Count HTTP 2xx (success) and 5xx (error) responses per second from a web
# server access log in Combined Log Format (nginx/apache default).
# Reads log lines from stdin and emits two counts per second.
#
# Usage: tail -f /var/log/nginx/access.log | ./examples/http_requests.sh | \
#   ./build/santana -2 -t "HTTP Traffic" -u "req/s" \
#   -m line -C dark1 -c "|" \
#   --hard-max 10000 -e "!" --hard-min 0 \
#   --history 120 --fps 4
#
# Features: dual-graph (-2), per-element color scheme (-C dark1),
#           custom plot char (-c "|"), hard-max alert for traffic spikes,
#           high fps for smooth rendering of bursty request counts.
#
# Tip: generate test traffic with: while true; do curl -s localhost; done

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
