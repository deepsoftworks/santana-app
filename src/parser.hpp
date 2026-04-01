#pragma once
#include <string>
#include <vector>

enum class LineFormat { Single, Whitespace, CSV };

struct ParseResult {
    LineFormat fmt        = LineFormat::Single;
    int        num_values = 1;
    std::vector<std::string> keys;  // auto-labels: "s1","s2",... or empty
};

// Inspect the first non-empty line to determine its format and stream count.
ParseResult detect_format(const std::string& line);

// Parse a single line according to the given format.
// Returns a vector of values; empty if line contains no parseable numbers.
std::vector<double> parse_line(const std::string& line, LineFormat fmt);
