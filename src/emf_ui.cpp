#include "emf_ui.h"

#include "lang.h"
#include "page_text.h"

namespace exped::emfui {
namespace {

using lang::L;

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

void put_utf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}

// A list entry: tab-separated cells -> "First (second · third)"; a leading "»" marks the chosen one.
std::string row_text(const std::string& entry, bool& selected) {
    std::vector<std::string> cells;
    size_t at = 0;
    while (at <= entry.size()) {
        size_t end = entry.find('\t', at);
        if (end == std::string::npos) end = entry.size();
        const std::string c = trim(entry.substr(at, end - at));
        if (!c.empty()) cells.push_back(c);
        at = end + 1;
    }
    selected = false;
    if (cells.empty()) return "";
    const std::string mark = "\xC2\xBB";  // »
    if (cells[0].rfind(mark, 0) == 0) {
        selected = true;
        cells[0] = trim(cells[0].substr(mark.size()));
    }
    std::string text = cells[0];
    if (cells.size() > 1) {
        text += " (";
        for (size_t i = 1; i < cells.size(); ++i) text += (i > 1 ? " \xC2\xB7 " : "") + cells[i];  // " · "
        text += ")";
    }
    return text;
}

bool interactive(const std::string& type) { return type == "button" || type == "toggle" || type == "tabs" || type == "list"; }

void add(Window& w, uint32_t type, uint32_t flags, std::string key, const std::string& text) {
    Widget x;
    x.type = type;
    x.flags = flags;
    x.key = std::move(key);
    x.text = drawable(text);
    w.widgets.push_back(std::move(x));
}

void node(Window& w, const json::Value& n, uint32_t first_flags, const ItemNames& names) {
    const std::string type = n["type"].as_string();
    const std::string id = n["id"].as_string();
    if (type == "text") {
        const std::string t = trim(n["text"].as_string());
        if (!t.empty()) add(w, Text, first_flags, "", t);
    } else if (type == "heading") {
        add(w, Heading, first_flags, "", trim(n["text"].as_string()));
    } else if (type == "separator") {
        add(w, Separator, 0, "", "");
    } else if (type == "button") {
        add(w, Button, first_flags, "b:" + id, trim(n["label"].as_string()));
    } else if (type == "toggle") {
        const bool on = n["value"].as_bool();
        w.toggles[id] = on;
        add(w, Row, first_flags | (on ? Selected : 0u), "g:" + id, trim(n["label"].as_string()) + " (" + (on ? L("вкл", "on") : L("выкл", "off")) + ")");
    } else if (type == "tabs") {
        const int selected = static_cast<int>(n["selected"].as_number(-1));
        int i = 0;
        for (const auto& t : n["tabs"].items()) {
            add(w, Button, (i == 0 ? first_flags : SameLine) | (i == selected ? Selected : 0u), "t:" + id + ":" + std::to_string(i), trim(t.as_string()));
            ++i;
        }
    } else if (type == "list") {
        int i = 0;
        for (const auto& e : n["entries"].items()) {
            bool selected = false;
            const std::string t = row_text(e.as_string(), selected);
            add(w, Row, selected ? Selected : 0u, "l:" + id + ":" + std::to_string(i), t);
            ++i;
        }
    } else if (type == "item_grid") {
        for (const auto& it : n["items"].items()) {
            const auto item = static_cast<uint32_t>(it["id"].as_number());
            const auto count = static_cast<uint32_t>(it["count"].as_number());
            add(w, Row, 0, "", (names ? names(item) : std::to_string(item)) + " (" + std::to_string(count) + ")");
        }
    } else if (type == "row") {
        const auto& children = n["children"].items();
        bool clickable = false;
        for (const auto& c : children) clickable = clickable || interactive(c["type"].as_string());
        if (!clickable) {
            // a line of text: "Label: value   Label: value"; headings alone are a caption
            std::string line;
            bool only_headings = true;
            std::string last;
            for (const auto& c : children) {
                const std::string t = trim(c["text"].as_string());
                const std::string ct = c["type"].as_string();
                if (t.empty()) {
                    if (!line.empty()) line += "   ";
                    last = "gap";
                    continue;
                }
                if (ct == "heading") {
                    if (!line.empty() && last != "gap") line += "   ";
                    line += t;
                } else {
                    only_headings = false;
                    line += last == "heading" ? ": " + t : (line.empty() || last == "gap" ? t : " " + t);
                }
                last = ct;
            }
            line = trim(line);
            if (!line.empty()) add(w, Text, only_headings ? Dim : 0u, "", line);
            return;
        }
        bool first = true;
        for (const auto& c : children) {
            const std::string ct = c["type"].as_string();
            if (ct == "heading") {  // a label on a line with buttons
                const std::string t = trim(c["text"].as_string());
                if (!t.empty()) add(w, Text, first ? 0u : SameLine, "", t);
            } else {
                node(w, c, first ? 0u : SameLine, names);
            }
            first = false;
        }
    }
}

std::string event_json(const std::string& node_id, const std::string& kind, int index, int value) {
    json::Value e = json::Value::object();
    e.set("kind", kind);
    if (index >= 0) e.set("index", index);
    if (value >= 0) e.set("value", value != 0);
    json::Value ev = json::Value::object();
    ev.set("node", node_id);
    ev.set("event", e);
    json::Value m = json::Value::object();
    m.set("op", "ui_event");
    m.set("event", ev);
    return m.dump();
}

}  // namespace

std::string drawable(const std::string& text, size_t max_bytes) {
    std::string out;
    for (char32_t c : page::code_points(text)) {
        std::string piece;
        if (c == U'\t') piece = " ";
        else if (c < 0x20) continue;
        else if (c <= 0x17F || (c >= 0x400 && c <= 0x52F)) put_utf8(piece, c);
        else if (c == 0x2013 || c == 0x2014 || c == 0x2212) piece = "-";
        else if (c == 0x2018 || c == 0x2019) piece = "'";
        else if (c == 0x201C || c == 0x201D) piece = "\"";
        else if (c == 0x2026) piece = "...";
        else if (c == 0x2022) piece = "\xC2\xB7";
        else if (c == 0x00A0 || c == 0x2009 || c == 0x202F) piece = " ";
        else piece = "?";
        if (out.size() + piece.size() > max_bytes) break;
        out += piece;
    }
    return out;
}

Window translate(const json::Value& window, const ItemNames& names) {
    Window w;
    w.title = drawable(trim(window["title"].as_string()), 64);
    for (const auto& n : window["content"].items()) node(w, n, 0, names);
    // at most 256 widgets in an EMF window
    if (w.widgets.size() > 256) w.widgets.resize(256);
    return w;
}

std::string click_event(const std::string& key, const Window& shown) {
    if (key.size() < 3 || key[1] != ':') return "";
    const std::string rest = key.substr(2);
    if (key[0] == 'b') return event_json(rest, "click", -1, -1);
    if (key[0] == 'g') {
        const auto it = shown.toggles.find(rest);
        const bool now = it != shown.toggles.end() && it->second;
        return event_json(rest, "change", -1, now ? 0 : 1);
    }
    if (key[0] == 'l' || key[0] == 't') {
        const size_t colon = rest.rfind(':');
        if (colon == std::string::npos) return "";
        const std::string id = rest.substr(0, colon);
        const std::string num = rest.substr(colon + 1);
        if (num.empty() || num.find_first_not_of("0123456789") != std::string::npos || num.size() > 6) return "";
        return event_json(id, "click", std::stoi(num), -1);
    }
    return "";
}

std::string menu_event() { return event_json("open", "menu", -1, -1); }
std::string closed_event() { return event_json("", "closed", -1, -1); }

std::vector<std::string> hud_lines(const json::Value& hud) {
    std::vector<std::string> lines;
    const auto& rows = hud["rows"].items();
    if (rows.empty()) return lines;
    lines.push_back(drawable(trim(hud["title"].as_string()), 160));
    for (const auto& r : rows) {
        std::string line = trim(r["label"].as_string());
        const std::string value = trim(r["value"].as_string());
        if (!value.empty()) line += " \xC2\xB7 " + value;
        lines.push_back(drawable(line, 160));
    }
    return lines;
}

}  // namespace exped::emfui
