#include "page_text.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace exped::page {

// The MODS page sizes list columns and buttons at 13.7 UI units a character, which fits
// Latin text; the game's Cyrillic letters are 16-21 units wide, so cells ran into each other
// and labels out of their buttons. Cells and labels get spaces to make up the difference.
constexpr double kPageGlyph = 13.7;
constexpr double kSpaceUnits = 8;
// text lines are broken here (the page is about 1350 units wide)
constexpr double kLineUnits = 1200;

std::vector<char32_t> code_points(const std::string& s) {
    std::vector<char32_t> out;
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        char32_t cp = extra == 3 ? (c & 0x07) : extra == 2 ? (c & 0x0F) : extra == 1 ? (c & 0x1F) : c;
        ++i;
        for (; extra > 0 && i < s.size(); --extra, ++i) cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        out.push_back(cp);
    }
    return out;
}

// Width of a letter of the page's text style, in UI units (measured on the game's font).
static double glyph_units(char32_t c) {
    if (c == U' ') return kSpaceUnits;
    if (c >= 0x400 && c <= 0x4FF) {
        // measured on screenshots of the MODS page: most lowercase letters 15-17 units
        const bool upper = (c >= 0x410 && c <= 0x42F) || c == 0x401;
        for (char32_t w : std::u32string_view(U"ЖжШшЩщЫыЮюМмФф")) {
            if (c == w) return upper ? 21 : 20;
        }
        return upper ? 17.5 : 16;
    }
    if (c >= U'A' && c <= U'Z') return c == U'M' || c == U'W' ? 20 : 16;
    if (c >= U'a' && c <= U'z') {
        if (c == U'm' || c == U'w') return 19;
        if (c == U'i' || c == U'l' || c == U'j' || c == U't' || c == U'f' || c == U'r') return 8;
        return 13;
    }
    if (c >= U'0' && c <= U'9') return 12;
    if (c == U'.' || c == U',' || c == U':' || c == U';' || c == U'!' || c == U'\'' || c == U'|') return 7;
    if (c == U'(' || c == U')' || c == U'-' || c == U'[' || c == U']') return 9;
    return 15;
}

double text_units(const std::string& s) {
    double w = 0;
    for (char32_t c : code_points(s)) w += glyph_units(c);
    return w;
}

// A list cell with trailing spaces, so that its column is as wide as the text really is.
std::string pad_cell(const std::string& s) {
    const double chars = static_cast<double>(code_points(s).size());
    const double need = std::ceil((text_units(s) + 4) / kPageGlyph) - chars;
    return need > 0 ? s + std::string(static_cast<size_t>(need), ' ') : s;
}

// A button label with spaces on both sides: the button gets as wide as the text and the
// text stays in its middle.
std::string pad_label(const std::string& s) {
    const double chars = static_cast<double>(code_points(s).size());
    const double extra = (text_units(s) - chars * kPageGlyph) / (2 * kPageGlyph - 2 * kSpaceUnits);
    const auto k = static_cast<size_t>(std::max(0.0, std::round(extra)));
    return std::string(k, ' ') + s + std::string(k, ' ');
}

// Tab-separated list cells, each padded, and an empty one at the end: the page draws the
// last cell of a row outside the column frames, a few pixels lower than the others.
std::string cells(std::initializer_list<std::string> parts) {
    std::string out;
    for (const auto& p : parts) out += pad_cell(p) + '\t';
    return out;
}

// Lines of at most kLineUnits, broken at spaces.
std::vector<std::string> wrap_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    double width = 0;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find(' ', at);
        if (end == std::string::npos) end = text.size();
        const std::string word = text.substr(at, end - at);
        const double w = text_units(word);
        if (!line.empty() && width + kSpaceUnits + w > kLineUnits) {
            lines.push_back(line);
            line.clear();
            width = 0;
        }
        if (!line.empty()) {
            line += ' ';
            width += kSpaceUnits;
        }
        line += word;
        width += w;
        at = end + 1;
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

// "Высокогорье кочевников (Nomad Highlands)" -> "Высокогорье кочевников"
std::string short_region(const std::string& name) {
    const size_t open = name.rfind(" (");
    if (open == std::string::npos || name.empty() || name.back() != ')') return name;
    for (size_t i = open + 2; i + 1 < name.size(); ++i) {
        if (static_cast<unsigned char>(name[i]) >= 0x80) return name; // not a Latin aside
    }
    return name.substr(0, open);
}

}  // namespace exped::page
