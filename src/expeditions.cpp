#include "expeditions.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string_view>

#include "defaults.h"
#include "items_table.h"
#include "lang.h"
#include "page_text.h"
#include "regions.h"
#include "supplies.h"
#include "platform.h"

namespace exped {

namespace {

using namespace page;
using lang::L;

constexpr const char* kConfigFile = "expeditions_config.json";
constexpr const char* kStateFile = "expeditions_state.json";
constexpr double kSurvivorRadius = 30;   // who can be sent: survivors this close to the player
constexpr double kNpcSearchRadius = 220; // finding a survivor again (they may stand outside the base)
constexpr double kStepEvery = 5;
constexpr double kSaveEvery = 30;
constexpr double kRefreshEvery = 10;
constexpr double kChestRadius = 40;      // without a base: chests this close to the player
constexpr float kAwayRange = 3;          // how far they wander around the spot they wait at
constexpr size_t kHistory = 8;
// `BalancingTable.buildzoneSizesPerAltarLevel`: the build zone's edge by the altar's level
constexpr double kBuildZone[] = {0, 40, 80, 120, 160, 160};
constexpr double kMultipliers[] = {0.25, 0.5, 1, 1.5, 2, 3, 5, 10};

uint32_t u32_at(const std::vector<uint8_t>& d, size_t at) {
    if (at + 4 > d.size()) return 0;
    uint32_t v;
    std::memcpy(&v, d.data() + at, 4);
    return v;
}
float f32_at(const std::vector<uint8_t>& d, size_t at) {
    if (at + 4 > d.size()) return 0;
    float v;
    std::memcpy(&v, d.data() + at, 4);
    return v;
}
void put_f32(std::vector<uint8_t>& d, size_t at, float v) {
    if (at + 4 <= d.size()) std::memcpy(d.data() + at, &v, 4);
}

std::string fmt_number(double v) {
    char buf[32];
    if (v == std::floor(v)) std::snprintf(buf, sizeof buf, "%.0f", v);
    else std::snprintf(buf, sizeof buf, "%g", v);
    return buf;
}

std::vector<std::pair<uint32_t, uint32_t>> pairs_of(const json::Value& a) {
    std::vector<std::pair<uint32_t, uint32_t>> out;
    for (const auto& p : a.items()) {
        const auto item = static_cast<uint32_t>(p[0].as_number(0));
        const auto count = static_cast<uint32_t>(p[1].as_number(0));
        if (item && count) out.emplace_back(item, count);
    }
    return out;
}
json::Value pairs_json(const std::vector<std::pair<uint32_t, uint32_t>>& v) {
    json::Value a = json::Value::array();
    for (const auto& [item, count] : v) {
        json::Value p = json::Value::array();
        p.push(item);
        p.push(count);
        a.push(p);
    }
    return a;
}

json::Value node_text(const std::string& text) {
    json::Value n = json::Value::object();
    n.set("type", "text");
    n.set("text", text);
    return n;
}
// A text, broken into lines that fit the page.
void push_text(json::Value& content, const std::string& text) {
    for (const auto& line : wrap_lines(text)) content.push(node_text(line));
}
json::Value node_heading(const std::string& text);
json::Value node_row(std::vector<json::Value> children);

// A quieter line: the page's thin title style (a heading alone in a row gets no divider).
void push_caption(json::Value& content, const std::string& text) {
    for (const auto& line : wrap_lines(text)) content.push(node_row({node_heading(line)}));
}

// "label value" pairs on a line: labels thin, values bold, pairs apart; a new line when full.
void push_stats(json::Value& content, const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::vector<json::Value> row;
    double width = 0;
    for (const auto& [label, value] : pairs) {
        const double w = text_units(label) * 0.9 + text_units(value) + 3 * 16 + 24;
        if (!row.empty() && width + w > 1150) {
            content.push(node_row(row));
            row.clear();
            width = 0;
        }
        if (!row.empty()) row.push_back(node_text("  "));
        row.push_back(node_heading(label));
        row.push_back(node_text(value));
        width += w;
    }
    if (!row.empty()) content.push(node_row(row));
}

json::Value node_toggle(const std::string& id, const std::string& label, bool value) {
    json::Value n = json::Value::object();
    n.set("type", "toggle");
    n.set("id", id);
    n.set("label", label);
    n.set("value", value);
    return n;
}
json::Value node_tabs(const std::string& id, const std::vector<std::string>& tabs, int selected) {
    json::Value n = json::Value::object();
    n.set("type", "tabs");
    n.set("id", id);
    json::Value t = json::Value::array();
    for (const auto& label : tabs) t.push(pad_label(label));
    n.set("tabs", t);
    n.set("selected", selected);
    return n;
}
json::Value node_heading(const std::string& text) {
    json::Value n = json::Value::object();
    n.set("type", "heading");
    n.set("text", text);
    return n;
}
json::Value node_button(const std::string& id, const std::string& label) {
    json::Value n = json::Value::object();
    n.set("type", "button");
    n.set("id", id);
    n.set("label", pad_label(label));
    return n;
}
json::Value node_list(const std::string& id, const std::vector<std::string>& entries) {
    json::Value n = json::Value::object();
    n.set("type", "list");
    n.set("id", id);
    json::Value e = json::Value::array();
    for (const auto& s : entries) e.push(s);
    n.set("entries", e);
    return n;
}
json::Value node_row(std::vector<json::Value> children) {
    json::Value n = json::Value::object();
    n.set("type", "row");
    json::Value c = json::Value::array();
    for (auto& ch : children) c.push(std::move(ch));
    n.set("children", c);
    return n;
}
json::Value node_items(const std::string& id, const std::vector<std::pair<uint32_t, uint32_t>>& items) {
    json::Value n = json::Value::object();
    n.set("type", "item_grid");
    n.set("id", id);
    n.set("columns", 8);
    json::Value list = json::Value::array();
    for (const auto& [item, count] : items) {
        json::Value it = json::Value::object();
        it.set("id", item);
        it.set("count", count);
        list.push(it);
    }
    n.set("items", list);
    return n;
}

}  // namespace

// ---- item names ----

uint32_t item_id(const std::string& name) {
    if (name.empty()) return 0;
    bool digits = true;
    for (const char c : name) digits = digits && c >= '0' && c <= '9';
    if (digits && name.size() <= 10) return static_cast<uint32_t>(std::strtoull(name.c_str(), nullptr, 10));
    const auto* begin = std::begin(kItems);
    const auto* end = std::end(kItems);
    const auto* it = std::lower_bound(begin, end, name, [](const ItemName& a, const std::string& b) { return std::strcmp(a.name, b.c_str()) < 0; });
    return it != end && name == it->name ? it->id : 0;
}

const char* item_name(uint32_t id) {
    for (const auto& i : kItems) {
        if (i.id == id) return i.name;
    }
    return nullptr;
}

std::string items_word(uint32_t n) { return lang::items_word(n); }

std::string format_minutes(double seconds) { return lang::minutes(seconds); }

// ---- config ----

const Profession* Config::profession(uint32_t workshop) const {
    for (const auto& p : professions) {
        if (p.workshop == workshop) return &p;
    }
    return nullptr;
}

Config parse_config(const json::Value& v) {
    Config c;
    for (const auto& d : v["durations_minutes"].items()) {
        const int m = static_cast<int>(d.as_number(0));
        if (m > 0 && m <= 24 * 60) c.durations_min.push_back(m);
    }
    if (c.durations_min.empty()) c.durations_min = {15, 30, 60, 120};
    c.multiplier = std::clamp(v["loot_multiplier"].as_number(1.0), 0.0, 1000.0);
    c.offline_time = v["time_passes_while_game_closed"].as_bool(false);
    c.walk_away = v["walk_away"].as_bool(true);
    c.walk_beyond_m = std::clamp(v["walk_beyond_base_m"].as_number(12), 0.0, 200.0);
    c.hud = v["hud_panel"].as_bool(true);
    const json::Value& sup = v["supplies"];
    c.supplies = sup["enabled"].as_bool(true);
    c.rations_per_hour = std::clamp(sup["rations_per_hour"].as_number(2), 0.0, 100.0);
    c.events = v["events"].as_bool(true);
    const json::Value& xp = v["experience"];
    c.experience = xp["enabled"].as_bool(true);
    c.minutes_per_level = std::clamp(xp["minutes_per_level"].as_number(30), 1.0, 100000.0);
    c.max_level = static_cast<int>(std::clamp(xp["max_level"].as_number(10), 1.0, 100.0));
    c.loot_per_level = std::clamp(xp["loot_bonus_per_level"].as_number(0.06), 0.0, 10.0);
    c.risk_per_level = std::clamp(xp["risk_reduction_per_level"].as_number(0.07), 0.0, 1.0);
    c.unlock_by_visiting = v["unlock_regions_by_visiting"].as_bool(true);
    c.language = v["language"].string_or("auto");
    if (c.language != "ru" && c.language != "en") c.language = "auto";
    const std::string access = v["settings_in_game"].string_or("host");
    c.settings_in_game = access == "everyone" ? Config::Access::Everyone : access == "nobody" ? Config::Access::Nobody : Config::Access::Host;
    if (access != "host" && access != "everyone" && access != "nobody")
        c.problems.push_back(L("settings_in_game: «", "settings_in_game: \"") + access + L("» — бывает host, everyone или nobody; действует host", "\" can be host, everyone or nobody; host is in use"));
    for (const auto& pv : v["professions"].items()) {
        Profession p;
        p.workshop = static_cast<uint32_t>(pv["workshop"].as_number(0));
        p.name = pv["name"].string_or("?");
        if (!p.workshop) {
            c.problems.push_back(L("профессия без workshop: ", "a trade without a workshop: ") + p.name);
            continue;
        }
        for (const auto& rv : pv["regions"].items()) {
            Region r;
            r.name = rv["name"].string_or("?");
            if (rv["area"].is_string()) {
                r.area = regions::by_english(rv["area"].as_string());
                if (r.area < 0) c.problems.push_back(L("неизвестный регион в area: ", "unknown region in area: ") + rv["area"].as_string());
            } else if (const size_t open = r.name.rfind(" ("); open != std::string::npos && r.name.back() == ')') {
                r.area = regions::by_english(r.name.substr(open + 2, r.name.size() - open - 3));  // "Ревелвуд (Revelwood)"
            } else {
                r.area = regions::by_english(r.name);  // "Revelwood"
            }
            r.danger = rv["danger"].is_number() ? std::clamp(rv["danger"].as_number(), 0.0, 0.95) : regions::default_danger(r.area);
            for (const auto& lv : rv["loot"].items()) {
                LootEntry e;
                const json::Value& item = lv["item"];
                e.name = item.is_number() ? fmt_number(item.as_number()) : item.as_string();
                e.item = item.is_number() ? static_cast<uint32_t>(item.as_number()) : item_id(e.name);
                if (!e.item) {
                    c.problems.push_back(L("неизвестный предмет: ", "unknown item: ") + e.name);
                    continue;
                }
                const json::Value& ph = lv["per_hour"];
                if (ph.is_array()) {
                    e.min = ph[0].as_number(0);
                    e.max = ph.size() > 1 ? ph[1].as_number(e.min) : e.min;
                } else {
                    e.min = e.max = ph.as_number(0);
                }
                if (e.max < e.min) std::swap(e.min, e.max);
                e.min = std::max(0.0, e.min);
                e.chance = std::clamp(lv["chance"].as_number(1.0), 0.0, 1.0);
                r.loot.push_back(e);
            }
            p.regions.push_back(r);
        }
        if (!p.regions.empty()) c.professions.push_back(p);
    }
    return c;
}

json::Value config_to_json(const Config& c, const json::Value& original) {
    json::Value v = original.is_object() ? original : json::parse(kDefaultConfig);
    v.set("loot_multiplier", c.multiplier);
    v.set("time_passes_while_game_closed", c.offline_time);
    v.set("walk_away", c.walk_away);
    v.set("hud_panel", c.hud);
    json::Value sup = v["supplies"].is_object() ? v["supplies"] : json::Value::object();
    sup.set("enabled", c.supplies);
    sup.set("rations_per_hour", c.rations_per_hour);
    v.set("supplies", sup);
    v.set("events", c.events);
    json::Value xp = v["experience"].is_object() ? v["experience"] : json::Value::object();
    xp.set("enabled", c.experience);
    xp.set("minutes_per_level", c.minutes_per_level);
    xp.set("max_level", c.max_level);
    xp.set("loot_bonus_per_level", c.loot_per_level);
    xp.set("risk_reduction_per_level", c.risk_per_level);
    v.set("experience", xp);
    v.set("unlock_regions_by_visiting", c.unlock_by_visiting);
    v.set("language", c.language);
    v.set("settings_in_game", c.settings_in_game == Config::Access::Everyone ? "everyone"
                              : c.settings_in_game == Config::Access::Nobody ? "nobody"
                                                                             : "host");
    return v;
}

// ---- the world ----

struct Mod::World {
    const sl::HostApi* api;

    bool has(uint32_t version) const { return api && api->version >= version; }

    std::vector<uint32_t> players() const {
        std::vector<uint32_t> out(64);
        if (!has(4)) return {};
        const size_t n = api->players(out.data(), out.size());
        out.resize(std::min(n, out.size()));
        return out;
    }
    bool position(uint32_t e, double out[3]) const { return has(4) && api->position(e, out); }
    std::vector<uint8_t> component(uint32_t e, uint32_t hash) const {
        std::vector<uint8_t> out(4096);
        if (!has(4)) return {};
        const size_t n = api->read_component(e, hash, out.data(), out.size());
        if (n == 0 || n > out.size()) return {};
        out.resize(n);
        return out;
    }
    bool set_component(uint32_t e, uint32_t hash, const std::vector<uint8_t>& d) const {
        return has(4) && api->write_component(e, hash, d.data(), d.size());
    }
    std::vector<uint32_t> entities_near(uint32_t e, double r, uint32_t hash) const {
        std::vector<uint32_t> out(1024);
        if (!has(6)) return {};
        const size_t n = api->entities_near(e, r, hash, out.data(), out.size());
        out.resize(std::min(n, out.size()));
        return out;
    }
    std::vector<uint32_t> inventories_near(uint32_t e, double r) const {
        std::vector<uint32_t> out(1024);
        if (!has(4)) return {};
        const size_t n = api->inventories_near(e, r, out.data(), out.size());
        out.resize(std::min(n, out.size()));
        return out;
    }
    std::vector<sl::Slot> inventory(uint32_t e) const {
        std::vector<sl::Slot> out(256);
        if (!has(4)) return {};
        size_t n = api->inventory(e, out.data(), out.size());
        if (n > out.size()) {
            out.resize(n);
            n = api->inventory(e, out.data(), out.size());
        }
        out.resize(std::min(n, out.size()));
        return out;
    }
    uint32_t give(uint32_t e, uint32_t item, uint32_t n) const { return has(7) ? api->give_items(e, item, n) : 0; }

    uint32_t workshop_of(uint32_t npc) const { return u32_at(component(npc, sl::comp::Crafting), 0x10); }
    bool is_assistant(uint32_t npc) const { return !component(npc, sl::comp::InventorySetup).empty(); }

    bool home(uint32_t npc, Home& out) const {
        const auto d = component(npc, sl::comp::ActiveNpcStateSaved);
        if (d.size() < 0x14) return false;
        for (int i = 0; i < 3; ++i) out.pos[i] = f32_at(d, static_cast<size_t>(i) * 4);
        out.override_range = d[0xc] != 0;
        out.range = f32_at(d, 0x10);
        return true;
    }
    bool set_home(uint32_t npc, const Home& h) const {
        auto d = component(npc, sl::comp::ActiveNpcStateSaved);
        if (d.size() < 0x14) return false;
        for (int i = 0; i < 3; ++i) put_f32(d, static_cast<size_t>(i) * 4, h.pos[i]);
        d[0xc] = h.override_range ? 1 : 0;
        put_f32(d, 0x10, h.range);
        return set_component(npc, sl::comp::ActiveNpcStateSaved, d);
    }
    // an idle or roaming survivor decides again at once (and so walks to the new home point)
    void nudge(uint32_t npc) const {
        auto d = component(npc, sl::comp::ActiveNpcState);
        if (d.empty()) return;
        const uint8_t s = d[0];
        if (s == sl::npc_state::Idle || s == sl::npc_state::Roam || s == sl::npc_state::WaitingCloseToPlayer) {
            d[0] = sl::npc_state::DecideTask;
            set_component(npc, sl::comp::ActiveNpcState, d);
        }
    }

    // The base an entity stands in: its altar's position and half the build zone's edge.
    bool base_of(uint32_t entity, double center[3], double& half, uint32_t* altar_out = nullptr) const {
        double at[3];
        if (!position(entity, at)) return false;
        for (const uint32_t altar : entities_near(entity, 160.0 / 2 * 1.75, sl::comp::BaseIdComponent)) {
            const auto lv = component(altar, sl::comp::NetworkLevel);
            const size_t level = lv.size() >= 2 ? static_cast<size_t>(lv[0] | (lv[1] << 8)) : 0;
            const double h = (level < std::size(kBuildZone) ? kBuildZone[level] : kBuildZone[std::size(kBuildZone) - 1]) / 2;
            double c[3];
            if (h <= 0 || !position(altar, c)) continue;
            if (std::fabs(at[0] - c[0]) <= h && std::fabs(at[1] - c[1]) <= h && std::fabs(at[2] - c[2]) <= h) {
                for (int i = 0; i < 3; ++i) center[i] = c[i];
                half = h;
                if (altar_out) *altar_out = altar;
                return true;
            }
        }
        return false;
    }

    bool storage(uint32_t e) const {
        for (const uint32_t c : sl::comp::NotStorage) {
            if (!component(e, c).empty()) return false;
        }
        return true;
    }
};

// ---- the mod ----

void Mod::log(const std::string& text) const {
    if (!api_ || !api_->log) return;
    const std::string line = std::string("expeditions: ") + text;
    api_->log(reinterpret_cast<const uint8_t*>(line.data()), line.size());
}

void Mod::send(void* ctx, uint64_t client, const json::Value& message) const {
    if (!api_ || !api_->send || !ctx) return;
    const std::string text = message.dump();
    api_->send(ctx, client, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

void Mod::notify_all(void* ctx, const std::string& text) const {
    json::Value m = json::Value::object();
    m.set("op", "notify");
    m.set("owner", kId);
    m.set("text", text);
    m.set("mod", "ui");
    for (const auto& [client, view] : views_) send(ctx, client, m);
    log(text);
}

void Mod::load(const sl::HostApi* api, const std::string& dir) {
    api_ = api;
    dir_ = dir.empty() ? platform::game_dir() : dir;
    load_config();
    load_state();
    last_tick_ = clock_();
    log(std::string("version ") + kVersion + " loaded: " + std::to_string(config_.professions.size()) + " trades, " + std::to_string(list_.size()) + " expedition(s) away");
    if (!api || api->version < 7) log("this shroudlink core is too old (HostApi v7 needed to create items)");
}

void Mod::load_config() {
    const std::string path = dir_ + kConfigFile;
    std::string text;
    config_broken_ = false;
    if (!platform::read_file(path, text)) {
        text = kDefaultConfig;
        if (platform::write_file_atomic(path, text)) log("wrote the default settings to " + std::string(kConfigFile));
    }
    bool ok = false;
    std::string error;
    json::Value v = json::parse(text, &ok, &error);
    if (!ok) {
        // a broken file is left alone for the player to fix; the defaults run meanwhile
        config_broken_ = true;
        log("expeditions_config.json: " + error + "; using the defaults");
        v = json::parse(kDefaultConfig);
        config_ = parse_config(v);
        config_.problems.insert(config_.problems.begin(), L("ошибка в файле настроек (", "error in the settings file (") + error + L("), работают настройки по умолчанию", "), the defaults are in use"));
    } else {
        config_ = parse_config(v);
    }
    config_json_ = v;
    lang::set(config_.language);
    for (const auto& p : config_.problems) log("config: " + p);
}

void Mod::save_config() {
    if (config_broken_) return;  // never overwrite a file the player is fixing
    config_json_ = config_to_json(config_, config_json_);
    platform::write_file_atomic(dir_ + kConfigFile, config_json_.dump(true));
}

void Mod::load_state() {
    list_.clear();
    history_.clear();
    survivors_.clear();
    visited_.clear();
    visits_known_ = false;
    std::string text;
    if (!platform::read_file(dir_ + kStateFile, text)) return;
    bool ok = false;
    const json::Value v = json::parse(text, &ok);
    if (!ok) {
        log("expeditions_state.json is unreadable; starting empty");
        return;
    }
    const int64_t saved = static_cast<int64_t>(v["saved_unix"].as_number(0));
    const double offline = config_.offline_time && saved > 0 ? std::max<int64_t>(0, platform::unix_now() - saved) : 0;
    auto f3 = [](const json::Value& a, double out[3]) {
        for (int i = 0; i < 3; ++i) out[i] = a[static_cast<size_t>(i)].as_number(0);
    };
    for (const auto& e : v["expeditions"].items()) {
        Expedition x;
        x.workshop = static_cast<uint32_t>(e["workshop"].as_number(0));
        if (!x.workshop) continue;
        x.who = e["who"].as_string();
        x.region = e["region"].as_string();
        x.duration_s = e["duration_s"].as_number(0);
        x.elapsed_s = e["elapsed_s"].as_number(0);
        x.recalled = e["recalled"].as_bool(false);
        x.rolled = e["rolled"].as_bool(false);
        x.pending = pairs_of(e["pending"]);
        x.delivered = static_cast<uint32_t>(e["delivered"].as_number(0));
        x.walked = e["walked"].as_bool(false);
        x.home_restored = e["home_restored"].as_bool(false);
        const json::Value& h = e["home"];
        for (int i = 0; i < 3; ++i) x.original.pos[i] = static_cast<float>(h["pos"][static_cast<size_t>(i)].as_number(0));
        x.original.override_range = h["override_range"].as_bool(false);
        x.original.range = static_cast<float>(h["range"].as_number(0));
        for (int i = 0; i < 3; ++i) x.site[i] = static_cast<float>(e["site"][static_cast<size_t>(i)].as_number(0));
        x.has_base = e["has_base"].as_bool(false);
        f3(e["base_center"], x.base_center);
        x.base_half = e["base_half"].as_number(0);
        f3(e["start_pos"], x.start_pos);
        x.told_full = e["told_full"].as_bool(false);
        x.level = static_cast<int>(e["level"].as_number(1));
        x.rations_kind = static_cast<int>(e["rations_kind"].as_number(-1));
        x.rations = static_cast<uint32_t>(e["rations"].as_number(0));
        x.repeat = e["repeat"].as_bool(false);
        x.event = lang::event_id(e["event"].string_or(""));
        x.loot_factor = e["loot_factor"].as_number(1);
        x.loot = pairs_of(e["loot"]);
        if (!x.time_up()) x.elapsed_s = std::min(x.duration_s, x.elapsed_s + offline);
        list_.push_back(x);
    }
    for (const auto& h : v["history"].items()) {
        Finished f;
        f.who = h["who"].as_string();
        f.region = h["region"].as_string();
        f.items = static_cast<uint32_t>(h["items"].as_number(0));
        f.at = static_cast<int64_t>(h["at"].as_number(0));
        f.workshop = static_cast<uint32_t>(h["workshop"].as_number(0));
        f.minutes = h["minutes"].as_number(0);
        f.level = static_cast<int>(h["level"].as_number(1));
        f.level_up = h["level_up"].as_bool(false);
        f.event = lang::event_id(h["event"].string_or(""));
        f.loot = pairs_of(h["loot"]);
        history_.push_back(f);
    }
    for (const auto& [key, sv] : v["survivors"].members()) {
        Survivor sv2;
        sv2.minutes = sv["minutes"].as_number(0);
        sv2.trips = static_cast<uint32_t>(sv["trips"].as_number(0));
        sv2.items = static_cast<uint64_t>(sv["items"].as_number(0));
        survivors_[static_cast<uint32_t>(std::strtoul(key.c_str(), nullptr, 10))] = sv2;
    }
    visits_known_ = v["visited"].is_array();
    for (const auto& r : v["visited"].items()) {
        const int area = regions::by_english(r.as_string());
        if (area >= 0) visited_.insert(area);
    }
}

void Mod::save_state() {
    json::Value v = json::Value::object();
    v.set("version", 2);
    v.set("saved_unix", static_cast<double>(platform::unix_now()));
    json::Value list = json::Value::array();
    auto a3 = [](const double* p) {
        json::Value a = json::Value::array();
        for (int i = 0; i < 3; ++i) a.push(p[i]);
        return a;
    };
    for (const auto& x : list_) {
        json::Value e = json::Value::object();
        e.set("workshop", x.workshop);
        e.set("who", x.who);
        e.set("region", x.region);
        e.set("duration_s", x.duration_s);
        e.set("elapsed_s", x.elapsed_s);
        e.set("recalled", x.recalled);
        e.set("rolled", x.rolled);
        e.set("pending", pairs_json(x.pending));
        e.set("delivered", x.delivered);
        e.set("walked", x.walked);
        e.set("home_restored", x.home_restored);
        json::Value h = json::Value::object();
        json::Value pos = json::Value::array();
        for (const float p : x.original.pos) pos.push(static_cast<double>(p));
        h.set("pos", pos);
        h.set("override_range", x.original.override_range);
        h.set("range", static_cast<double>(x.original.range));
        e.set("home", h);
        json::Value site = json::Value::array();
        for (const float p : x.site) site.push(static_cast<double>(p));
        e.set("site", site);
        e.set("has_base", x.has_base);
        e.set("base_center", a3(x.base_center));
        e.set("base_half", x.base_half);
        e.set("start_pos", a3(x.start_pos));
        e.set("told_full", x.told_full);
        e.set("level", x.level);
        e.set("rations_kind", x.rations_kind);
        e.set("rations", x.rations);
        e.set("repeat", x.repeat);
        e.set("event", x.event);
        e.set("loot_factor", x.loot_factor);
        e.set("loot", pairs_json(x.loot));
        list.push(e);
    }
    v.set("expeditions", list);
    json::Value hist = json::Value::array();
    for (const auto& h : history_) {
        json::Value e = json::Value::object();
        e.set("who", h.who);
        e.set("region", h.region);
        e.set("items", h.items);
        e.set("at", static_cast<double>(h.at));
        e.set("workshop", h.workshop);
        e.set("minutes", h.minutes);
        e.set("level", h.level);
        e.set("level_up", h.level_up);
        e.set("event", h.event);
        e.set("loot", pairs_json(h.loot));
        hist.push(e);
    }
    v.set("history", hist);
    json::Value surv = json::Value::object();
    for (const auto& [workshop, sv] : survivors_) {
        json::Value e = json::Value::object();
        e.set("minutes", sv.minutes);
        e.set("trips", sv.trips);
        e.set("items", static_cast<double>(sv.items));
        surv.set(std::to_string(workshop), e);
    }
    v.set("survivors", surv);
    json::Value visited = json::Value::array();
    for (const int r : visited_) visited.push(regions::english(r));
    if (visits_known_) v.set("visited", visited);
    if (!platform::write_file_atomic(dir_ + kStateFile, v.dump(true))) log("could not write expeditions_state.json");
    dirty_ = false;
    since_save_ = 0;
}

void Mod::send_entries(void* ctx, uint64_t client) const {
    json::Value entries = json::Value::object();
    entries.set("op", "entries");
    entries.set("owner", kId);
    json::Value list = json::Value::array();
    json::Value e = json::Value::object();
    e.set("id", "open");
    e.set("label", L("Экспедиции", "Expeditions"));
    list.push(e);
    entries.set("entries", list);
    entries.set("mod", "ui");
    send(ctx, client, entries);
}

void Mod::join(const sl::HostApi* api, void* ctx, uint64_t client) {
    api_ = api;
    views_[client] = View{};
    hud_sent_.erase(client);
    send_entries(ctx, client);
}

std::string Mod::who(uint32_t workshop, const std::string& configured) const { return lang::trade(workshop, configured); }

std::string Mod::where(uint32_t workshop, const std::string& region_name) const {
    if (const Profession* prof = config_.profession(workshop)) {
        for (const auto& r : prof->regions) {
            if (r.name == region_name) return lang::region(r.area, r.name);
        }
    }
    for (const auto& prof : config_.professions) {
        for (const auto& r : prof.regions) {
            if (r.name == region_name) return lang::region(r.area, r.name);
        }
    }
    return lang::region(-1, region_name);
}

void Mod::leave(uint64_t client) {
    views_.erase(client);
    hud_sent_.erase(client);
}

uint32_t Mod::player_of(uint64_t client) const {
    const World w{api_};
    if (w.has(4) && api_->player_of) {
        if (const uint32_t p = api_->player_of(client)) return p;
    }
    // alone in the world (singleplayer): that is the player
    const auto players = w.players();
    return players.size() == 1 ? players[0] : 0;
}

std::vector<std::pair<uint32_t, uint32_t>> Mod::survivors_near(uint32_t player) const {
    const World w{api_};
    std::vector<std::pair<uint32_t, uint32_t>> found;
    for (const uint32_t npc : w.entities_near(player, kSurvivorRadius, sl::comp::ActiveNpcState)) {
        const uint32_t workshop = w.workshop_of(npc);
        if (!workshop || !config_.profession(workshop)) continue;
        auto it = std::find_if(found.begin(), found.end(), [&](const auto& f) { return f.second == workshop; });
        if (it == found.end()) {
            found.emplace_back(npc, workshop);
        } else if (w.is_assistant(it->first) && !w.is_assistant(npc)) {
            it->first = npc;  // the survivor goes, their assistant stays at work
        }
    }
    return found;
}

Expedition* Mod::find(uint32_t workshop) {
    for (auto& x : list_) {
        if (x.workshop == workshop) return &x;
    }
    return nullptr;
}

// ---- levels, regions, rations ----

int Mod::level_of(uint32_t workshop) const {
    if (!config_.experience) return 1;
    const auto it = survivors_.find(workshop);
    double minutes = it == survivors_.end() ? 0 : it->second.minutes;
    int level = 1;
    while (level < config_.max_level && minutes >= config_.minutes_per_level * level) {
        minutes -= config_.minutes_per_level * level;
        ++level;
    }
    return level;
}

// minutes into the current level and the minutes the level takes (0: the last level)
static std::pair<double, double> level_progress(const Config& c, double minutes) {
    int level = 1;
    while (level < c.max_level && minutes >= c.minutes_per_level * level) {
        minutes -= c.minutes_per_level * level;
        ++level;
    }
    return {minutes, level < c.max_level ? c.minutes_per_level * level : 0};
}

double Mod::level_bonus(int level) const { return config_.experience ? 1 + config_.loot_per_level * (level - 1) : 1; }

double Mod::hurt_chance(const Region& r, double hours, int level) const {
    if (!config_.events) return 0;
    const double base = 1 - std::pow(1 - std::clamp(r.danger, 0.0, 0.95), std::max(0.0, hours));
    const double factor = config_.experience ? std::max(0.1, 1 - config_.risk_per_level * (level - 1)) : 1;
    return base * factor;
}

bool Mod::region_open(const Region& r) const {
    return !config_.unlock_by_visiting || r.area < 0 || r.area == regions::kSpringlands || visited_.count(r.area) > 0;
}

const Region* Mod::region_of(const Expedition& x) const {
    const Profession* prof = config_.profession(x.workshop);
    if (!prof) return nullptr;
    for (const auto& r : prof->regions) {
        if (r.name == x.region) return &r;
    }
    return nullptr;
}

std::vector<uint32_t> Mod::chests_around(uint32_t entity) const {
    const World w{api_};
    std::vector<uint32_t> chests;
    uint32_t altar = 0;
    double center[3];
    double half = 0;
    if (w.base_of(entity, center, half, &altar)) {
        for (const uint32_t c : w.inventories_near(altar, half * 1.75)) {
            double p[3];
            if (!w.position(c, p)) continue;
            if (std::fabs(p[0] - center[0]) <= half && std::fabs(p[1] - center[1]) <= half && std::fabs(p[2] - center[2]) <= half && w.storage(c))
                chests.push_back(c);
        }
    } else {
        const auto players = w.players();
        for (const uint32_t c : w.inventories_near(entity, kChestRadius)) {
            if (c != entity && std::find(players.begin(), players.end(), c) == players.end() && w.storage(c)) chests.push_back(c);
        }
    }
    return chests;
}

std::array<uint32_t, 3> Mod::rations_in(const std::vector<uint32_t>& chests) const {
    const World w{api_};
    std::array<uint32_t, 3> have{};
    for (const uint32_t c : chests) {
        for (const auto& slot : w.inventory(c)) {
            const int kind = supplies::kind_of(slot.item);
            if (kind >= 0 && slot.count) have[static_cast<size_t>(kind)] += slot.count;
        }
    }
    return have;
}

uint32_t Mod::rations_for(double hours) const {
    if (!config_.supplies || config_.rations_per_hour <= 0) return 0;
    return std::max<uint32_t>(1, static_cast<uint32_t>(std::ceil(hours * config_.rations_per_hour - 1e-9)));
}

// ---- sending and coming back ----

std::string Mod::launch(void* ctx, uint32_t npc, uint32_t player, const Profession& prof, const Region& region, double duration_s,
                        int rations, bool repeat, Expedition& x) {
    (void)ctx;
    const World w{api_};
    const uint32_t need = rations_for(duration_s / 3600.0);
    std::vector<uint32_t> chests;
    rations = std::clamp(rations, 0, supplies::kKinds - 1);
    if (need) {
        chests = chests_around(npc);
        const uint32_t have = rations_in(chests)[static_cast<size_t>(rations)];
        if (have < need)
            return L("Не хватает припасов (", "Not enough rations (") + std::string(supplies::kind_name(rations)) + L("): нужно ", "): need ") + std::to_string(need) + L(", в сундуках ", ", the chests hold ") +
                   std::to_string(have) + ".";
    }

    x = Expedition{};
    x.workshop = prof.workshop;
    x.who = prof.name;
    x.region = region.name;
    x.duration_s = duration_s;
    x.level = level_of(prof.workshop);
    x.repeat = repeat;
    double npc_pos[3] = {0, 0, 0};
    w.position(npc, npc_pos);
    double player_pos[3] = {0, 0, 0};
    if (!w.position(player, player_pos)) std::copy(npc_pos, npc_pos + 3, player_pos);
    for (int i = 0; i < 3; ++i) x.start_pos[i] = npc_pos[i];
    x.has_base = w.base_of(npc, x.base_center, x.base_half);

    // rations: the plainest food of the kind first
    if (need) {
        struct Food {
            uint32_t chest;
            sl::Slot slot;
            int tier;
        };
        std::vector<Food> food;
        for (const uint32_t c : chests) {
            for (const auto& slot : w.inventory(c)) {
                if (slot.count && supplies::kind_of(slot.item) == rations) food.push_back({c, slot, supplies::tier_of(slot.item)});
            }
        }
        std::stable_sort(food.begin(), food.end(), [](const Food& a, const Food& b) { return a.tier < b.tier; });
        uint32_t left = need;
        for (const auto& f : food) {
            if (!left) break;
            left -= std::min(left, api_->remove_items(f.chest, f.slot.id, std::min(left, f.slot.count)));
        }
        x.rations = need - left;
        x.rations_kind = rations;
    }

    if (config_.walk_away && w.home(npc, x.original)) {
        // away from the altar (or from the player), out past the edge of the base
        double from[3];
        double dist;
        if (x.has_base) {
            for (int i = 0; i < 3; ++i) from[i] = x.base_center[i];
            dist = x.base_half + config_.walk_beyond_m;
        } else {
            for (int i = 0; i < 3; ++i) from[i] = player_pos[i];
            dist = 30 + config_.walk_beyond_m;
        }
        double dx = npc_pos[0] - from[0], dz = npc_pos[2] - from[2];
        const double len = std::sqrt(dx * dx + dz * dz);
        if (len < 0.5) {
            dx = 1;
            dz = 0;
        } else {
            dx /= len;
            dz /= len;
        }
        x.site[0] = static_cast<float>(from[0] + dx * dist);
        x.site[1] = static_cast<float>(npc_pos[1]);
        x.site[2] = static_cast<float>(from[2] + dz * dist);
        Home away;
        for (int i = 0; i < 3; ++i) away.pos[i] = x.site[i];
        away.override_range = true;
        away.range = kAwayRange;
        if (w.set_home(npc, away)) {
            w.nudge(npc);
            x.walked = true;
        }
    }
    log(x.who + " left for " + x.region + " (" + format_minutes(x.duration_s) + ", level " + std::to_string(x.level) + ", " +
        std::to_string(x.rations) + " rations)" + (x.walked ? ", walking off" : "") + (x.repeat ? ", repeating" : ""));
    return "";
}

std::string Mod::start(void* ctx, uint64_t client, uint32_t workshop, int region, int duration, int rations, bool repeat) {
    const World w{api_};
    if (!w.has(7)) return L("Ядро shroudlink слишком старое для экспедиций.", "This shroudlink core is too old for expeditions.");
    const Profession* prof = config_.profession(workshop);
    if (!prof || region < 0 || region >= static_cast<int>(prof->regions.size())) return L("Нет такого маршрута.", "No such route.");
    if (find(workshop)) return who(workshop, prof->name) + L(" уже в походе.", " is already away.");
    const Region& r = prof->regions[static_cast<size_t>(region)];
    if (!region_open(r)) return L("Сначала побывайте в этом регионе сами: ", "Visit this region yourself first: ") + lang::region(r.area, r.name) + ".";
    const uint32_t player = player_of(client);
    if (!player) return L("Откройте любой сундук, чтобы мод понял, кто вы.", "Open any chest once so the mod knows who you are.");
    uint32_t npc = 0;
    for (const auto& [e, ws] : survivors_near(player)) {
        if (ws == workshop) npc = e;
    }
    if (!npc) return who(workshop, prof->name) + L(" далеко: подойдите ближе.", " is too far away: come closer.");
    const int d = std::clamp(duration, 0, static_cast<int>(config_.durations_min.size()) - 1);
    Expedition x;
    const std::string why = launch(ctx, npc, player, *prof, r, config_.durations_min[static_cast<size_t>(d)] * 60.0, rations, repeat, x);
    if (!why.empty()) return why;
    list_.push_back(x);
    save_state();
    notify_all(ctx, who(x.workshop, x.who) + L(" отправляется в поход: ", " sets out: ") + where(x.workshop, x.region) + ", " + format_minutes(x.duration_s) + ".");
    update_hud(ctx, true);
    return who(x.workshop, x.who) + L(" ушёл в поход: ", " set out: ") + where(x.workshop, x.region) + L(", вернётся через ", ", back in ") + format_minutes(x.duration_s) + ".";
}

void Mod::recall(void* ctx, uint32_t workshop) {
    Expedition* x = find(workshop);
    if (!x || x->time_up()) return;
    x->recalled = true;
    x->repeat = false;
    dirty_ = true;
    log(x->who + " recalled after " + format_minutes(x->elapsed_s));
    notify_all(ctx, who(x->workshop, x->who) + L(" возвращается из похода раньше срока.", " is coming back early."));
    update_hud(ctx, true);
}

void Mod::advance(double seconds) {
    for (auto& x : list_) {
        if (!x.time_up()) x.elapsed_s = std::min(x.duration_s, x.elapsed_s + seconds);
    }
}

std::vector<std::pair<uint32_t, uint32_t>> Mod::roll(const Region& region, double hours, double factor) {
    std::vector<std::pair<uint32_t, uint32_t>> out;
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for (const auto& e : region.loot) {
        double total = 0;
        double left = hours;
        while (left > 1e-9) {
            const double part = std::min(1.0, left);
            if (unit(rng_) < e.chance) total += (e.min + (e.max - e.min) * unit(rng_)) * part;
            left -= part;
        }
        total *= config_.multiplier * factor;
        uint32_t n = static_cast<uint32_t>(std::floor(total));
        if (unit(rng_) < total - std::floor(total)) ++n;
        if (n == 0) continue;
        auto it = std::find_if(out.begin(), out.end(), [&](const auto& p) { return p.first == e.item; });
        if (it != out.end()) it->second += n;
        else out.emplace_back(e.item, n);
    }
    return out;
}

std::vector<std::pair<uint32_t, uint32_t>> Mod::expected(const Region& region, double hours, double factor) const {
    std::vector<std::pair<uint32_t, uint32_t>> out;
    for (const auto& e : region.loot) {
        const double n = (e.min + e.max) / 2 * e.chance * hours * config_.multiplier * factor;
        const uint32_t count = static_cast<uint32_t>(std::lround(n));
        if (count == 0) continue;
        auto it = std::find_if(out.begin(), out.end(), [&](const auto& p) { return p.first == e.item; });
        if (it != out.end()) it->second += count;
        else out.emplace_back(e.item, count);
    }
    return out;
}

// The regions players are in: their routes open (the first visit of each tells everyone).
void Mod::track_visits(void* ctx) {
    const World w{api_};
    if (!visits_known_) {
        // first run with region tracking: where expeditions left from and went to counts as visited
        for (const auto& x : list_) {
            const int r = regions::at(x.has_base ? x.base_center[0] : x.start_pos[0], x.has_base ? x.base_center[2] : x.start_pos[2]);
            if (r >= 0) visited_.insert(r);
            if (const Region* to = region_of(x); to && to->area >= 0) visited_.insert(to->area);
        }
        for (const auto& h : history_) {
            for (const auto& prof : config_.professions) {
                for (const auto& reg : prof.regions) {
                    if (reg.name == h.region && reg.area >= 0) visited_.insert(reg.area);
                }
            }
        }
        visits_known_ = true;
        dirty_ = true;
    }
    for (const uint32_t p : w.players()) {
        double pos[3];
        if (!w.position(p, pos)) continue;
        const int r = regions::at(pos[0], pos[2]);
        if (r < 0) continue;
        auto it = player_region_.find(p);
        if (it == player_region_.end() || it->second != r) {
            player_region_[p] = r;
            log(std::string("a player is in ") + regions::english(r) + " (" + std::to_string(static_cast<long>(pos[0])) + ", " +
                std::to_string(static_cast<long>(pos[2])) + ")");
        }
        if (!visited_.insert(r).second) continue;
        dirty_ = true;
        if (!config_.unlock_by_visiting || r == regions::kSpringlands) continue;
        std::string name;
        for (const auto& prof : config_.professions) {
            for (const auto& reg : prof.regions) {
                if (reg.area == r && name.empty()) name = lang::region(reg.area, reg.name);
            }
        }
        if (!name.empty()) notify_all(ctx, L("Экспедиции: открыты маршруты в регион «", "Expeditions: routes to ") + name + L("».", " are open."));
    }
}

void Mod::finish(void* ctx, Expedition& x) {
    const double minutes = (x.recalled ? x.elapsed_s : x.duration_s) / 60.0;
    const int before = level_of(x.workshop);
    Survivor& sv = survivors_[x.workshop];
    sv.minutes += minutes;
    sv.trips += 1;
    sv.items += x.delivered;
    const int after = level_of(x.workshop);
    Finished f;
    f.who = x.who;
    f.region = x.region;
    f.items = x.delivered;
    f.at = platform::unix_now();
    f.workshop = x.workshop;
    f.minutes = minutes;
    f.level = after;
    f.level_up = after > before;
    f.event = x.event;
    f.loot = x.loot;
    history_.insert(history_.begin(), f);
    if (history_.size() > kHistory) history_.resize(kHistory);
    std::string text = who(x.workshop, x.who) + L(" вернулся из похода (", " is back (") + where(x.workshop, x.region) + ")";
    if (!x.event.empty()) text += ": " + lang::event_text(x.event);
    text += L(". В сундуках ", ". In the chests: ") + std::to_string(x.delivered) + " " + items_word(x.delivered) + ".";
    if (f.level_up) text += " " + who(x.workshop, x.who) + L(" теперь ", " is now level ") + std::to_string(after) + L(" уровня!", "!");
    notify_all(ctx, text);
    dirty_ = true;
}

void Mod::step(void* ctx) {
    const World w{api_};
    track_visits(ctx);
    const auto players = w.players();
    for (size_t k = 0; k < list_.size();) {
        Expedition& x = list_[k];
        if (!x.time_up()) {
            ++k;
            continue;
        }
        // a player at the base the expedition left from
        uint32_t at_base = 0;
        for (const uint32_t p : players) {
            double pos[3];
            if (!w.position(p, pos)) continue;
            bool here;
            if (x.has_base) {
                here = std::fabs(pos[0] - x.base_center[0]) <= x.base_half + 8 && std::fabs(pos[2] - x.base_center[2]) <= x.base_half + 8 &&
                       std::fabs(pos[1] - x.base_center[1]) <= x.base_half + 60;
            } else {
                const double dx = pos[0] - x.start_pos[0], dy = pos[1] - x.start_pos[1], dz = pos[2] - x.start_pos[2];
                here = dx * dx + dy * dy + dz * dz <= 50.0 * 50.0;
            }
            if (here) {
                at_base = p;
                break;
            }
        }
        if (!at_base) {
            ++k;
            continue;
        }

        // home again: the home point they had comes back
        uint32_t npc = 0;
        if (x.walked && !x.home_restored) {
            for (const uint32_t e : w.entities_near(at_base, kNpcSearchRadius, sl::comp::ActiveNpcState)) {
                if (w.workshop_of(e) != x.workshop) continue;
                Home h;
                if (w.home(e, h) && std::fabs(h.pos[0] - x.site[0]) < 6 && std::fabs(h.pos[2] - x.site[2]) < 6) {
                    npc = e;
                    break;
                }
            }
            if (npc && w.set_home(npc, x.original)) {
                w.nudge(npc);
                x.home_restored = true;
                dirty_ = true;
                log(x.who + " walks home");
            } else if (!npc) {
                // not the one we moved (it may have been moved by someone else): nothing to give back
                bool any = false;
                for (const uint32_t e : w.entities_near(at_base, kNpcSearchRadius, sl::comp::ActiveNpcState)) any = any || w.workshop_of(e) == x.workshop;
                if (any) {
                    x.home_restored = true;
                    dirty_ = true;
                    log(x.who + ": their home point was changed meanwhile, left as it is");
                }
            }
        }

        if (!x.rolled) {
            const Region* region = region_of(x);
            const double hours = (x.recalled ? x.elapsed_s : x.duration_s) / 3600.0;
            double factor = level_bonus(x.level) * (1 + supplies::bonus(x.rations_kind));
            if (region && config_.events && hours > 0) {
                std::uniform_real_distribution<double> unit(0.0, 1.0);
                const double cache = 0.06 + (config_.experience ? 0.01 * (x.level - 1) : 0);
                if (unit(rng_) < hurt_chance(*region, hours, x.level)) {
                    factor *= 0.5;
                    x.event = "hurt";
                } else if (const double u = unit(rng_); u < cache) {
                    factor *= 1.5;
                    x.event = "cache";
                } else if (u < cache + 0.1) {
                    factor *= 1.25;
                    x.event = "rich";
                } else if (u < cache + 0.18) {
                    factor *= 0.8;
                    x.event = "weather";
                }
            }
            x.loot_factor = factor;
            if (region) x.pending = roll(*region, hours, factor);
            x.loot = x.pending;
            x.rolled = true;
            dirty_ = true;
            if (!x.event.empty()) log(x.who + ": " + x.event);
        }

        // the loot into the base's chests: chests holding the item first
        const std::vector<uint32_t> chests = chests_around(at_base);
        uint32_t placed = 0;
        for (auto& [item, count] : x.pending) {
            if (count == 0) continue;
            std::vector<uint32_t> order;
            for (const uint32_t c : chests) {
                const auto slots = w.inventory(c);
                if (std::any_of(slots.begin(), slots.end(), [&](const sl::Slot& s) { return s.item == item && s.count > 0; })) order.push_back(c);
            }
            for (const uint32_t c : chests) {
                if (std::find(order.begin(), order.end(), c) == order.end()) order.push_back(c);
            }
            for (const uint32_t c : order) {
                if (count == 0) break;
                const uint32_t got = std::min(count, w.give(c, item, count));
                count -= got;
                placed += got;
            }
        }
        if (placed) {
            x.delivered += placed;
            dirty_ = true;
        }
        x.pending.erase(std::remove_if(x.pending.begin(), x.pending.end(), [](const auto& p) { return p.second == 0; }), x.pending.end());

        uint32_t left = 0;
        for (const auto& p : x.pending) left += p.second;
        if (left > 0) {
            // no chest in sight (not loaded yet, right after a world loads): the loot waits without a word
            if (!x.told_full && !chests.empty()) {
                notify_all(ctx, who(x.workshop, x.who) + L(" вернулся, но в сундуках нет места для ", " is back, but the chests have no room for ") + std::to_string(left) + " " + items_word(left) +
                                    L(". Освободите место — добыча подождёт.", ". Make room, the loot will wait."));
                x.told_full = true;
                dirty_ = true;
            }
            ++k;
            continue;
        }
        if (x.walked && !x.home_restored) {
            ++k;  // the loot is in, they still have to be found to walk home
            continue;
        }
        Expedition done = x;
        list_.erase(list_.begin() + static_cast<long>(k));
        finish(ctx, done);
        if (done.repeat) {
            // off again, the same way
            const Profession* prof = config_.profession(done.workshop);
            const Region* region = region_of(done);
            uint32_t survivor = 0;
            for (const uint32_t e : w.entities_near(at_base, kNpcSearchRadius, sl::comp::ActiveNpcState)) {
                if (w.workshop_of(e) != done.workshop) continue;
                if (!survivor || (w.is_assistant(survivor) && !w.is_assistant(e))) survivor = e;
            }
            std::string why = !prof || !region ? L("маршрута больше нет в настройках", "the route is no longer in the settings") : !survivor ? L("его не найти рядом с базой", "they cannot be found near the base") : "";
            Expedition again;
            if (why.empty()) why = launch(ctx, survivor, at_base, *prof, *region, done.duration_s, done.rations_kind < 0 ? 0 : done.rations_kind, true, again);
            if (why.empty()) {
                list_.insert(list_.begin() + static_cast<long>(k), again);
                ++k;
                notify_all(ctx, who(again.workshop, again.who) + L(" снова уходит в поход: ", " sets out again: ") + where(again.workshop, again.region) + ", " + format_minutes(again.duration_s) + ".");
            } else {
                notify_all(ctx, who(done.workshop, done.who) + L(" не пошёл снова: ", " did not set out again: ") + why);
            }
        }
        dirty_ = true;
        update_hud(ctx, true);
    }
}

// The panel on the screen: who is away, how long still, a bar for the time gone.
void Mod::update_hud(void* ctx, bool force) {
    json::Value rows = json::Value::array();
    if (config_.hud) {
        for (const auto& x : list_) {
            json::Value r = json::Value::object();
            r.set("label", who(x.workshop, x.who));
            std::string value;
            if (!x.time_up()) value = format_minutes(x.duration_s - x.elapsed_s);
            else if (!x.pending.empty() && x.told_full) value = L("нет места", "no room");
            else value = L("ждёт вас", "waiting");
            r.set("value", value);
            const double fraction = x.duration_s > 0 ? std::clamp(x.elapsed_s / x.duration_s, 0.0, 1.0) : 1.0;
            r.set("fraction", std::max(0.02, std::round(fraction * 50) / 50));
            rows.push(r);
        }
    }
    json::Value m = json::Value::object();
    m.set("op", "hud");
    m.set("owner", kId);
    m.set("id", "away");
    m.set("title", L("Экспедиции", "Expeditions"));
    m.set("rows", rows);
    m.set("mod", "ui");
    const std::string text = m.dump();
    for (const auto& [client, view] : views_) {
        auto it = hud_sent_.find(client);
        if (!force && it != hud_sent_.end() && it->second == text) continue;
        if (it == hud_sent_.end() && rows.size() == 0) {
            hud_sent_[client] = text;  // nothing shown yet, nothing to take away
            continue;
        }
        send(ctx, client, m);
        hud_sent_[client] = text;
    }
}

void Mod::tick(const sl::HostApi* api, void* ctx) {
    api_ = api;
    const double now = clock_();
    // a loading screen or a hitch does not count as time away
    const double dt = std::clamp(now - last_tick_, 0.0, 5.0);
    last_tick_ = now;
    advance(dt);
    since_step_ += dt;
    since_save_ += dt;
    since_refresh_ += dt;
    if (since_step_ >= kStepEvery) {
        since_step_ = 0;
        step(ctx);
        update_hud(ctx);
    }
    if (dirty_ || (since_save_ >= kSaveEvery && !list_.empty())) save_state();
    if (since_refresh_ >= kRefreshEvery) {
        since_refresh_ = 0;
        if (!list_.empty()) {
            for (auto& [client, view] : views_) {
                if (view.screen == View::Screen::Main && view.open_window == "main") show(ctx, client);
            }
        }
    }
}

// ---- windows ----

static std::string danger_word(double danger) {
    if (danger < 0.07) return L("низкая", "low");
    if (danger < 0.15) return L("средняя", "medium");
    if (danger < 0.25) return L("высокая", "high");
    return L("очень высокая", "very high");
}

static std::string percent(double fraction) { return std::to_string(static_cast<int>(std::lround(fraction * 100))) + "%"; }

void Mod::main_window(View& view, json::Value& content, uint32_t player) {
    if (!view.message.empty()) push_text(content, view.message);
    view.active.clear();
    if (!list_.empty()) {
        content.push(node_heading(L("В походе", "Away")));
        std::vector<std::string> rows;
        for (const auto& x : list_) {
            std::string status;
            if (!x.time_up()) status = L("через ", "in ") + format_minutes(x.duration_s - x.elapsed_s);
            else if (!x.pending.empty() && x.told_full) status = L("нет места в сундуках", "no room in the chests");
            else status = L("ждёт вас на базе", "waiting for you at the base");
            if (x.repeat) status += L(", повтор", ", repeating");
            rows.push_back(cells({who(x.workshop, x.who), where(x.workshop, x.region), status}));
            view.active.push_back(x.workshop);
        }
        content.push(node_list("active", rows));
        push_caption(content, L("Нажмите на поход, чтобы отозвать выжившего или остановить повтор", "Click an expedition to call the survivor back or stop repeating"));
    }

    content.push(node_heading(L("Отправить в поход", "Send out")));
    view.survivors.clear();
    if (!player) {
        push_caption(content, L("Откройте любой сундук один раз, чтобы мод понял, кто вы", "Open any chest once so the mod knows who you are"));
    } else {
        std::vector<std::string> rows;
        for (const auto& [npc, workshop] : survivors_near(player)) {
            if (find(workshop)) continue;
            const Profession* p = config_.profession(workshop);
            const std::string what = lang::brings(workshop);
            const std::string about = what.empty() ? std::to_string(p->regions.size()) + " " + lang::routes_word(p->regions.size()) : what;
            if (config_.experience) rows.push_back(cells({who(workshop, p->name), L("ур. ", "lv. ") + std::to_string(level_of(workshop)), about}));
            else rows.push_back(cells({who(workshop, p->name), about}));
            view.survivors.push_back(workshop);
        }
        if (rows.empty()) {
            push_caption(content, L("Рядом нет свободных выживших: подойдите к ним на базе", "No free survivors nearby: go to them at the base"));
        } else {
            content.push(node_list("survivors", rows));
            push_caption(content, L("Выберите, кого отправить: добыча его ремесла придёт в сундуки базы", "Pick who goes: the loot of their trade goes into the base's chests"));
        }
    }
    if (!history_.empty()) {
        content.push(node_heading(L("Недавно вернулись", "Recently back")));
        std::vector<std::string> rows;
        for (const auto& h : history_) {
            std::string what = std::to_string(h.items) + " " + items_word(h.items);
            if (h.level_up) what += L(", новый уровень", ", new level");
            rows.push_back(cells({who(h.workshop, h.who), where(h.workshop, h.region), what}));
        }
        content.push(node_list("history", rows));
        push_caption(content, L("Нажмите на запись, чтобы увидеть добычу", "Click an entry to see the loot"));
    }
    content.push(node_row({node_button("settings", L("Настройки", "Settings")), node_button("refresh", L("Обновить", "Refresh"))}));
}

void Mod::send_window(View& view, json::Value& content, std::string& title, uint32_t player) {
    const Profession* prof = config_.profession(view.workshop);
    if (!prof) {
        view.screen = View::Screen::Main;
        return;
    }
    title = who(prof->workshop, prof->name) + L(": поход", ": expedition");
    const int level = level_of(view.workshop);
    if (config_.experience) {
        const auto it = survivors_.find(view.workshop);
        const auto [into, need] = level_progress(config_, it == survivors_.end() ? 0 : it->second.minutes);
        std::vector<std::pair<std::string, std::string>> stats = {{L("Уровень", "Level"), std::to_string(level)}};
        stats.emplace_back(L("Опыт", "Experience"), need > 0 ? format_minutes(into * 60) + L(" из ", " of ") + format_minutes(need * 60) : L("наивысший уровень", "highest level"));
        if (level > 1)
            stats.emplace_back(L("Бонус", "Bonus"), "+" + percent(level_bonus(level) - 1) + L(" добычи, -", " loot, -") + percent(std::min(0.9, config_.risk_per_level * (level - 1))) + L(" риска", " risk"));
        push_stats(content, stats);
    }

    // where: routes of regions nobody has been to yet are closed
    if (view.region < 0 || view.region >= static_cast<int>(prof->regions.size()) || !region_open(prof->regions[static_cast<size_t>(view.region)])) {
        view.region = -1;
        for (size_t i = 0; i < prof->regions.size() && view.region < 0; ++i) {
            if (region_open(prof->regions[i])) view.region = static_cast<int>(i);
        }
    }
    content.push(node_heading(L("Куда", "Where")));
    std::vector<std::string> regions;
    bool closed = false;
    for (size_t i = 0; i < prof->regions.size(); ++i) {
        const Region& r = prof->regions[i];
        const std::string name = (static_cast<int>(i) == view.region ? "»  " : "    ") + lang::region(r.area, r.name);
        if (!region_open(r)) {
            regions.push_back(cells({name, L("закрыт", "closed")}));
            closed = true;
        } else if (config_.events) {
            regions.push_back(cells({name, L("опасность ", "danger ") + danger_word(r.danger)}));
        } else {
            regions.push_back(name);
        }
    }
    content.push(node_list("regions", regions));
    if (closed) push_caption(content, L("Закрытый маршрут откроется, когда вы сами побываете в этом регионе", "A closed route opens once you have been to its region yourself"));
    if (view.region < 0) {
        content.push(node_row({node_button("back", L("« Назад", "« Back"))}));
        return;
    }
    const Region& region = prof->regions[static_cast<size_t>(view.region)];

    content.push(node_heading(L("Надолго", "How long")));
    view.duration = std::clamp(view.duration, 0, static_cast<int>(config_.durations_min.size()) - 1);
    std::vector<std::string> durations;
    for (double minutes : config_.durations_min) durations.push_back(format_minutes(minutes * 60.0));
    content.push(node_tabs("durations", durations, view.duration));
    const double hours = config_.durations_min[static_cast<size_t>(view.duration)] / 60.0;
    if (config_.events) push_stats(content, {{L("Риск ранения", "Risk of injury"), percent(hurt_chance(region, hours, level))}, {L("Если ранят", "If hurt"), L("добычи вдвое меньше", "half the loot")}});

    double factor = level_bonus(level);
    const uint32_t need = rations_for(hours);
    if (need) {
        content.push(node_heading(L("Припасы", "Rations")));
        uint32_t npc = 0;
        if (player) {
            for (const auto& [e, ws] : survivors_near(player)) {
                if (ws == view.workshop) npc = e;
            }
        }
        const auto have = npc ? rations_in(chests_around(npc)) : std::array<uint32_t, 3>{};
        view.rations = std::clamp(view.rations, 0, supplies::kKinds - 1);
        std::vector<std::string> kinds;
        const char* const kTitles[supplies::kKinds] = {L("Простая еда", "Plain food"), L("Жареное", "Roasted"), L("Блюда", "Dishes")};
        for (int k = 0; k < supplies::kKinds; ++k) kinds.push_back(std::string(kTitles[k]) + " (" + std::to_string(have[static_cast<size_t>(k)]) + ")");
        content.push(node_tabs("rations", kinds, view.rations));
        const double bonus = supplies::bonus(view.rations);
        const uint32_t here = have[static_cast<size_t>(view.rations)];
        push_stats(content, {{L("Нужно", "Needed"), std::to_string(need) + L(" шт.", "")}, {L("В сундуках", "In the chests"), std::to_string(here)}, {L("Бонус", "Bonus"), bonus > 0 ? "+" + percent(bonus) + L(" добычи", " loot") : L("нет", "none")}});
        if (here < need) push_text(content, L("Не хватает еды этого вида в сундуках базы.", "Not enough food of this kind in the base's chests."));
        factor *= 1 + bonus;
    }

    content.push(node_heading(L("Примерная добыча", "Expected loot")));
    const auto loot = expected(region, hours, factor);
    if (loot.empty()) push_caption(content, L("Почти ничего: поход слишком короткий", "Next to nothing: the trip is too short"));
    else content.push(node_items("preview", loot));
    content.push(node_toggle("repeat", L("Повторять поход, пока не остановлю", "Repeat until I stop it"), view.repeat));
    if (!view.message.empty()) push_text(content, view.message);
    content.push(node_row({node_button("go", L("Отправить", "Send")), node_button("back", L("« Назад", "« Back"))}));
}

void Mod::recall_window(View& view, json::Value& content, std::string& title) {
    const Expedition* x = find(view.workshop);
    if (!x) {
        view.screen = View::Screen::Main;
        return;
    }
    title = who(x->workshop, x->who) + L(": в походе", ": away");
    std::vector<std::pair<std::string, std::string>> stats = {{L("Регион", "Region"), where(x->workshop, x->region)},
                                                              {L("Прошло", "Gone for"), format_minutes(x->elapsed_s) + L(" из ", " of ") + format_minutes(x->duration_s)}};
    if (x->rations) stats.emplace_back(L("Припасы", "Rations"), std::to_string(x->rations) + L(" шт., ", ", ") + supplies::kind_name(x->rations_kind));
    push_stats(content, stats);
    if (x->repeat) push_caption(content, L("Повтор включён: вернувшись, ", "Repeating: once back, ") + who(x->workshop, x->who) + L(" отдаст добычу и уйдёт снова", " delivers the loot and sets out again"));
    std::vector<json::Value> buttons;
    if (x->repeat) buttons.push_back(node_button("stop_repeat", L("Остановить повтор", "Stop repeating")));
    if (x->time_up()) {
        push_caption(content, L("Уже вернулся и отдаст добычу, когда вы будете на базе", "Already back: the loot comes when you are at the base"));
    } else {
        push_caption(content, L("Если отозвать сейчас, добыча будет только за пройденное время", "Called back now, they bring loot only for the time gone"));
        buttons.push_back(node_button("do_recall", L("Отозвать", "Call back")));
    }
    buttons.push_back(node_button("back", L("« Назад", "« Back")));
    content.push(node_row(buttons));
}

void Mod::report_window(View& view, json::Value& content, std::string& title) {
    if (view.report >= history_.size()) {
        view.screen = View::Screen::Main;
        return;
    }
    const Finished& h = history_[view.report];
    title = who(h.workshop, h.who) + L(": отчёт", ": report");
    const int64_t ago = std::max<int64_t>(0, platform::unix_now() - h.at);
    std::vector<std::pair<std::string, std::string>> stats = {{L("Регион", "Region"), where(h.workshop, h.region)}};
    if (h.minutes > 0) stats.emplace_back(L("В походе", "Away"), format_minutes(h.minutes * 60));
    stats.emplace_back(L("Вернулся", "Back"), format_minutes(static_cast<double>(ago)) + L(" назад", " ago"));
    push_stats(content, stats);
    std::vector<std::pair<std::string, std::string>> more;
    if (config_.experience && h.workshop) more.emplace_back(L("Уровень", "Level"), std::to_string(h.level) + (h.level_up ? L(" (новый!)", " (new!)") : ""));
    if (!h.event.empty()) more.emplace_back(L("В пути", "On the way"), lang::event_text(h.event));
    if (!more.empty()) push_stats(content, more);
    content.push(node_heading(L("Принёс", "Brought")));
    if (h.loot.empty()) push_caption(content, h.items ? std::to_string(h.items) + " " + items_word(h.items) + L(", подробностей нет: поход был до обновления мода", ", no details: the trip was before this version") : L("Ничего", "Nothing"));
    else content.push(node_items("report_items", h.loot));
    content.push(node_row({node_button("back", L("« Назад", "« Back"))}));
}

uint64_t Mod::host_client() const {
    if (fixed_host_) return fixed_host_;
    // the shroudlink addon of the player whose game runs the world shares this process and keeps
    // its session there as "slot:client" (hex); a dedicated server has no addon of its own
    const std::string session = platform::env("SHROUDLINK_CLIENT_SESSION");
    const size_t colon = session.find(':');
    if (colon == std::string::npos || colon + 1 >= session.size()) return 0;
    uint64_t id = 0;
    for (size_t i = colon + 1; i < session.size(); ++i) {
        const char ch = session[i];
        const int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
        if (d < 0 || i - colon > 16) return 0;
        id = id * 16 + static_cast<uint64_t>(d);
    }
    return id;
}

bool Mod::may_change_settings(uint64_t client) const {
    switch (config_.settings_in_game) {
    case Config::Access::Everyone: return true;
    case Config::Access::Nobody: return false;
    case Config::Access::Host: break;
    }
    const uint64_t host = host_client();
    return host != 0 && client == host;
}

std::string Mod::settings_locked_text() const {
    if (config_.settings_in_game == Config::Access::Nobody)
        return L("Настройки меняются только в файле expeditions_config.json в папке игры (или сервера).",
                 "The settings are changed only in expeditions_config.json in the game's (or the server's) folder.");
    if (!host_client())
        return L("Настройки меняет администратор сервера в файле expeditions_config.json, потом «Перечитать файл настроек».",
                 "The server's admin changes the settings in expeditions_config.json, then \"Reload the settings file\".");
    return L("Настройки может менять только тот, кто запустил мир.", "Only the player who runs the world can change the settings.");
}

void Mod::settings_window(View& view, json::Value& content, uint64_t client) {
    (void)view;
    for (const auto& p : config_.problems) push_text(content, "! " + p);
    if (!may_change_settings(client)) {
        // what is set, without the controls
        const auto on = [](bool b) { return b ? L("вкл", "on") : L("выкл", "off"); };
        push_caption(content, settings_locked_text());
        content.push(node_row({node_heading(L("Язык", "Language")),
                               node_text(config_.language == "ru" ? "Русский" : config_.language == "en" ? "English" : L("как в Windows", "as in Windows"))}));
        content.push(node_row({node_heading(L("Множитель добычи", "Loot multiplier")), node_text("×" + fmt_number(config_.multiplier))}));
        content.push(node_row({node_heading(L("Панель на экране", "Panel on the screen")), node_text(on(config_.hud))}));
        content.push(node_row({node_heading(L("Еда из сундуков", "Food from the chests")), node_text(on(config_.supplies))}));
        content.push(node_row({node_heading(L("События в пути", "Events on the way")), node_text(on(config_.events))}));
        content.push(node_row({node_heading(L("Опыт", "Experience")), node_text(on(config_.experience))}));
        content.push(node_row({node_heading(L("Регионы по посещению", "Regions by visiting")), node_text(on(config_.unlock_by_visiting))}));
        content.push(node_row({node_heading(L("Уходят за пределы базы", "Leave the base")), node_text(on(config_.walk_away))}));
        content.push(node_row({node_heading(L("Время идёт без игры", "Time passes while closed")), node_text(on(config_.offline_time))}));
        content.push(node_row({node_button("reload", L("Перечитать файл настроек", "Reload the settings file")), node_button("back", L("« Назад", "« Back"))}));
        return;
    }
    content.push(node_heading(L("Язык", "Language")));
    content.push(node_tabs("language", {L("Как в Windows", "As in Windows"), "Русский", "English"},
                           config_.language == "ru" ? 1 : config_.language == "en" ? 2 : 0));
    content.push(node_heading(L("Добыча", "Loot")));
    content.push(node_row({node_heading(L("Множитель", "Multiplier")), node_text("×" + fmt_number(config_.multiplier)), node_button("mult_down", L("Меньше", "Less")),
                           node_button("mult_up", L("Больше", "More"))}));
    content.push(node_heading(L("Возможности", "Features")));
    content.push(node_toggle("hud", L("Панель с походами на экране", "Panel with the expeditions on the screen"), config_.hud));
    content.push(node_toggle("supplies", L("Походы берут еду из сундуков базы", "Expeditions take food from the base's chests"), config_.supplies));
    content.push(node_toggle("events", L("События в пути: тайники, непогода, ранения", "Events on the way: caches, bad weather, injuries"), config_.events));
    content.push(node_toggle("experience", L("Опыт: выжившие растут в уровне", "Experience: survivors level up"), config_.experience));
    content.push(node_toggle("unlock", L("Регионы открываются, когда вы там побываете", "Regions open once you have been there"), config_.unlock_by_visiting));
    content.push(node_heading(L("Выжившие", "Survivors")));
    content.push(node_toggle("walk_away", L("Уходят за пределы базы на время похода", "Leave the base while away"), config_.walk_away));
    content.push(node_toggle("offline_time", L("Время похода идёт и при выключенной игре", "Time passes while the game is closed"), config_.offline_time));
    std::string durations;
    for (size_t i = 0; i < config_.durations_min.size(); ++i) durations += (i ? ", " : "") + format_minutes(config_.durations_min[i] * 60.0);
    push_caption(content, L("Длительность походов: ", "Durations: ") + durations + L(". Её, маршруты, добычу и опасность регионов можно поменять в файле expeditions_config.json в папке игры", ". These, the routes, loot and dangers can be changed in expeditions_config.json in the game folder"));
    content.push(node_row({node_button("reload", L("Перечитать файл настроек", "Reload the settings file")), node_button("back", L("« Назад", "« Back"))}));
}

json::Value Mod::window(uint64_t client) {
    View& view = views_[client];
    json::Value content = json::Value::array();
    std::string title = L("Экспедиции", "Expeditions");
    std::string id = "main";
    const uint32_t player = player_of(client);
    if (!api_ || api_->version < 7) {
        push_text(content, L("Для экспедиций нужно ядро shroudlink новее (HostApi v7).", "Expeditions need a newer shroudlink core (HostApi v7)."));
    } else {
        if (view.screen == View::Screen::Send) {
            send_window(view, content, title, player);
            id = "send";
        } else if (view.screen == View::Screen::Report) {
            report_window(view, content, title);
            id = "report";
        } else if (view.screen == View::Screen::Recall) {
            recall_window(view, content, title);
            id = "recall";
        } else if (view.screen == View::Screen::Settings) {
            settings_window(view, content, client);
            title = L("Экспедиции: настройки", "Expeditions: settings");
            id = "settings";
        }
        if (view.screen == View::Screen::Main) {
            content = json::Value::array();
            title = L("Экспедиции", "Expeditions");
            id = "main";
            main_window(view, content, player);
        }
    }
    json::Value window = json::Value::object();
    window.set("id", id);
    window.set("title", title);
    window.set("content", content);
    json::Value open = json::Value::object();
    open.set("op", "open");
    open.set("owner", kId);
    open.set("window", window);
    open.set("mod", "ui");
    return open;
}

void Mod::show(void* ctx, uint64_t client) {
    json::Value open = window(client);
    View& view = views_[client];
    const std::string id = open["window"]["id"].as_string();
    // one window at a time
    if (!view.open_window.empty() && view.open_window != id) {
        json::Value close = json::Value::object();
        close.set("op", "close");
        close.set("owner", kId);
        close.set("id", view.open_window);
        close.set("mod", "ui");
        send(ctx, client, close);
    }
    view.open_window = id;
    send(ctx, client, open);
}

void Mod::refuse_settings(uint64_t client, const std::string& what) {
    // once per player: a log line per click would only fill the log
    if (refused_logged_.insert(client).second) {
        char id[24];
        std::snprintf(id, sizeof id, "%016llx", static_cast<unsigned long long>(client));
        log(std::string("settings: client ") + id + " may not change them in the game (" + what +
            "; settings_in_game: " + (config_.settings_in_game == Config::Access::Nobody ? "nobody" : "host") + ")");
    }
}

void Mod::message(const sl::HostApi* api, void* ctx, uint64_t client, const std::string& text) {
    api_ = api;
    bool ok = false;
    const json::Value m = json::parse(text, &ok);
    if (!ok || m["op"].as_string() != "ui_event") return;
    const json::Value& event = m["event"];
    const std::string node = event["node"].as_string();
    const std::string kind = event["event"]["kind"].as_string();
    const int index = event["event"]["index"].is_number() ? static_cast<int>(event["event"]["index"].as_number()) : -1;
    View& view = views_[client];
    if (kind == "closed") {
        view.open_window.clear();
        return;
    }
    view.message.clear();
    if (node == "open" && kind == "menu") {
        view.screen = View::Screen::Main;
    } else if (kind == "click") {
        if (node == "refresh") {
        } else if (node == "back") {
            view.screen = View::Screen::Main;
        } else if (node == "settings") {
            view.screen = View::Screen::Settings;
        } else if (node == "survivors" && index >= 0 && index < static_cast<int>(view.survivors.size())) {
            if (view.workshop != view.survivors[static_cast<size_t>(index)]) view.region = -1;
            view.workshop = view.survivors[static_cast<size_t>(index)];
            view.repeat = false;
            view.screen = View::Screen::Send;
        } else if (node == "history" && index >= 0 && index < static_cast<int>(history_.size())) {
            view.report = static_cast<size_t>(index);
            view.screen = View::Screen::Report;
        } else if (node == "active" && index >= 0 && index < static_cast<int>(view.active.size())) {
            view.workshop = view.active[static_cast<size_t>(index)];
            view.screen = View::Screen::Recall;
        } else if (node == "regions" && index >= 0) {
            const Profession* prof = config_.profession(view.workshop);
            if (prof && index < static_cast<int>(prof->regions.size())) {
                const Region& r = prof->regions[static_cast<size_t>(index)];
                if (region_open(r)) view.region = index;
                else view.message = L("Маршрут закрыт: сначала побывайте в регионе «", "Route closed: first visit ") + lang::region(r.area, r.name) + L("» сами.", " yourself.");
            }
        } else if (node == "durations" && index >= 0) {
            view.duration = index;
        } else if (node == "rations" && index >= 0) {
            view.rations = index;
        } else if (node == "go") {
            view.message = start(ctx, client, view.workshop, view.region, view.duration, view.rations, view.repeat);
            if (find(view.workshop)) view.screen = View::Screen::Main;
        } else if (node == "stop_repeat") {
            if (Expedition* x = find(view.workshop); x && x->repeat) {
                x->repeat = false;
                dirty_ = true;
                view.message = who(x->workshop, x->who) + L(" больше не пойдёт снова.", " will not set out again.");
            }
            view.screen = View::Screen::Main;
        } else if (node == "do_recall") {
            recall(ctx, view.workshop);
            view.screen = View::Screen::Main;
        } else if ((node == "mult_down" || node == "mult_up" || node == "language") && !may_change_settings(client)) {
            refuse_settings(client, node);
            view.message = settings_locked_text();
        } else if (node == "mult_down" || node == "mult_up") {
            // the step nearest to the value (the file may hold any), then one step up or down
            const size_t n = std::size(kMultipliers);
            size_t at = 0;
            for (size_t i = 1; i < n; ++i) {
                if (std::fabs(kMultipliers[i] - config_.multiplier) < std::fabs(kMultipliers[at] - config_.multiplier)) at = i;
            }
            at = node == "mult_up" ? std::min(at + 1, n - 1) : (at > 0 ? at - 1 : 0);
            config_.multiplier = kMultipliers[at];
            save_config();
            log("loot multiplier x" + fmt_number(config_.multiplier));
        } else if (node == "language" && index >= 0 && index <= 2) {
            static const char* const kSettings[] = {"auto", "ru", "en"};
            config_.language = kSettings[index];
            lang::set(config_.language);
            save_config();
            log(std::string("language ") + config_.language);
            for (const auto& [c, v] : views_) send_entries(ctx, c);
            update_hud(ctx, true);
        } else if (node == "reload") {
            load_config();
            view.message = L("Настройки перечитаны.", "Settings reloaded.");
        }
    } else if (kind == "change" && node == "repeat") {
        const json::Value& value = event["event"]["value"];
        view.repeat = value.is_bool() ? value.as_bool() : !view.repeat;
    } else if (kind == "change") {
        bool* setting = node == "offline_time" ? &config_.offline_time
                        : node == "walk_away"  ? &config_.walk_away
                        : node == "hud"        ? &config_.hud
                        : node == "supplies"   ? &config_.supplies
                        : node == "events"     ? &config_.events
                        : node == "experience" ? &config_.experience
                        : node == "unlock"     ? &config_.unlock_by_visiting
                                               : nullptr;
        if (!setting) return;
        if (!may_change_settings(client)) {
            refuse_settings(client, node);
            view.message = settings_locked_text();
            show(ctx, client);
            return;
        }
        const json::Value& value = event["event"]["value"];
        *setting = value.is_bool() ? value.as_bool() : !*setting;
        save_config();
        log("setting " + node + (*setting ? " on" : " off"));
        if (node == "hud") update_hud(ctx, true);
    } else {
        return;
    }
    show(ctx, client);
}

}  // namespace exped
