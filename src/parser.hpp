#pragma once
#include <string>
#include <vector>

enum class LineFormat { Single, Whitespace, CSV, JSONObject, JSONArray, KeyValueLog };

struct ParsedField {
    std::string key;
    double value = 0.0;
};

struct ParsedRow {
    LineFormat fmt = LineFormat::Single;
    std::vector<ParsedField> fields;

    [[nodiscard]] bool empty() const { return fields.empty(); }
};

struct ParseResult {
    LineFormat fmt        = LineFormat::Single;
    int        num_values = 0;
    std::vector<std::string> keys;
};

// Inspect a non-empty line to determine the parser mode and initial field names.
ParseResult detect_format(const std::string& line);

// Parse a single line and emit named fields.
ParsedRow parse_line(const std::string& line);
