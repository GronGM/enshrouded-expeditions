// Text for the shroudlink MODS page. The page sizes list columns and buttons at 13.7 UI units
// a character, which fits Latin text; the game's Cyrillic letters are 16-21 units wide, so
// cells ran into each other and labels out of their buttons. These helpers pad cells and
// labels with spaces to make up the difference and break long texts into lines that fit.
#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace exped::page {

std::vector<char32_t> code_points(const std::string& s);
// Width of a text in the page's text style, in UI units (measured on the game's font).
double text_units(const std::string& s);
// A list cell with trailing spaces, so that its column is as wide as the text really is.
std::string pad_cell(const std::string& s);
// A button label with spaces on both sides: as wide as the text, the text in the middle.
std::string pad_label(const std::string& s);
// Tab-separated list cells, all padded, with an empty cell at the end (lines them up).
std::string cells(std::initializer_list<std::string> parts);
// Lines that fit the page, broken at spaces.
std::vector<std::string> wrap_lines(const std::string& text);
// "Высокогорье кочевников (Nomad Highlands)" -> "Высокогорье кочевников"
std::string short_region(const std::string& name);

// Plain text (the EMF build): no padding and no line breaks, the window lays text out itself.
void set_plain(bool plain);
bool plain();

}  // namespace exped::page
