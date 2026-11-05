#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace json {

namespace {
const Value& null_value() {
    static const Value v;
    return v;
}
const std::string& empty_string() {
    static const std::string s;
    return s;
}

void append_utf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xc0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xe0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (cp & 0x3f));
    }
}

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string error;
    int depth = 0;

    explicit Parser(const std::string& text) : s(text) {}

    bool fail(const char* what) {
        if (error.empty()) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "%s at byte %zu", what, i);
            error = buf;
        }
        return false;
    }
    void ws() {
        while (i < s.size()) {
            const char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++i;
            } else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
                // comments in hand-edited config files are forgiven
                while (i < s.size() && s[i] != '\n') ++i;
            } else {
                break;
            }
        }
    }
    bool literal(const char* word) {
        size_t n = 0;
        while (word[n]) ++n;
        if (s.compare(i, n, word) != 0) return fail("unexpected text");
        i += n;
        return true;
    }
    bool hex4(uint32_t& out) {
        if (i + 4 > s.size()) return fail("short \\u escape");
        out = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s[i++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<uint32_t>(c - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool string(std::string& out) {
        if (i >= s.size() || s[i] != '"') return fail("expected a string");
        ++i;
        while (i < s.size()) {
            const char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i >= s.size()) break;
            const char e = s[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xd800 && cp < 0xdc00 && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                        i += 2;
                        uint32_t lo = 0;
                        if (!hex4(lo)) return false;
                        if (lo >= 0xdc00 && lo < 0xe000) cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool number(Value& out) {
        const size_t start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '-' || s[i] == '+')) ++i;
        if (i == start) return fail("expected a value");
        const std::string text = s.substr(start, i - start);
        char* end = nullptr;
        const double d = std::strtod(text.c_str(), &end);
        if (!end || *end != '\0') return fail("bad number");
        out = Value(d);
        return true;
    }
    bool value(Value& out) {
        if (++depth > 64) return fail("nested too deep");
        ws();
        if (i >= s.size()) return fail("unexpected end");
        bool ok = true;
        const char c = s[i];
        if (c == '{') {
            ++i;
            out = Value::object();
            ws();
            if (i < s.size() && s[i] == '}') {
                ++i;
            } else {
                while (true) {
                    ws();
                    std::string key;
                    if (!string(key)) { ok = false; break; }
                    ws();
                    if (i >= s.size() || s[i] != ':') { ok = fail("expected ':'"); break; }
                    ++i;
                    Value v;
                    if (!value(v)) { ok = false; break; }
                    out.set(key, std::move(v));
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; ws(); if (i < s.size() && s[i] == '}') { ++i; break; } continue; }
                    if (i < s.size() && s[i] == '}') { ++i; break; }
                    ok = fail("expected ',' or '}'");
                    break;
                }
            }
        } else if (c == '[') {
            ++i;
            out = Value::array();
            ws();
            if (i < s.size() && s[i] == ']') {
                ++i;
            } else {
                while (true) {
                    Value v;
                    if (!value(v)) { ok = false; break; }
                    out.push(std::move(v));
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; ws(); if (i < s.size() && s[i] == ']') { ++i; break; } continue; }
                    if (i < s.size() && s[i] == ']') { ++i; break; }
                    ok = fail("expected ',' or ']'");
                    break;
                }
            }
        } else if (c == '"') {
            std::string str;
            ok = string(str);
            out = Value(std::move(str));
        } else if (c == 't') {
            ok = literal("true");
            out = Value(true);
        } else if (c == 'f') {
            ok = literal("false");
            out = Value(false);
        } else if (c == 'n') {
            ok = literal("null");
            out = Value();
        } else {
            ok = number(out);
        }
        --depth;
        return ok;
    }
};
}  // namespace

const std::string& Value::as_string() const { return type_ == Type::String ? str_ : empty_string(); }

const Value& Value::operator[](size_t i) const {
    if (type_ != Type::Array || i >= items_.size()) return null_value();
    return items_[i];
}

Value& Value::push(Value v) {
    if (type_ != Type::Array) *this = array();
    items_.push_back(std::move(v));
    return items_.back();
}

const Value& Value::get(const std::string& key) const {
    if (type_ == Type::Object) {
        for (const auto& m : members_) {
            if (m.first == key) return m.second;
        }
    }
    return null_value();
}

bool Value::has(const std::string& key) const {
    if (type_ != Type::Object) return false;
    for (const auto& m : members_) {
        if (m.first == key) return true;
    }
    return false;
}

Value& Value::set(const std::string& key, Value v) {
    if (type_ != Type::Object) *this = object();
    for (auto& m : members_) {
        if (m.first == key) {
            m.second = std::move(v);
            return m.second;
        }
    }
    members_.emplace_back(key, std::move(v));
    return members_.back().second;
}

void escape_to(std::string& out, const std::string& s) {
    out += '"';
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    out += '"';
}

void Value::write(std::string& out, bool pretty, int indent) const {
    auto newline = [&](int level) {
        if (!pretty) return;
        out += '\n';
        out.append(static_cast<size_t>(level) * 2, ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: {
            if (!std::isfinite(num_)) {
                out += "0";
            } else if (num_ == std::floor(num_) && std::fabs(num_) < 9e15) {
                char buf[32];
                std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(num_));
                out += buf;
            } else {
                char buf[32];
                std::snprintf(buf, sizeof buf, "%.6g", num_);
                out += buf;
            }
            break;
        }
        case Type::String: escape_to(out, str_); break;
        case Type::Array: {
            out += '[';
            // short arrays of plain values stay on one line
            bool flat = true;
            for (const auto& v : items_) flat = flat && !v.is_array() && !v.is_object();
            for (size_t k = 0; k < items_.size(); ++k) {
                if (k) out += pretty && flat ? ", " : ",";
                if (!flat) newline(indent + 1);
                items_[k].write(out, pretty, indent + 1);
            }
            if (!flat && !items_.empty()) newline(indent);
            out += ']';
            break;
        }
        case Type::Object: {
            out += '{';
            for (size_t k = 0; k < members_.size(); ++k) {
                if (k) out += ',';
                newline(indent + 1);
                escape_to(out, members_[k].first);
                out += pretty ? ": " : ":";
                members_[k].second.write(out, pretty, indent + 1);
            }
            if (!members_.empty()) newline(indent);
            out += '}';
            break;
        }
    }
}

std::string Value::dump(bool pretty) const {
    std::string out;
    write(out, pretty, 0);
    if (pretty) out += '\n';
    return out;
}

Value parse(const std::string& text, bool* ok, std::string* error) {
    Parser p(text);
    // a UTF-8 byte order mark (Notepad) is skipped
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef && static_cast<unsigned char>(text[1]) == 0xbb &&
        static_cast<unsigned char>(text[2]) == 0xbf)
        p.i = 3;
    Value v;
    bool good = p.value(v);
    if (good) {
        p.ws();
        if (p.i != text.size()) good = p.fail("text after the value");
    }
    if (ok) *ok = good;
    if (error) *error = p.error;
    return good ? v : Value();
}

}  // namespace json
