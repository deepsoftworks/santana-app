#include "parser.hpp"
#include <cstdlib>
#include <cctype>
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

ParseResult detect_format(const std::string& line) {
    std::string t = trim(line);
    if (t.empty()) return {};

    // Reserved for JSON (F2)
    if (t[0] == '{' || t[0] == '[') return {};

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
    return {};  // Single, num_values=1, no keys
}

std::vector<double> parse_line(const std::string& line, LineFormat fmt) {
    std::string t = trim(line);
    if (t.empty()) return {};

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
