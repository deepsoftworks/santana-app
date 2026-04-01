#include "parser.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <sstream>

static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

static bool is_numeric(const std::string& s) {
    if (s.empty()) return false;
    char* end;
    std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

static std::vector<std::string> split_by_comma(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ','))
        tokens.push_back(trim(token));
    return tokens;
}

static std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string token;
    while (ss >> token)
        tokens.push_back(token);
    return tokens;
}

// Recursively collect numeric leaves with dot-notation keys.
static void flatten_numeric(const nlohmann::json& j, const std::string& prefix,
                             std::vector<std::pair<std::string, double>>& out) {
    if (j.is_number()) {
        out.push_back({prefix, j.get<double>()});
    } else if (j.is_object()) {
        for (auto& [k, v] : j.items()) {
            std::string key = prefix.empty() ? k : prefix + "." + k;
            flatten_numeric(v, key, out);
        }
    }
}

// Navigate a dot-path like ".metrics.latency" through a JSON object.
// Returns nullopt if path not found or value not numeric.
static std::optional<double> navigate_jq(const nlohmann::json& j, const std::string& path) {
    std::string p = path;
    if (!p.empty() && p[0] == '.') p = p.substr(1);
    if (p.empty()) {
        if (j.is_number()) return j.get<double>();
        return std::nullopt;
    }
    const nlohmann::json* cur = &j;
    std::istringstream ss(p);
    std::string key;
    while (std::getline(ss, key, '.')) {
        if (!cur->is_object() || !cur->contains(key)) return std::nullopt;
        cur = &(*cur)[key];
    }
    if (cur->is_number()) return cur->get<double>();
    return std::nullopt;
}

// ── detect_format ────────────────────────────────────────────────────────────

ParseResult detect_format(const std::string& line) {
    std::string t = trim(line);
    if (t.empty()) return {};

    // JSON object
    if (t[0] == '{') {
        try {
            auto j = nlohmann::json::parse(t);
            std::vector<std::pair<std::string, double>> leaves;
            flatten_numeric(j, "", leaves);
            if (!leaves.empty()) {
                ParseResult r;
                r.fmt = LineFormat::JSONObject;
                r.num_values = static_cast<int>(leaves.size());
                for (auto& [k, _] : leaves) r.keys.push_back(k);
                return r;
            }
        } catch (...) {}
        return {};  // unparseable or no numeric fields
    }

    // JSON array
    if (t[0] == '[') {
        try {
            auto j = nlohmann::json::parse(t);
            int count = 0;
            for (auto& el : j)
                if (el.is_number()) ++count;
            if (count > 0) {
                ParseResult r;
                r.fmt = LineFormat::JSONArray;
                r.num_values = count;
                for (int i = 0; i < count; ++i)
                    r.keys.push_back("s" + std::to_string(i + 1));
                return r;
            }
        } catch (...) {}
        return {};
    }

    // CSV: has commas and all tokens are numeric
    if (t.find(',') != std::string::npos) {
        auto tokens = split_by_comma(t);
        if (tokens.size() > 1) {
            bool all_numeric = true;
            for (auto& tok : tokens)
                if (!is_numeric(tok)) { all_numeric = false; break; }
            if (all_numeric) {
                ParseResult r;
                r.fmt = LineFormat::CSV;
                r.num_values = static_cast<int>(tokens.size());
                for (int i = 0; i < r.num_values; ++i)
                    r.keys.push_back("s" + std::to_string(i + 1));
                return r;
            }
        }
    }

    // Whitespace-separated: multiple numeric tokens
    auto tokens = split_whitespace(t);
    if (tokens.size() > 1) {
        bool all_numeric = true;
        for (auto& tok : tokens)
            if (!is_numeric(tok)) { all_numeric = false; break; }
        if (all_numeric) {
            ParseResult r;
            r.fmt = LineFormat::Whitespace;
            r.num_values = static_cast<int>(tokens.size());
            for (int i = 0; i < r.num_values; ++i)
                r.keys.push_back("s" + std::to_string(i + 1));
            return r;
        }
    }

    // Default: single value
    return {};
}

// ── parse_line ───────────────────────────────────────────────────────────────

std::vector<double> parse_line(const std::string& line, LineFormat fmt,
                               const std::vector<std::string>& field_selectors,
                               const std::string& jq_path) {
    std::string t = trim(line);
    if (t.empty()) return {};

    if (fmt == LineFormat::JSONObject) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(t); } catch (...) { return {}; }

        if (!jq_path.empty()) {
            auto v = navigate_jq(j, jq_path);
            return v ? std::vector<double>{*v} : std::vector<double>{};
        }

        if (!field_selectors.empty()) {
            std::vector<double> vals;
            vals.reserve(field_selectors.size());
            for (auto& key : field_selectors) {
                auto v = navigate_jq(j, "." + key);
                if (v) vals.push_back(*v);
            }
            return vals;
        }

        // No selectors: flatten all numeric leaves
        std::vector<std::pair<std::string, double>> leaves;
        flatten_numeric(j, "", leaves);
        std::vector<double> vals;
        vals.reserve(leaves.size());
        for (auto& [_, v] : leaves) vals.push_back(v);
        return vals;
    }

    if (fmt == LineFormat::JSONArray) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(t); } catch (...) { return {}; }
        std::vector<double> vals;
        for (auto& el : j)
            if (el.is_number()) vals.push_back(el.get<double>());
        return vals;
    }

    if (fmt == LineFormat::CSV) {
        auto tokens = split_by_comma(t);
        std::vector<double> vals;
        vals.reserve(tokens.size());
        for (auto& tok : tokens) {
            char* end;
            double v = std::strtod(tok.c_str(), &end);
            if (end != tok.c_str()) vals.push_back(v);
        }
        return vals;
    }

    if (fmt == LineFormat::Whitespace) {
        auto tokens = split_whitespace(t);
        std::vector<double> vals;
        vals.reserve(tokens.size());
        for (auto& tok : tokens) {
            char* end;
            double v = std::strtod(tok.c_str(), &end);
            if (end != tok.c_str()) vals.push_back(v);
        }
        return vals;
    }

    // Single
    char* end;
    double v = std::strtod(t.c_str(), &end);
    if (end != t.c_str()) return {v};
    return {};
}
