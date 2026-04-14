#include "parser.hpp"
#include <nlohmann/json.hpp>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string_view>

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    const size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

std::string make_series_key(size_t index) {
    return "s" + std::to_string(index + 1);
}

bool is_key_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.' || c == '-';
}

bool is_key_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string clean_key(std::string_view token) {
    size_t start = 0;
    while (start < token.size() && !is_key_char(token[start])) ++start;

    size_t end = token.size();
    while (end > start && !is_key_char(token[end - 1])) --end;

    if (start >= end || !is_key_start(token[start])) return "";
    return std::string(token.substr(start, end - start));
}

bool try_parse_number_strict(std::string token, double& out) {
    token = trim(token);
    if (token.empty()) return false;

    while (!token.empty()) {
        const char c = token.front();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.') break;
        if (std::ispunct(static_cast<unsigned char>(c)) == 0) return false;
        token.erase(token.begin());
    }
    while (!token.empty()) {
        const char c = token.back();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') break;
        if (std::ispunct(static_cast<unsigned char>(c)) == 0) return false;
        token.pop_back();
    }
    if (token.empty()) return false;

    char* end = nullptr;
    out = std::strtod(token.c_str(), &end);
    return end != token.c_str() && end != nullptr && *end == '\0';
}

bool try_parse_number_relaxed(std::string token, double& out) {
    token = trim(token);
    if (token.empty()) return false;

    while (!token.empty()) {
        const char c = token.front();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.') break;
        token.erase(token.begin());
    }
    while (!token.empty()) {
        const char c = token.back();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') break;
        token.pop_back();
    }
    if (token.empty()) return false;

    char* end = nullptr;
    out = std::strtod(token.c_str(), &end);
    return end != token.c_str() && end != nullptr && *end == '\0';
}

std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string token;
    while (ss >> token) tokens.push_back(token);
    return tokens;
}

std::vector<std::string> split_by_comma(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) tokens.push_back(trim(token));
    return tokens;
}

void append_field(std::vector<ParsedField>& fields, const std::string& key, double value) {
    if (key.empty()) return;
    for (auto& field : fields) {
        if (field.key == key) {
            field.value = value;
            return;
        }
    }
    fields.push_back({key, value});
}

void flatten_numeric(const nlohmann::json& j, const std::string& prefix, std::vector<ParsedField>& out) {
    if (j.is_number()) {
        append_field(out, prefix, j.get<double>());
        return;
    }

    if (j.is_object()) {
        for (const auto& [key, value] : j.items()) {
            const std::string next = prefix.empty() ? key : prefix + "." + key;
            flatten_numeric(value, next, out);
        }
        return;
    }

    if (j.is_array()) {
        for (size_t i = 0; i < j.size(); ++i) {
            const std::string next = prefix.empty() ? make_series_key(i) : prefix + "." + std::to_string(i + 1);
            flatten_numeric(j[i], next, out);
        }
    }
}

ParsedRow parse_json_object_line(const std::string& line) {
    if (line.empty() || line.front() != '{') return {};

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(line);
    } catch (...) {
        return {};
    }
    if (!json.is_object()) return {};

    ParsedRow row;
    row.fmt = LineFormat::JSONObject;
    flatten_numeric(json, "", row.fields);
    return row;
}

ParsedRow parse_json_array_line(const std::string& line) {
    if (line.empty() || line.front() != '[') return {};

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(line);
    } catch (...) {
        return {};
    }
    if (!json.is_array()) return {};

    ParsedRow row;
    row.fmt = LineFormat::JSONArray;
    for (size_t i = 0; i < json.size(); ++i) {
        if (!json[i].is_number()) continue;
        row.fields.push_back({make_series_key(i), json[i].get<double>()});
    }
    return row;
}

ParsedRow parse_csv_line(const std::string& line) {
    if (line.find(',') == std::string::npos) return {};

    const auto tokens = split_by_comma(line);
    if (tokens.size() <= 1) return {};

    ParsedRow row;
    row.fmt = LineFormat::CSV;
    row.fields.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        double value = 0.0;
        if (!try_parse_number_strict(tokens[i], value)) return {};
        row.fields.push_back({make_series_key(i), value});
    }
    return row;
}

ParsedRow parse_whitespace_line(const std::string& line) {
    const auto tokens = split_whitespace(line);
    if (tokens.size() <= 1) return {};

    ParsedRow row;
    row.fmt = LineFormat::Whitespace;
    row.fields.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        double value = 0.0;
        if (!try_parse_number_strict(tokens[i], value)) return {};
        row.fields.push_back({make_series_key(i), value});
    }
    return row;
}

ParsedRow parse_key_value_log_line(const std::string& line) {
    std::string normalized;
    normalized.reserve(line.size());
    for (char c : line) {
        switch (c) {
            case ',':
            case ';':
            case '|':
            case '\t':
                normalized.push_back(' ');
                break;
            default:
                normalized.push_back(c);
                break;
        }
    }

    const auto tokens = split_whitespace(normalized);
    if (tokens.empty()) return {};

    ParsedRow row;
    row.fmt = LineFormat::KeyValueLog;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        const size_t eq_pos = token.find('=');
        const size_t colon_pos = token.find(':');
        const size_t sep_pos = std::min(eq_pos, colon_pos);

        if (sep_pos != std::string::npos && sep_pos > 0 && sep_pos + 1 < token.size()) {
            const std::string key = clean_key(token.substr(0, sep_pos));
            double value = 0.0;
            if (!key.empty() && try_parse_number_relaxed(token.substr(sep_pos + 1), value)) {
                append_field(row.fields, key, value);
                continue;
            }
        }

        const std::string key = clean_key(token);
        if (key.empty() || i + 1 >= tokens.size()) continue;

        double value = 0.0;
        if (try_parse_number_strict(tokens[i + 1], value)) {
            append_field(row.fields, key, value);
            ++i;
        }
    }

    return row;
}

ParsedRow parse_single_value_line(const std::string& line) {
    double value = 0.0;
    if (!try_parse_number_strict(line, value)) return {};
    return ParsedRow{LineFormat::Single, {{"value", value}}};
}

}  // namespace

ParseResult detect_format(const std::string& line) {
    ParseResult result;
    const ParsedRow row = parse_line(line);
    if (row.empty()) return result;

    result.fmt = row.fmt;
    result.num_values = static_cast<int>(row.fields.size());
    result.keys.reserve(row.fields.size());
    for (const auto& field : row.fields) result.keys.push_back(field.key);
    return result;
}

ParsedRow parse_line(const std::string& line) {
    const std::string trimmed = trim(line);
    if (trimmed.empty()) return {};

    if (ParsedRow row = parse_json_object_line(trimmed); !row.empty()) return row;
    if (ParsedRow row = parse_json_array_line(trimmed); !row.empty()) return row;
    if (ParsedRow row = parse_csv_line(trimmed); !row.empty()) return row;
    if (ParsedRow row = parse_whitespace_line(trimmed); !row.empty()) return row;
    if (ParsedRow row = parse_key_value_log_line(trimmed); !row.empty()) return row;
    return parse_single_value_line(trimmed);
}
