#include "emf_host.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <set>
#include <vector>

#include "emf_ui.h"
#include "expeditions.h"
#include "item_names.h"
#include "lang.h"
#include "page_text.h"
#include "platform.h"
#include "sdk.h"

#ifdef _WIN32
#include "../third_party/emf/keys.hpp"
#endif

namespace exped::emfhost {
namespace {

// ---- component ids (types.json qualifiedHash, as in sdk.h) ----
constexpr uint32_t kCurrentTransform = 0x027ac564;
constexpr uint32_t kPlayerState = 0x6ff967bb;
constexpr uint32_t kInventory = 0xb19528a9;
constexpr uint32_t kDynamicActiveNpcState = 0x9e65507a;  // the per-entity half of ActiveNpcState (state at +0)
constexpr uint32_t kMaxPlayer = 1023;                     // players are the entities 1..1023
constexpr uint8_t kGenericInventory = 4;                  // InventoryCategory::Generic
constexpr const char* kWindowId = "expeditions.window";
constexpr double kTickEvery = 0.2;

// InventorySetup (84 bytes): linksEntities[16] u32, linksCategories[16] u8, flags, genericSlotCount, availableSlotCount, isInitialized
struct InventorySetup {
    uint32_t links[16];
    uint8_t categories[16];
    uint8_t flags;
    uint8_t generic_slots;
    uint8_t available_slots;
    uint8_t initialized;
};
static_assert(sizeof(InventorySetup) == 84);
// Inventory (96 bytes): 8 x {item id, count, pide}
struct Stack {
    uint32_t item;
    uint32_t count;
    uint32_t pide;  // a gear item's own data entity
};
struct Inventory {
    Stack slots[8];
};
static_assert(sizeof(Inventory) == 96);

const EmfApi* g_api = nullptr;
exped::Mod g_mod;
sl::HostApi g_host{};
int g_ctx_dummy = 0;
void* const g_ctx = &g_ctx_dummy;  // the mod sends only with a context
constexpr uint64_t kClient = 1;    // the one player who sees windows: whoever runs the world

EmfWorld* g_world = nullptr;  // the server world during a call into the mod
uint32_t g_session = 0;
bool g_joined = false;
bool g_blocked = false;  // the shroudlink build is installed too
bool g_told_blocked = false;
double g_last_tick = 0;
double g_last_key_read = -100;
uint32_t g_open_vk = 0;
#ifdef _WIN32
bool g_key_was_down = false;
#endif

// per call: what the world looked like (filled on first use)
struct Seen {
    uint32_t id;
    double pos[3];
};
bool g_scanned = false;
std::vector<Seen> g_seen;
bool g_players_known = false;
std::vector<uint32_t> g_players;

// per call: what the chests will hold once the requests made in this call are done
struct SlotState {
    uint32_t inventory;
    uint8_t index;
    uint32_t item;
    uint32_t count;
    bool gear;
};
std::map<uint32_t, std::vector<SlotState>> g_stock;
std::map<uint32_t, uint32_t> g_max_stack;

// the window and the HUD
bool g_pending_open = false, g_pending_close = false;
emfui::Window g_pending, g_shown;
bool g_open = false;
bool g_told_refused = false;  // once per world
size_t g_hud_count = 0;

double (*g_clock)() = &platform::steady_seconds;

void log_line(const std::string& text) {
    if (g_api && g_api->log) g_api->log(kModId, "%s", text.c_str());
}

#ifdef _WIN32
std::string utf8(const std::wstring& w) {
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n : 0, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}
std::string env(const wchar_t* name) {
    wchar_t buf[2048];
    const DWORD n = GetEnvironmentVariableW(name, buf, 2048);
    if (!n) return "(not set)";
    if (n >= 2048) return "(too long)";
    return utf8(std::wstring(buf, n));
}
bool elevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD n = 0;
    const bool yes = GetTokenInformation(token, TokenElevation, &e, sizeof e, &n) && e.TokenIsElevated;
    CloseHandle(token);
    return yes;
}
// "1.4.341.0 (C:\Windows\SYSTEM32\vulkan-1.dll)": the Vulkan loader the game loaded
std::string vulkan_loader() {
    HMODULE m = GetModuleHandleW(L"vulkan-1.dll");
    if (!m) return "not loaded yet";
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(m, path, MAX_PATH);
    std::string version = "version unknown";
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &handle);
    if (size) {
        std::vector<uint8_t> info(size);
        VS_FIXEDFILEINFO* fixed = nullptr;
        UINT len = 0;
        if (GetFileVersionInfoW(path, 0, size, info.data()) && VerQueryValueW(info.data(), L"\\", reinterpret_cast<void**>(&fixed), &len) && fixed)
            version = std::to_string(HIWORD(fixed->dwFileVersionMS)) + "." + std::to_string(LOWORD(fixed->dwFileVersionMS)) + "." +
                      std::to_string(HIWORD(fixed->dwFileVersionLS)) + "." + std::to_string(LOWORD(fixed->dwFileVersionLS));
    }
    return version + " (" + utf8(path) + ")";
}
#endif

// What decides whether EMF's overlay (a Vulkan layer) gets into the game: logged at start and when a window is refused.
void log_overlay_facts(const char* when) {
#ifdef _WIN32
    log_line(std::string("diagnostics (") + when + "): game as administrator: " + (elevated() ? "YES (Windows' Vulkan loader then ignores EMF's layer)" : "no") +
             "; vulkan-1.dll " + vulkan_loader() + "; VK_ADD_LAYER_PATH=" + env(L"VK_ADD_LAYER_PATH") + "; VK_INSTANCE_LAYERS=" + env(L"VK_INSTANCE_LAYERS") +
             "; VK_LOADER_LAYERS_DISABLE=" + env(L"VK_LOADER_LAYERS_DISABLE") + "; VK_LOADER_LAYERS_ENABLE=" + env(L"VK_LOADER_LAYERS_ENABLE") +
             "; VK_LAYER_PATH=" + env(L"VK_LAYER_PATH"));
#else
    (void)when;
#endif
}

// nothing may unwind into EMF: a failure is logged and the call dropped
template <class F>
void guarded(const char* what, F&& f) {
    try {
        f();
    } catch (const std::exception& e) {
        log_line(std::string(what) + " failed: " + e.what());
    } catch (...) {
        log_line(std::string(what) + " failed");
    }
}

bool in_call() { return g_api && g_world; }

void* component(uint32_t entity, uint32_t hash, uint64_t* size = nullptr) {
    if (!in_call() || !entity) return nullptr;
    uint64_t s = 0;
    void* p = g_api->get_component(g_world, hash, entity, &s);
    if (size) *size = p ? s : 0;
    return p;
}

// ActiveNpcState's data lives in its Dynamic half on EMF's side
uint32_t data_hash(uint32_t hash) { return hash == sl::comp::ActiveNpcState ? kDynamicActiveNpcState : hash; }

// the entity has the component: its own data, its Dynamic half or its template's
bool has(uint32_t entity, uint32_t hash) {
    if (component(entity, data_hash(hash))) return true;
    if (component(entity, hash)) return true;
    return in_call() && g_api->get_template_component && g_api->get_template_component(g_world, hash, entity, nullptr);
}

bool position_of(uint32_t entity, double out[3]) {
    uint64_t size = 0;
    const auto* t = static_cast<const int64_t*>(component(entity, kCurrentTransform, &size));
    if (!t || size < 24) return false;
    for (int i = 0; i < 3; ++i) out[i] = static_cast<double>(t[i]) / 4294967296.0;
    return true;
}

void scan() {
    if (g_scanned || !in_call()) return;
    g_scanned = true;
    g_seen.clear();
    g_api->for_each_entity(
        g_world,
        [](uint32_t id, void*) {
            Seen s{id, {0, 0, 0}};
            if (position_of(id, s.pos)) g_seen.push_back(s);
        },
        nullptr);
}

const std::vector<uint32_t>& players() {
    if (!g_players_known && in_call()) {
        g_players_known = true;
        g_players.clear();
        for (uint32_t id = 1; id <= kMaxPlayer; ++id) {
            if (component(id, kPlayerState) && component(id, kCurrentTransform)) g_players.push_back(id);
        }
    }
    return g_players;
}
bool is_player(uint32_t e) {
    const auto& p = players();
    return std::find(p.begin(), p.end(), e) != p.end();
}

template <class F>
std::vector<uint32_t> nearby(uint32_t entity, double radius, F&& keep) {
    std::vector<std::pair<double, uint32_t>> found;
    double at[3];
    if (!position_of(entity, at)) return {};
    scan();
    for (const auto& s : g_seen) {
        const double dx = s.pos[0] - at[0], dy = s.pos[1] - at[1], dz = s.pos[2] - at[2];
        const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d <= radius && keep(s.id)) found.emplace_back(d, s.id);
    }
    std::sort(found.begin(), found.end());
    std::vector<uint32_t> out;
    for (const auto& f : found) out.push_back(f.second);
    return out;
}

const InventorySetup* setup_of(uint32_t entity) {
    uint64_t size = 0;
    const auto* s = static_cast<const InventorySetup*>(component(entity, sl::comp::InventorySetup, &size));
    return s && size >= sizeof(InventorySetup) ? s : nullptr;
}

// the storage slots of a chest: its Generic inventories, as many slots as it has open
std::vector<SlotState> read_stock(uint32_t entity) {
    std::vector<SlotState> out;
    std::vector<uint32_t> inventories;
    size_t limit = 0;
    if (const InventorySetup* s = setup_of(entity)) {
        for (int i = 0; i < 16; ++i) {
            if (s->links[i] && s->categories[i] == kGenericInventory &&
                std::find(inventories.begin(), inventories.end(), s->links[i]) == inventories.end())
                inventories.push_back(s->links[i]);
        }
        limit = s->available_slots ? s->available_slots : s->generic_slots;
    }
    if (inventories.empty() && component(entity, kInventory)) inventories.push_back(entity);
    for (const uint32_t inv : inventories) {
        uint64_t size = 0;
        const auto* in = static_cast<const Inventory*>(component(inv, kInventory, &size));
        if (!in || size < sizeof(Inventory)) continue;
        for (uint8_t i = 0; i < 8; ++i) {
            if (limit && out.size() >= limit) return out;
            const Stack& st = in->slots[i];
            out.push_back({inv, i, st.count ? st.item : 0, st.item ? st.count : 0, st.pide != 0});
        }
    }
    return out;
}

std::vector<SlotState>& stock(uint32_t chest) {
    auto it = g_stock.find(chest);
    if (it == g_stock.end()) it = g_stock.emplace(chest, read_stock(chest)).first;
    return it->second;
}

uint32_t max_stack(uint32_t item) {
    auto it = g_max_stack.find(item);
    if (it != g_max_stack.end()) return it->second;
    EmfItemInfo info{};
    info.size = sizeof info;
    uint32_t m = 0;
    if (in_call() && g_api->item_info(g_world, item, &info)) m = info.max_stack;
    if (m) g_max_stack[item] = m;  // not known yet: asked again later
    return m;
}

// ---- the HostApi ----

void api_send(void*, uint64_t, const uint8_t* text, size_t len) {
    bool ok = false;
    const json::Value m = json::parse(std::string(reinterpret_cast<const char*>(text), len), &ok);
    if (!ok) return;
    const std::string op = m["op"].as_string();
    if (op == "open") {
        g_pending = emfui::translate(m["window"], [](uint32_t id) {
            const char* name = exped::item_name(id);
            return name ? items::readable(name, lang::ru()) : std::to_string(id);
        });
        g_pending_open = true;
        g_pending_close = false;
    } else if (op == "close") {
        if (!g_pending_open) g_pending_close = true;
    } else if (op == "notify") {
        if (g_api) g_api->notify("%s", emfui::drawable(m["text"].as_string(), 400).c_str());
    } else if (op == "hud") {
        const auto lines = emfui::hud_lines(m);
        const int64_t place = g_api->setting_int ? g_api->setting_int(kModId, "hud_place", 0) : 0;
        const uint32_t where = place == 1 ? EMF_HUD_BOTTOM_RIGHT : place == 2 ? EMF_HUD_BOTTOM_LEFT : EMF_HUD_TOP_RIGHT;
        char id[48];
        for (size_t i = 0; i < std::max(lines.size(), g_hud_count); ++i) {
            snprintf(id, sizeof id, "expeditions.hud.%02u", static_cast<unsigned>(i));
            g_api->hud_set(id, i < lines.size() ? lines[i].c_str() : "", where, i == 0 && i < lines.size() ? 0xE8C872FFu : 0);
        }
        g_hud_count = lines.size();
    }
}

void api_log(const uint8_t* text, size_t len) {
    std::string line(reinterpret_cast<const char*>(text), len);
    const std::string prefix = "expeditions: ";
    if (line.rfind(prefix, 0) == 0) line.erase(0, prefix.size());
    log_line(line);
}

size_t api_players(uint32_t* out, size_t cap) {
    const auto& p = players();
    for (size_t i = 0; i < p.size() && i < cap; ++i) out[i] = p[i];
    return p.size();
}

bool api_position(uint32_t entity, double* out) { return position_of(entity, out); }

uint32_t api_player_of(uint64_t) {
    const auto& p = players();
    const uint32_t local = g_api && g_api->local_player ? g_api->local_player() : 0;
    if (local && is_player(local)) return local;
    return p.empty() ? 0 : p[0];
}

size_t api_read_component(uint32_t entity, uint32_t hash, uint8_t* out, size_t cap) {
    uint64_t size = 0;
    const void* p = component(entity, data_hash(hash), &size);
    if (!p) p = component(entity, hash, &size);
    if (!p && in_call() && g_api->get_template_component) p = g_api->get_template_component(g_world, hash, entity, &size);
    if (!p || size == 0) return 0;
    std::memcpy(out, p, std::min<size_t>(cap, size));
    return static_cast<size_t>(size);
}

bool api_write_component(uint32_t entity, uint32_t hash, const uint8_t* data, size_t len) {
    uint64_t size = 0;
    void* p = component(entity, data_hash(hash), &size);
    if (!p || size != len) return false;
    std::memcpy(p, data, len);
    return true;
}

size_t api_inventories_near(uint32_t entity, double radius, uint32_t* out, size_t cap) {
    const auto found = nearby(entity, radius, [](uint32_t id) {
        const InventorySetup* s = setup_of(id);
        // a chest whose slots are not there yet (just loaded) is not a chest yet
        return s && s->initialized && !is_player(id) && !read_stock(id).empty();
    });
    for (size_t i = 0; i < found.size() && i < cap; ++i) out[i] = found[i];
    return found.size();
}

size_t api_inventory(uint32_t entity, sl::Slot* out, size_t cap) {
    size_t n = 0;
    for (const auto& s : read_stock(entity)) {
        if (!s.item || !s.count) continue;
        if (n < cap) out[n] = {(static_cast<uint64_t>(s.index) << 32) | s.inventory, s.item, s.count};
        ++n;
    }
    return n;
}

size_t api_entities_near(uint32_t entity, double radius, uint32_t hash, uint32_t* out, size_t cap) {
    const auto found = nearby(entity, radius, [hash](uint32_t id) { return has(id, hash); });
    for (size_t i = 0; i < found.size() && i < cap; ++i) out[i] = found[i];
    return found.size();
}

void put_done_inner(EmfWorld* world, uint32_t chest, uint32_t item, uint32_t requested, uint32_t added) {
    if (added >= requested) return;
    // the chest took fewer than planned (it changed meanwhile): the rest drops at the player's feet
    const uint32_t rest = requested - added;
    const char* name = exped::item_name(item);
    log_line("chest " + std::to_string(chest) + " took " + std::to_string(added) + " of " + std::to_string(requested) + " " + (name ? name : std::to_string(item)) +
             "; the rest drops at the player");
    if (!world || !g_api->local_player) return;
    const uint32_t player = g_api->local_player();
    if (player && g_api->give_item(world, player, item, rest))
        g_api->notify("%s", emfui::drawable(std::string(lang::L("Экспедиции: в сундуках не хватило места, ", "Expeditions: the chests had no room for ")) + std::to_string(rest) + " " +
                                                (name ? items::readable(name, lang::ru()) : std::to_string(item)) + lang::L(" — у ваших ног.", ": at your feet."))
                                .c_str());
}

void put_done(EmfWorld* world, uint32_t chest, uint32_t item, uint32_t requested, uint32_t added, void*) {
    guarded("put_items result", [&] { put_done_inner(world, chest, item, requested, added); });
}

uint32_t api_give_items(uint32_t entity, uint32_t item, uint32_t count) {
    if (!in_call() || !count) return 0;
    const uint32_t max = max_stack(item);
    if (!max) return 0;
    auto& slots = stock(entity);
    uint64_t room = 0;
    for (const auto& s : slots) {
        if (!s.item) room += max;
        else if (s.item == item && !s.gear && s.count < max) room += max - s.count;
    }
    const auto n = static_cast<uint32_t>(std::min<uint64_t>({count, room, 65535}));
    if (!n) return 0;
    if (!g_api->put_items(g_world, entity, item, n, put_done, nullptr)) return 0;
    // what the chest holds once it is done: stacks topped up first, then empty slots
    uint32_t left = n;
    for (auto& s : slots) {
        if (left && s.item == item && !s.gear && s.count < max) {
            const uint32_t k = std::min(left, max - s.count);
            s.count += k;
            left -= k;
        }
    }
    for (auto& s : slots) {
        if (left && !s.item) {
            const uint32_t k = std::min(left, max);
            s.item = item;
            s.count = k;
            left -= k;
        }
    }
    return n;
}

void take_done(EmfWorld*, uint32_t chest, uint32_t item, uint32_t requested, uint32_t taken, void*) {
    if (taken < requested)
        guarded("take_items result", [&] { log_line("rations: chest " + std::to_string(chest) + " gave " + std::to_string(taken) + " of " + std::to_string(requested) + " (item " + std::to_string(item) + ")"); });
}

uint32_t api_remove_items(uint32_t entity, uint64_t slot, uint32_t count) {
    if (!in_call() || !count) return 0;
    const auto inventory = static_cast<uint32_t>(slot & 0xFFFFFFFFu);
    const auto index = static_cast<uint32_t>((slot >> 32) & 0xFF);
    uint64_t size = 0;
    const auto* in = static_cast<const Inventory*>(component(inventory, kInventory, &size));
    if (!in || size < sizeof(Inventory) || index >= 8 || !in->slots[index].item) return 0;
    const uint32_t n = std::min(count, in->slots[index].count);
    if (!n) return 0;
    EmfItemsRequest r{};
    r.size = sizeof r;
    r.entity = entity;
    r.item = in->slots[index].item;
    r.count = n;
    r.slot = slot;
    r.cb = take_done;
    return g_api->take_items(g_world, &r) ? n : 0;
}

// what this build does not offer: the mod does not use these
size_t api_slots(void*, uint32_t, sl::Slot*, size_t) { return 0; }
bool api_is_player(void*, uint32_t e) { return is_player(e); }
size_t api_chest_name(void*, uint32_t, uint8_t*, size_t) { return 0; }
uint32_t api_max_stack(void*, uint32_t item) { return max_stack(item); }
uint32_t api_move_stack(void*, uint64_t, uint32_t) { return 0; }
bool api_teleport(uint32_t, double, double, double) { return false; }
uint32_t api_move_stack_between(uint32_t, uint64_t, uint32_t) { return 0; }
bool api_apply_buff(uint32_t, const uint8_t*, double) { return false; }
size_t api_player_name(uint32_t, uint8_t*, size_t) { return 0; }
uint32_t api_move_items(uint32_t, uint64_t, uint32_t, uint32_t) { return 0; }
size_t api_world_chest_name(uint32_t, uint8_t*, size_t) { return 0; }

// ---- calls into the mod ----

void begin(EmfWorld* world) {
    g_world = world && g_api->world_kind(world) == EMF_WORLD_SERVER ? world : nullptr;
    g_scanned = false;
    g_players_known = false;
    g_stock.clear();
}

void ui_event(EmfWorld* world, const EmfUiEvent* event, void*);

void flush_window() {
    if (g_pending_open) {
        g_pending_open = false;
        std::vector<EmfUiWidget> widgets(g_pending.widgets.size());
        for (size_t i = 0; i < widgets.size(); ++i) {
            const auto& w = g_pending.widgets[i];
            widgets[i] = EmfUiWidget{};
            widgets[i].type = w.type;
            widgets[i].flags = w.flags;
            widgets[i].key = w.key.empty() ? nullptr : w.key.c_str();
            widgets[i].text = w.text.c_str();
        }
        EmfWindowDef def{};
        def.size = sizeof def;
        def.id = kWindowId;
        def.title = g_pending.title.c_str();
        def.widgets = widgets.data();
        def.count = static_cast<uint32_t>(widgets.size());
        def.widget_size = sizeof(EmfUiWidget);
        def.close_key = g_open_vk;
        def.place = EMF_UI_PLACE_CENTRE;
        def.width = 34;
        def.world_kind = EMF_WORLD_SERVER;
        def.cb = ui_event;
        bool shown = g_open && g_api->window_update(&def);
        if (!shown) shown = g_api->window_open(&def);
        g_open = shown;
        if (shown) g_shown = g_pending;
        else {
            // EMF says why in the line before ("window_open refused ..."); the mod stops refreshing a window it does not have
            if (!g_told_refused) {
                g_told_refused = true;
                log_line("EMF did not open the window (its reason is in the line above); with no EMF overlay nothing can be shown");
                log_overlay_facts("window refused");
            }
            g_mod.message(&g_host, g_ctx, kClient, emfui::closed_event());
        }
    } else if (g_pending_close) {
        if (g_open) g_api->window_close(kWindowId);
        g_open = false;
    }
    g_pending_close = false;
}

void end() {
    flush_window();
    g_world = nullptr;
    g_stock.clear();
    g_seen.clear();
    g_scanned = false;
    g_players_known = false;
}

void message(EmfWorld* world, const std::string& text) {
    begin(world);
    guarded("a window event", [&] { g_mod.message(&g_host, g_ctx, kClient, text); });
    end();
}

void ui_event_inner(EmfWorld* world, const EmfUiEvent* event) {
    if (!event) return;
    if (event->kind == EMF_UI_CLOSED) {
        if (event->reason == EMF_UI_BY_MOD) return;  // our own close or a replacement
        g_open = false;
        g_mod.message(&g_host, g_ctx, kClient, emfui::closed_event());
        return;
    }
    if (event->kind != EMF_UI_CLICK || !event->key || g_blocked) return;
    const std::string m = emfui::click_event(event->key, g_shown);
    if (!m.empty()) message(world, m);
}

void ui_event(EmfWorld* world, const EmfUiEvent* event, void*) {
    guarded("ui_event", [&] { ui_event_inner(world, event); });
}

void open_menu(EmfWorld* world) {
    if (g_blocked) return;
    if (g_open) {
        g_api->window_close(kWindowId);
        g_open = false;
        g_mod.message(&g_host, g_ctx, kClient, emfui::closed_event());
        return;
    }
    message(world, emfui::menu_event());
}

void read_key() {
#ifdef _WIN32
    char buf[64] = {};
    buf[1] = 1;
    if (g_api->setting_text) g_api->setting_text(kModId, "open_key", buf, sizeof buf);
    std::wstring w;
    for (const char* c = buf; *c; ++c) w += static_cast<wchar_t>(static_cast<unsigned char>(*c));
    g_open_vk = w.empty() ? 0 : emf::keys::parse_key(w);
#endif
}

void on_tick_inner(EmfWorld* world) {
    if (!world || g_api->world_kind(world) != EMF_WORLD_SERVER) return;
    const uint32_t session = g_api->server_session ? g_api->server_session() : 1;
    if (g_blocked) {
        if (!g_told_blocked) {
            g_told_blocked = true;
            g_api->notify("%s", emfui::drawable(lang::L("Экспедиции (EMF) выключены: стоит и версия для shroudlink (shroudlink_mods\\expeditions.dll). Оставьте одну из них.",
                                                       "Expeditions (EMF) is off: the shroudlink version is installed too (shroudlink_mods\\expeditions.dll). Keep one of them."))
                                    .c_str());
        }
        return;
    }
    if (!g_joined || session != g_session) {
        // a new world (or the same one loaded again): entity ids mean other things now
        if (g_joined) g_mod.leave(kClient);
        g_session = session;
        g_joined = true;
        g_open = false;
        g_told_refused = false;
        for (size_t i = 0; i < g_hud_count; ++i) {
            char id[48];
            snprintf(id, sizeof id, "expeditions.hud.%02u", static_cast<unsigned>(i));
            g_api->hud_set(id, "", EMF_HUD_TOP_RIGHT, 0);
        }
        g_hud_count = 0;
        begin(world);
        g_mod.join(&g_host, g_ctx, kClient);
        end();
    }
    const double now = g_clock();
    if (now - g_last_key_read > 2) {
        g_last_key_read = now;
        read_key();
    }
#ifdef _WIN32
    const bool down = emf::keys::down(g_open_vk);
    const bool pressed = down && !g_key_was_down;
    g_key_was_down = down;
    if (pressed) open_menu(world);
#endif
    if (now - g_last_tick < kTickEvery) return;
    g_last_tick = now;
    begin(world);
    g_mod.tick(&g_host, g_ctx);
    end();
}

void on_tick(EmfWorld* world, void*) {
    guarded("on_tick", [&] { on_tick_inner(world); });
    g_world = nullptr;
}

void command(EmfWorld* world, int, const char* const*, void*) {
    if (g_blocked) {
        g_api->command_reply("%s", "expeditions: off (the shroudlink version is installed too)");
        return;
    }
    if (!world) return;
    guarded("command", [&] { open_menu(world); });
}

}  // namespace

void open_key_pressed(EmfWorld* world) { open_menu(world); }
exped::Mod& mod_for_tests() { return g_mod; }
void set_clock_for_tests(double (*clock)()) { g_clock = clock; }

bool start(const EmfApi* api, const std::string& dir) {
    if (!api || api->version < kMinApi || api->size < offsetof(EmfApi, window_close) + sizeof(void*)) {
        if (api && api->log) api->log(kModId, "this EMF is too old: Expeditions needs EMF API %u (EMF 0.4 or newer)", kMinApi);
        return false;
    }
    g_api = api;
    page::set_plain(true);
    g_host = sl::HostApi{};
    g_host.version = sl::kAbiVersion;
    g_host.send = api_send;
    g_host.log = api_log;
    g_host.slots = api_slots;
    g_host.is_player = api_is_player;
    g_host.chest_name = api_chest_name;
    g_host.max_stack = api_max_stack;
    g_host.move_stack = api_move_stack;
    g_host.players = api_players;
    g_host.position = api_position;
    g_host.teleport = api_teleport;
    g_host.player_of = api_player_of;
    g_host.read_component = api_read_component;
    g_host.write_component = api_write_component;
    g_host.inventories_near = api_inventories_near;
    g_host.inventory = api_inventory;
    g_host.move_stack_between = api_move_stack_between;
    g_host.apply_buff = api_apply_buff;
    g_host.player_name = api_player_name;
    g_host.entities_near = api_entities_near;
    g_host.move_items = api_move_items;
    g_host.give_items = api_give_items;
    g_host.remove_items = api_remove_items;
    g_host.world_chest_name = api_world_chest_name;

    const std::string game = dir.empty() ? platform::game_dir() : dir;
    g_blocked = platform::file_exists(game + "shroudlink_mods" + (game.find('\\') != std::string::npos ? "\\" : "/") + "expeditions.dll");
    if (g_blocked) {
        log_line("off: the shroudlink version is installed too (shroudlink_mods\\expeditions.dll); keep one of them");
    } else {
        g_mod.set_host_client(kClient);  // EMF windows are the local player's: whoever runs the world
        g_mod.load(&g_host, dir);
    }
    api->on_tick(on_tick, nullptr);
    api->register_command("expeditions", ": open the Expeditions window", command, nullptr);
    log_overlay_facts("start");
    log_line(std::string("EMF build ") + kVersion + (g_blocked ? " (off)" : "") + "; the window opens with the key set in the Mods tab or the console command 'expeditions'");
    return true;
}

}  // namespace exped::emfhost
