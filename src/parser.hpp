#pragma once
#include <string>
#include <vector>

enum class LineFormat { Single, Whitespace, CSV, JSONObject, JSONArray };

struct ParseResult {
    LineFormat fmt        = LineFormat::Single;
    int        num_values = 1;
    std::vector<std::string> keys;  // field names or "s1","s2",... or empty
};

// Inspect the first non-empty line to determine its format and stream count.
ParseResult detect_format(const std::string& line);

// Parse a single line according to the given format.
// field_selectors: for JSON, extract only these fields in this order.
// jq_path:        for JSON, navigate a dot-path like ".metrics.latency".
// Returns a vector of values; empty if line contains no parseable numbers.
std::vector<double> parse_line(const std::string& line, LineFormat fmt,
                               const std::vector<std::string>& field_selectors = {},
                               const std::string& jq_path = "");
