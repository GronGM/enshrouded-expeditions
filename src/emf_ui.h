// The EMF build's windows: the mod describes its windows for shroudlink's MODS page (JSON nodes:
// text, heading, row, list, tabs, toggle, button, item_grid); these turn such a description into
// the widgets of an EMF mod window, and an EMF click back into the event the mod expects.
// No EMF headers here, so the translation is tested on its own.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "json.h"

namespace exped::emfui {

// the same numbers as EMF_UI_* in emf_api.h
constexpr uint32_t Text = 0, Heading = 1, Button = 2, Row = 3, Slot = 4, Bar = 5, Separator = 6;
constexpr uint32_t SameLine = 0x01, Disabled = 0x02, Selected = 0x04, Dim = 0x08;

struct Widget {
    uint32_t type = Text;
    uint32_t flags = 0;
    std::string key;  // "" = no events
    std::string text;
};

struct Window {
    std::string title;
    std::vector<Widget> widgets;
    std::map<std::string, bool> toggles;  // toggle id -> its value as shown
};

// item id -> the name shown for it
using ItemNames = std::function<std::string(uint32_t)>;

// The "window" object of the mod's "open" message.
Window translate(const json::Value& window, const ItemNames& names);

// The mod's ui_event message for a click on the widget `key` of the window `shown` ("" for none).
std::string click_event(const std::string& key, const Window& shown);
// The mod's ui_event message for opening its window (the MODS tab's entry in shroudlink).
std::string menu_event();
// ... and for the window closed by the player.
std::string closed_event();

// The HUD lines for the mod's "hud" message: a title line, then one per row ("" lines: none).
std::vector<std::string> hud_lines(const json::Value& hud);

// Text the EMF overlay can draw: its font has Latin-1, Latin Extended-A and Cyrillic; dashes,
// quotes and an ellipsis outside those become their plain forms, anything else '?'. Cut to
// `max_bytes` at a whole character.
std::string drawable(const std::string& text, size_t max_bytes = 256);

}  // namespace exped::emfui
