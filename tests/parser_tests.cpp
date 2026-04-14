#include "parser.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "parser_tests: " << message << '\n';
        std::exit(1);
    }
}

void require_field(const ParsedRow& row, const std::string& key, double expected) {
    for (const auto& field : row.fields) {
        if (field.key == key) {
            require(std::abs(field.value - expected) < 1e-9, "unexpected value for key '" + key + "'");
            return;
        }
    }
    require(false, "missing key '" + key + "'");
}

}  // namespace

int main() {
    {
        const ParsedRow row = parse_line("[LOG] a=3, y=4 b:5; r 4");
        require(row.fmt == LineFormat::KeyValueLog, "freeform log should parse as key/value");
        require(row.fields.size() == 4, "freeform log should emit 4 fields");
        require_field(row, "a", 3.0);
        require_field(row, "y", 4.0);
        require_field(row, "b", 5.0);
        require_field(row, "r", 4.0);
    }

    {
        const ParsedRow row = parse_line("cpu 12.5 mem=42 temp:71");
        require(row.fmt == LineFormat::KeyValueLog, "mixed key/value separators should parse");
        require_field(row, "cpu", 12.5);
        require_field(row, "mem", 42.0);
        require_field(row, "temp", 71.0);
    }

    {
        const ParsedRow row = parse_line(R"({"cpu":42,"mem":{"used":73},"rx":[1,2]})");
        require(row.fmt == LineFormat::JSONObject, "json object should parse as object");
        require(row.fields.size() == 4, "json object should flatten numeric leaves");
        require_field(row, "cpu", 42.0);
        require_field(row, "mem.used", 73.0);
        require_field(row, "rx.1", 1.0);
        require_field(row, "rx.2", 2.0);
    }

    {
        const ParsedRow row = parse_line("1,2,3");
        require(row.fmt == LineFormat::CSV, "csv row should parse as csv");
        require_field(row, "s1", 1.0);
        require_field(row, "s2", 2.0);
        require_field(row, "s3", 3.0);
    }

    {
        const ParsedRow row = parse_line("4.5");
        require(row.fmt == LineFormat::Single, "single number should parse as single");
        require(row.fields.size() == 1, "single number should emit one field");
        require_field(row, "value", 4.5);
    }

    {
        const ParseResult result = detect_format("[metrics] rx=10 tx=20");
        require(result.fmt == LineFormat::KeyValueLog, "detect_format should classify key/value logs");
        require(result.num_values == 2, "detect_format should count extracted fields");
        require(result.keys.size() == 2, "detect_format should expose extracted keys");
        require(result.keys[0] == "rx", "first detected key should be rx");
        require(result.keys[1] == "tx", "second detected key should be tx");
    }

    {
        const ParsedRow first = parse_line("a=1 b=2");
        const ParsedRow second = parse_line("a=3 b=4 c=5");
        require(first.fields.size() == 2, "first row should only contain initial keys");
        require(second.fields.size() == 3, "later row should keep newly appearing fields");
        require_field(second, "c", 5.0);
    }

    return 0;
}
