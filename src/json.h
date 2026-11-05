// A small JSON value with a parser and a writer (UTF-8), enough for shroudlink messages
// and the mod's files. No exceptions: a parse error gives a null value and `ok == false`.
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type_(Type::Bool), bool_(b) {}
    Value(int n) : type_(Type::Number), num_(n) {}
    Value(unsigned n) : type_(Type::Number), num_(n) {}
    Value(int64_t n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Value(uint64_t n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Value(double n) : type_(Type::Number), num_(n) {}
    Value(const char* s) : type_(Type::String), str_(s) {}
    Value(std::string s) : type_(Type::String), str_(std::move(s)) {}

    static Value array() { Value v; v.type_ = Type::Array; return v; }
    static Value object() { Value v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
    double as_number(double fallback = 0) const { return type_ == Type::Number ? num_ : fallback; }
    const std::string& as_string() const;
    std::string string_or(const std::string& fallback) const { return type_ == Type::String ? str_ : fallback; }

    // arrays
    size_t size() const { return type_ == Type::Array ? items_.size() : type_ == Type::Object ? members_.size() : 0; }
    const Value& operator[](size_t i) const;
    const Value& operator[](int i) const { return (*this)[static_cast<size_t>(i < 0 ? 0 : i)]; }
    Value& push(Value v);
    const std::vector<Value>& items() const { return items_; }

    // objects (member order is kept)
    const Value& operator[](const char* key) const { return get(key); }
    const Value& get(const std::string& key) const;
    bool has(const std::string& key) const;
    Value& set(const std::string& key, Value v);
    const std::vector<std::pair<std::string, Value>>& members() const { return members_; }

    std::string dump(bool pretty = false) const;

private:
    void write(std::string& out, bool pretty, int indent) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Value> items_;
    std::vector<std::pair<std::string, Value>> members_;
};

// Parses UTF-8 JSON text; on an error `ok` is false and the result is null.
Value parse(const std::string& text, bool* ok = nullptr, std::string* error = nullptr);

void escape_to(std::string& out, const std::string& s);

}  // namespace json
