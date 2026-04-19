use serde_json::Value;

#[derive(Debug, Clone, PartialEq)]
pub enum LineFormat {
    Single,
    Whitespace,
    Csv,
    JsonObject,
    JsonArray,
    KeyValueLog,
}

#[derive(Debug, Clone)]
pub struct ParsedField {
    pub key:   String,
    pub value: f64,
}

#[derive(Debug, Clone)]
pub struct ParsedRow {
    pub format: LineFormat,
    pub fields: Vec<ParsedField>,
}

impl ParsedRow {
    pub fn is_empty(&self) -> bool {
        self.fields.is_empty()
    }
}

// ── helpers ──────────────────────────────────────────────────────────────────

fn trim(s: &str) -> &str {
    s.trim()
}

fn series_key(index: usize) -> String {
    format!("s{}", index + 1)
}

fn is_key_char(c: char) -> bool {
    c.is_alphanumeric() || c == '_' || c == '.' || c == '-'
}

fn is_key_start(c: char) -> bool {
    c.is_alphabetic() || c == '_'
}

fn clean_key(token: &str) -> Option<String> {
    let stripped = token.trim_matches(|c: char| !is_key_char(c));
    let stripped = stripped.trim_end_matches(|c: char| !is_key_char(c));
    if stripped.is_empty() {
        return None;
    }
    let first = stripped.chars().next().unwrap();
    if !is_key_start(first) {
        return None;
    }
    Some(stripped.to_string())
}

fn try_parse_strict(token: &str) -> Option<f64> {
    let s = token.trim();
    if s.is_empty() {
        return None;
    }
    // Strip leading punctuation (but keep +/-/.)
    let mut start = 0;
    for (i, c) in s.char_indices() {
        if c.is_ascii_digit() || c == '+' || c == '-' || c == '.' {
            start = i;
            break;
        } else if c.is_ascii_punctuation() {
            continue;
        } else {
            return None;
        }
    }
    // Strip trailing punctuation
    let s = &s[start..];
    let end = s
        .rfind(|c: char| c.is_ascii_digit() || c == '.')
        .map(|i| i + 1)
        .unwrap_or(0);
    let s = &s[..end];
    if s.is_empty() {
        return None;
    }
    s.parse::<f64>().ok().filter(|v| v.is_finite())
}

fn try_parse_relaxed(token: &str) -> Option<f64> {
    let s = token.trim();
    if s.is_empty() {
        return None;
    }
    // Drop leading non-numeric chars, keeping +/-/.
    let start = s.find(|c: char| c.is_ascii_digit() || c == '+' || c == '-' || c == '.')?;
    let s = &s[start..];
    let end = s
        .rfind(|c: char| c.is_ascii_digit() || c == '.')
        .map(|i| i + 1)
        .unwrap_or(0);
    let s = &s[..end];
    if s.is_empty() {
        return None;
    }
    s.parse::<f64>().ok().filter(|v| v.is_finite())
}

fn append_field(fields: &mut Vec<ParsedField>, key: String, value: f64) {
    if key.is_empty() {
        return;
    }
    for f in fields.iter_mut() {
        if f.key == key {
            f.value = value;
            return;
        }
    }
    fields.push(ParsedField { key, value });
}

fn flatten_numeric(j: &Value, prefix: &str, out: &mut Vec<ParsedField>) {
    match j {
        Value::Number(n) => {
            if let Some(v) = n.as_f64() {
                append_field(out, prefix.to_string(), v);
            }
        }
        Value::Object(map) => {
            for (k, v) in map {
                let next = if prefix.is_empty() {
                    k.clone()
                } else {
                    format!("{}.{}", prefix, k)
                };
                flatten_numeric(v, &next, out);
            }
        }
        Value::Array(arr) => {
            for (i, v) in arr.iter().enumerate() {
                let next = if prefix.is_empty() {
                    series_key(i)
                } else {
                    format!("{}.{}", prefix, i + 1)
                };
                flatten_numeric(v, &next, out);
            }
        }
        _ => {}
    }
}

// ── strategies ───────────────────────────────────────────────────────────────

fn parse_json_object(line: &str) -> Option<ParsedRow> {
    if !line.starts_with('{') {
        return None;
    }
    let j: Value = serde_json::from_str(line).ok()?;
    if !j.is_object() {
        return None;
    }
    let mut fields = Vec::new();
    flatten_numeric(&j, "", &mut fields);
    if fields.is_empty() {
        return None;
    }
    Some(ParsedRow { format: LineFormat::JsonObject, fields })
}

fn parse_json_array(line: &str) -> Option<ParsedRow> {
    if !line.starts_with('[') {
        return None;
    }
    let j: Value = serde_json::from_str(line).ok()?;
    let arr = j.as_array()?;
    let mut fields: Vec<ParsedField> = arr
        .iter()
        .enumerate()
        .filter_map(|(i, v)| v.as_f64().map(|n| ParsedField { key: series_key(i), value: n }))
        .collect();
    if fields.is_empty() {
        return None;
    }
    // Re-number to match C++ (s1, s2, …)
    for (i, f) in fields.iter_mut().enumerate() {
        f.key = series_key(i);
    }
    Some(ParsedRow { format: LineFormat::JsonArray, fields })
}

fn parse_csv(line: &str) -> Option<ParsedRow> {
    if !line.contains(',') {
        return None;
    }
    let tokens: Vec<&str> = line.split(',').map(|t| t.trim()).collect();
    if tokens.len() <= 1 {
        return None;
    }
    let fields: Vec<ParsedField> = tokens
        .iter()
        .enumerate()
        .map(|(i, t)| try_parse_strict(t).map(|v| ParsedField { key: series_key(i), value: v }))
        .collect::<Option<Vec<_>>>()?;
    Some(ParsedRow { format: LineFormat::Csv, fields })
}

fn parse_whitespace(line: &str) -> Option<ParsedRow> {
    let tokens: Vec<&str> = line.split_whitespace().collect();
    if tokens.len() <= 1 {
        return None;
    }
    let fields: Vec<ParsedField> = tokens
        .iter()
        .enumerate()
        .map(|(i, t)| try_parse_strict(t).map(|v| ParsedField { key: series_key(i), value: v }))
        .collect::<Option<Vec<_>>>()?;
    Some(ParsedRow { format: LineFormat::Whitespace, fields })
}

fn parse_kv_log(line: &str) -> Option<ParsedRow> {
    // Normalize separators to space
    let normalized: String = line
        .chars()
        .map(|c| match c {
            ',' | ';' | '|' | '\t' => ' ',
            other => other,
        })
        .collect();

    let tokens: Vec<&str> = normalized.split_whitespace().collect();
    if tokens.is_empty() {
        return None;
    }

    let mut fields: Vec<ParsedField> = Vec::new();
    let mut i = 0;
    while i < tokens.len() {
        let token = tokens[i];

        // Try key=value or key:value
        let sep_pos = token
            .find('=')
            .map(|p| (p, '='))
            .into_iter()
            .chain(token.find(':').map(|p| (p, ':')).into_iter())
            .min_by_key(|(p, _)| *p);

        if let Some((pos, _)) = sep_pos {
            if pos > 0 && pos + 1 < token.len() {
                let k = clean_key(&token[..pos]);
                let v = try_parse_relaxed(&token[pos + 1..]);
                if let (Some(key), Some(val)) = (k, v) {
                    append_field(&mut fields, key, val);
                    i += 1;
                    continue;
                }
            }
        }

        // Try: key followed by next-token value
        if let Some(key) = clean_key(token) {
            if i + 1 < tokens.len() {
                if let Some(val) = try_parse_strict(tokens[i + 1]) {
                    append_field(&mut fields, key, val);
                    i += 2;
                    continue;
                }
            }
        }

        i += 1;
    }

    if fields.is_empty() {
        return None;
    }
    Some(ParsedRow { format: LineFormat::KeyValueLog, fields })
}

fn parse_single(line: &str) -> Option<ParsedRow> {
    let v = try_parse_strict(line)?;
    Some(ParsedRow {
        format: LineFormat::Single,
        fields: vec![ParsedField { key: "value".into(), value: v }],
    })
}

// ── public API ───────────────────────────────────────────────────────────────

pub fn parse_line(line: &str) -> Option<ParsedRow> {
    let line = trim(line);
    if line.is_empty() {
        return None;
    }

    parse_json_object(line)
        .or_else(|| parse_json_array(line))
        .or_else(|| parse_csv(line))
        .or_else(|| parse_whitespace(line))
        .or_else(|| parse_kv_log(line))
        .or_else(|| parse_single(line))
}

/// Remove fields whose keys don't match the filter.
pub fn filter_fields(mut row: ParsedRow, filter: Option<&regex::Regex>) -> ParsedRow {
    if let Some(re) = filter {
        row.fields.retain(|f| re.is_match(&f.key));
    }
    row
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_single() {
        let r = parse_line("42.5").unwrap();
        assert_eq!(r.format, LineFormat::Single);
        assert_eq!(r.fields[0].value, 42.5);
    }

    #[test]
    fn test_whitespace() {
        let r = parse_line("1 2 3").unwrap();
        assert_eq!(r.format, LineFormat::Whitespace);
        assert_eq!(r.fields.len(), 3);
    }

    #[test]
    fn test_csv() {
        let r = parse_line("1.0, 2.0, 3.0").unwrap();
        assert_eq!(r.format, LineFormat::Csv);
        assert_eq!(r.fields.len(), 3);
    }

    #[test]
    fn test_json_object() {
        let r = parse_line(r#"{"cpu": 72.3, "mem": 4.2}"#).unwrap();
        assert_eq!(r.format, LineFormat::JsonObject);
        assert_eq!(r.fields.len(), 2);
    }

    #[test]
    fn test_json_array() {
        let r = parse_line("[1, 2, 3]").unwrap();
        assert_eq!(r.format, LineFormat::JsonArray);
        assert_eq!(r.fields.len(), 3);
        assert_eq!(r.fields[0].key, "s1");
    }

    #[test]
    fn test_kv_log() {
        let r = parse_line("a=3 b:4 c 5").unwrap();
        assert_eq!(r.format, LineFormat::KeyValueLog);
        let keys: Vec<&str> = r.fields.iter().map(|f| f.key.as_str()).collect();
        assert!(keys.contains(&"a"));
        assert!(keys.contains(&"b"));
        assert!(keys.contains(&"c"));
    }

    #[test]
    fn test_empty() {
        assert!(parse_line("").is_none());
        assert!(parse_line("   ").is_none());
    }

    #[test]
    fn test_nested_json() {
        let r = parse_line(r#"{"a": {"b": 1, "c": 2}}"#).unwrap();
        let keys: Vec<&str> = r.fields.iter().map(|f| f.key.as_str()).collect();
        assert!(keys.contains(&"a.b"));
        assert!(keys.contains(&"a.c"));
    }
}
