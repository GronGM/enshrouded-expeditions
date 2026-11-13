#include <algorithm>
// Tests of the expeditions mod against a simulated world behind the shroudlink HostApi.
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../src/defaults.h"
#include "../src/expeditions.h"
#include "../src/json.h"
#include "../src/lang.h"
#include "../src/page_text.h"
#include "../src/platform.h"
#include "../src/regions.h"
#include "../src/supplies.h"

using namespace exped;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failed;                                                             \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                           \
    } while (0)

// ---- a small world ----
struct Fake {
    std::map<uint32_t, std::array<double, 3>> pos;
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> comps;
    std::map<uint32_t, std::vector<sl::Slot>> inv;  // fixed slot count per inventory
    std::vector<uint32_t> players{1};
    std::vector<std::string> sent, logs;
    uint32_t stack = 100;

    void reset() { *this = Fake{}; }
    double dist(uint32_t a, uint32_t b) const {
        const auto& p = pos.at(a);
        const auto& q = pos.at(b);
        return std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));
    }
    void chest(uint32_t e, std::array<double, 3> p, size_t slots) {
        pos[e] = p;
        inv[e] = std::vector<sl::Slot>(slots);
        for (size_t i = 0; i < slots; ++i) inv[e][i].id = e | (static_cast<uint64_t>(i) << 32);
    }
    uint32_t count(uint32_t e, uint32_t item) const {
        uint32_t n = 0;
        if (!inv.count(e)) return 0;
        for (const auto& s : inv.at(e)) {
            if (s.item == item) n += s.count;
        }
        return n;
    }
} W;

static std::vector<uint8_t> u32le(uint32_t v, size_t size, size_t at) {
    std::vector<uint8_t> d(size);
    std::memcpy(d.data() + at, &v, 4);
    return d;
}

extern "C" {
static void f_send(void*, uint64_t, const uint8_t* j, size_t n) { W.sent.emplace_back(reinterpret_cast<const char*>(j), n); }
static void f_log(const uint8_t* t, size_t n) { W.logs.emplace_back(reinterpret_cast<const char*>(t), n); }
static size_t f_slots(void*, uint32_t, sl::Slot*, size_t) { return 0; }
static bool f_is_player(void*, uint32_t) { return false; }
static size_t f_chest_name(void*, uint32_t, uint8_t*, size_t) { return 0; }
static uint32_t f_max_stack(void*, uint32_t) { return 0; }
static uint32_t f_move_stack(void*, uint64_t, uint32_t) { return 0; }
static size_t f_players(uint32_t* out, size_t cap) {
    for (size_t i = 0; i < W.players.size() && i < cap; ++i) out[i] = W.players[i];
    return W.players.size();
}
static bool f_position(uint32_t e, double* out) {
    if (!W.pos.count(e)) return false;
    for (int i = 0; i < 3; ++i) out[i] = W.pos[e][static_cast<size_t>(i)];
    return true;
}
static bool f_teleport(uint32_t, double, double, double) { return false; }
static uint32_t f_player_of(uint64_t) { return 0; }
static size_t f_read(uint32_t e, uint32_t h, uint8_t* out, size_t cap) {
    auto it = W.comps.find({e, h});
    if (it == W.comps.end()) return 0;
    std::memcpy(out, it->second.data(), std::min(cap, it->second.size()));
    return it->second.size();
}
static bool f_write(uint32_t e, uint32_t h, const uint8_t* d, size_t n) {
    auto it = W.comps.find({e, h});
    if (it == W.comps.end() || it->second.size() != n) return false;
    std::memcpy(it->second.data(), d, n);
    return true;
}
static size_t f_inv_near(uint32_t e, double r, uint32_t* out, size_t cap) {
    size_t n = 0;
    for (const auto& [x, slots] : W.inv) {
        if (W.pos.count(x) && W.pos.count(e) && W.dist(e, x) <= r) {
            if (n < cap) out[n] = x;
            ++n;
        }
    }
    return n;
}
static size_t f_inventory(uint32_t e, sl::Slot* out, size_t cap) {
    if (!W.inv.count(e)) return 0;
    const auto& s = W.inv[e];
    for (size_t i = 0; i < s.size() && i < cap; ++i) out[i] = s[i];
    return s.size();
}
static uint32_t f_move_between(uint32_t, uint64_t, uint32_t) { return 0; }
static bool f_buff(uint32_t, const uint8_t*, double) { return false; }
static size_t f_player_name(uint32_t, uint8_t*, size_t) { return 0; }
static size_t f_ent_near(uint32_t e, double r, uint32_t h, uint32_t* out, size_t cap) {
    size_t n = 0;
    for (const auto& [key, data] : W.comps) {
        if (key.second != h || !W.pos.count(key.first) || !W.pos.count(e)) continue;
        if (W.dist(e, key.first) <= r) {
            if (n < cap) out[n] = key.first;
            ++n;
        }
    }
    return n;
}
static uint32_t f_move_items(uint32_t, uint64_t, uint32_t, uint32_t) { return 0; }
static uint32_t f_give(uint32_t e, uint32_t item, uint32_t count) {
    if (!W.inv.count(e)) return 0;
    uint32_t left = count;
    for (auto& s : W.inv[e]) {
        if (s.item == item && s.count > 0 && s.count < W.stack) {
            const uint32_t add = std::min(left, W.stack - s.count);
            s.count += add;
            left -= add;
        }
    }
    for (auto& s : W.inv[e]) {
        if (left == 0) break;
        if (s.count == 0) {
            s.item = item;
            s.count = std::min(left, W.stack);
            left -= s.count;
        }
    }
    return count - left;
}
static uint32_t f_remove(uint32_t e, uint64_t slot, uint32_t count) {
    if (!W.inv.count(e)) return 0;
    for (auto& s : W.inv[e]) {
        if (s.id != slot) continue;
        const uint32_t n = std::min(count, s.count);
        s.count -= n;
        if (!s.count) s.item = 0;
        return n;
    }
    return 0;
}
static size_t f_wname(uint32_t, uint8_t*, size_t) { return 0; }
}

static const sl::HostApi kApi = {8,           f_send,       f_log,     f_slots,         f_is_player, f_chest_name,  f_max_stack, f_move_stack,
                                 f_players,   f_position,   f_teleport, f_player_of,    f_read,      f_write,       f_inv_near,  f_inventory,
                                 f_move_between, f_buff,    f_player_name, f_ent_near,  f_move_items, f_give,       f_remove,    f_wname};

// ---- the scene: a base (altar level 2: 80 m zone) with the blacksmith and two chests ----
constexpr uint32_t PLAYER = 1, ALTAR = 2, SMITH = 10, SMITH_ASSISTANT = 11, CHEST_A = 20, CHEST_B = 21, FAR_CHEST = 22;
constexpr uint32_t BLACKSMITH = 1103055054;
const std::array<double, 3> CENTER = {3744, 843, 1431};  // in the Springlands (a real base)

static void scene() {
    W.reset();
    W.pos[PLAYER] = {CENTER[0] + 5, CENTER[1], CENTER[2]};
    W.pos[ALTAR] = CENTER;
    W.comps[{ALTAR, sl::comp::BaseIdComponent}] = std::vector<uint8_t>(48);
    W.comps[{ALTAR, sl::comp::NetworkLevel}] = {2, 0};
    for (const uint32_t npc : {SMITH, SMITH_ASSISTANT}) {
        W.pos[npc] = {CENTER[0] + 10, CENTER[1], CENTER[2]};
        W.comps[{npc, sl::comp::ActiveNpcState}] = std::vector<uint8_t>(456, 0);
        W.comps[{npc, sl::comp::ActiveNpcState}][0] = sl::npc_state::Roam;
        W.comps[{npc, sl::comp::Crafting}] = u32le(BLACKSMITH, 20, 0x10);
        std::vector<uint8_t> saved(48);
        const float home[5] = {static_cast<float>(CENTER[0] + 10), static_cast<float>(CENTER[1]), static_cast<float>(CENTER[2]), 0, 30};
        std::memcpy(saved.data(), home, 12);
        saved[0xc] = 0;
        std::memcpy(saved.data() + 0x10, &home[4], 4);
        W.comps[{npc, sl::comp::ActiveNpcStateSaved}] = saved;
    }
    W.comps[{SMITH_ASSISTANT, sl::comp::InventorySetup}] = std::vector<uint8_t>(84);
    W.chest(CHEST_A, {CENTER[0] + 5, CENTER[1], CENTER[2] + 5}, 4);
    W.chest(CHEST_B, {CENTER[0] - 5, CENTER[1], CENTER[2] - 5}, 40);
    W.chest(FAR_CHEST, {CENTER[0] + 200, CENTER[1], CENTER[2]}, 40);  // another base
}

static std::string ui(const std::string& window, const std::string& node, const std::string& kind, int index = -1) {
    json::Value m = json::Value::object();
    m.set("mod", kId);
    m.set("op", "ui_event");
    json::Value e = json::Value::object();
    e.set("window", window);
    e.set("node", node);
    json::Value ev = json::Value::object();
    ev.set("kind", kind);
    if (index >= 0) ev.set("index", index);
    e.set("event", ev);
    m.set("event", e);
    return m.dump();
}

static std::string ui_change(const std::string& window, const std::string& node, bool value) {
    json::Value m = json::parse(ui(window, node, "change"));
    json::Value e = json::Value::object();
    e.set("window", window);
    e.set("node", node);
    json::Value ev = json::Value::object();
    ev.set("kind", "change");
    ev.set("value", value);
    e.set("event", ev);
    m.set("event", e);
    return m.dump();
}

static json::Value last_open() {
    for (auto it = W.sent.rbegin(); it != W.sent.rend(); ++it) {
        const json::Value v = json::parse(*it);
        if (v["op"].as_string() == "open") return v;
    }
    return json::Value();
}

static bool sent_contains(const std::string& text) {
    for (const auto& s : W.sent) {
        if (s.find(text) != std::string::npos) return true;
    }
    return false;
}

static float home_x(uint32_t npc) {
    float x;
    std::memcpy(&x, W.comps[{npc, sl::comp::ActiveNpcStateSaved}].data(), 4);
    return x;
}

static double g_now = 0;
static double fake_clock() { return g_now; }

// one game second per tick
static void run_ticks(Mod& m, int n) {
    for (int i = 0; i < n; ++i) {
        g_now += 1;
        m.tick(&kApi, reinterpret_cast<void*>(1));
    }
}

static const std::string kDir = "/tmp/expeditions-test/";

static void clean_dir() {
    std::remove((kDir + "expeditions_config.json").c_str());
    std::remove((kDir + "expeditions_state.json").c_str());
}

// The default settings with the newer features switched off (the older tests count loot exactly).
static void features(bool supplies, bool events, bool experience, bool unlock) {
    json::Value v = json::parse(kDefaultConfig);
    json::Value sup = json::Value::object();
    sup.set("enabled", supplies);
    sup.set("rations_per_hour", 2);
    v.set("supplies", sup);
    v.set("events", events);
    json::Value xp = json::Value::object();
    xp.set("enabled", experience);
    xp.set("minutes_per_level", 30);
    v.set("experience", xp);
    v.set("unlock_regions_by_visiting", unlock);
    platform::write_file_atomic(kDir + "expeditions_config.json", v.dump(true));
}

static void put(uint32_t chest, size_t slot, const char* item, uint32_t count) {
    W.inv[chest][slot].item = item_id(item);
    W.inv[chest][slot].count = count;
}

static void test_json() {
    std::printf("json\n");
    bool ok = false;
    const json::Value v = json::parse(R"({"a": [1, 2.5, "xé\"\\"], "b": {"c": true, "d": null}, "e": "Кузнец"} )", &ok);
    CHECK(ok);
    CHECK(v["a"][1].as_number() == 2.5);
    CHECK(v["a"][2].as_string() == "x\xc3\xa9\"\\");
    CHECK(v["b"]["c"].as_bool());
    CHECK(v["b"]["d"].is_null());
    CHECK(v["e"].as_string() == "Кузнец");
    const json::Value back = json::parse(v.dump(), &ok);
    CHECK(ok && back.dump() == v.dump());
    json::parse("{\"a\": [1,}", &ok);
    CHECK(!ok);
    json::parse("// comment\n{\"a\": 1,}", &ok);
    CHECK(ok);
}

static void test_config() {
    std::printf("config\n");
    const Config c = parse_config(json::parse(kDefaultConfig));
    CHECK(c.problems.empty());
    for (const auto& p : c.problems) std::printf("    problem: %s\n", p.c_str());
    CHECK(c.professions.size() == 6);
    for (const auto& p : c.professions) {
        CHECK(p.regions.size() == 7);
        for (const auto& r : p.regions) CHECK(!r.loot.empty());
    }
    CHECK(c.durations_min.size() == 4);
    CHECK(item_id("Material_T2_Ore_Copper") != 0);
    CHECK(item_id("No_Such_Item") == 0);
    CHECK(item_id("12345") == 12345);
    const Config bad = parse_config(json::parse(R"({"professions": [{"name": "X", "workshop": 5, "regions": [{"name": "R", "loot": [{"item": "Nope", "per_hour": [1, 2]}]}]}]})"));
    CHECK(bad.problems.size() == 1);
    CHECK(items_word(1) == "предмет" && items_word(3) == "предмета" && items_word(11) == "предметов" && items_word(22) == "предмета");
    CHECK(format_minutes(1800) == "30 мин" && format_minutes(3600) == "1 ч" && format_minutes(5400) == "1 ч 30 мин");
}

static void test_send_and_return() {
    std::printf("send and return\n");
    clean_dir();
    features(false, false, false, false);
    scene();
    Mod m;
    m.set_seed(7);
    m.set_clock(fake_clock);
    m.load(&kApi, kDir);
    CHECK(platform::file_exists(kDir + "expeditions_config.json"));
    m.join(&kApi, reinterpret_cast<void*>(1), 5);
    CHECK(sent_contains("\"op\":\"entries\""));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("", "open", "menu"));
    json::Value open = last_open();
    CHECK(open["window"]["id"].as_string() == "main");
    CHECK(open.dump().find("Кузнец") != std::string::npos);

    // pick the blacksmith, Revelwood, 30 minutes
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "survivors", "click", 0));
    open = last_open();
    CHECK(open["window"]["id"].as_string() == "send");
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "regions", "click", 1));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "durations", "click", 1));
    open = last_open();
    CHECK(open.dump().find("item_grid") != std::string::npos);
    CHECK(open.dump().find("»  Ревелвуд") != std::string::npos);
    CHECK(open.dump().find("\"tabs\"") != std::string::npos);
    W.sent.clear();
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "go", "click"));
    CHECK(m.expeditions().size() == 1);
    const Expedition& x = m.expeditions()[0];
    CHECK(x.workshop == BLACKSMITH);
    CHECK(x.duration_s == 1800);
    CHECK(x.walked);
    CHECK(x.has_base && x.base_half == 40);
    // the survivor (not the assistant) walks off, out past the zone's edge (40 m + 12 m)
    CHECK(std::fabs(home_x(SMITH) - static_cast<float>(CENTER[0] + 52)) < 0.01f);
    CHECK(std::fabs(home_x(SMITH_ASSISTANT) - static_cast<float>(CENTER[0] + 10)) < 0.01f);
    { const uint8_t state = W.comps[{SMITH, sl::comp::ActiveNpcState}][0]; CHECK(state == sl::npc_state::DecideTask); }
    CHECK(sent_contains("notify"));
    CHECK(platform::file_exists(kDir + "expeditions_state.json"));
    // the blacksmith is not offered again
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "refresh", "click"));
    open = last_open();
    CHECK(open.dump().find("\"survivors\"") == std::string::npos);
    CHECK(open.dump().find("В походе") != std::string::npos);

    // not yet back
    m.advance(1000);
    run_ticks(m, 31);  // saved every 30 s
    CHECK(m.expeditions().size() == 1);

    // a restart in the middle: the expedition goes on
    {
        Mod again;
        again.load(&kApi, kDir);
        CHECK(again.expeditions().size() == 1);
        // saved at most 30 s ago
        CHECK(again.expeditions()[0].elapsed_s >= 1000 && again.expeditions()[0].elapsed_s <= m.expeditions()[0].elapsed_s + 1 &&
              m.expeditions()[0].elapsed_s - again.expeditions()[0].elapsed_s <= 31);
    }

    // back while the player is away from the base: waits
    W.pos[PLAYER] = {CENTER[0] + 300, CENTER[1], CENTER[2]};
    m.advance(1000);
    for (int i = 0; i < 6; ++i) {
        m.advance(1);
        run_ticks(m, 1);
    }
    CHECK(m.expeditions().size() == 1);
    CHECK(m.expeditions()[0].time_up());
    // the player comes home: home point back, loot in the base's chests (not the far one)
    W.pos[PLAYER] = {CENTER[0] + 5, CENTER[1], CENTER[2]};
    W.sent.clear();
    int guard = 0;
    while (!m.expeditions().empty() && guard++ < 60) run_ticks(m, 1);
    CHECK(m.expeditions().empty());
    CHECK(std::fabs(home_x(SMITH) - static_cast<float>(CENTER[0] + 10)) < 0.01f);
    const uint32_t copper = item_id("Material_T2_Ore_Copper");
    const uint32_t got = W.count(CHEST_A, copper) + W.count(CHEST_B, copper);
    CHECK(got >= 15 && got <= 25);  // 30 min of 30-50 per hour
    CHECK(W.count(FAR_CHEST, copper) == 0);
    CHECK(m.history().size() == 1 && m.history()[0].items > 0);
    CHECK(sent_contains("вернулся из похода"));
}

static void test_full_chests_and_recall() {
    std::printf("full chests, recall\n");
    clean_dir();
    features(false, false, false, false);
    scene();
    W.inv.erase(CHEST_B);
    W.inv[CHEST_A] = std::vector<sl::Slot>(1);
    W.inv[CHEST_A][0] = {CHEST_A, item_id("Material_T1_Wood_Logs"), 100};  // full
    Mod m;
    m.set_seed(3);
    m.set_clock(fake_clock);
    m.load(&kApi, kDir);
    m.join(&kApi, reinterpret_cast<void*>(1), 5);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("", "open", "menu"));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "survivors", "click", 0));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "durations", "click", 3));  // 2 h
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "go", "click"));
    CHECK(m.expeditions().size() == 1);
    // recalled after half an hour: loot for half an hour only
    m.advance(1800);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "active", "click", 0));
    CHECK(last_open()["window"]["id"].as_string() == "recall");
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("recall", "do_recall", "click"));
    CHECK(m.expeditions()[0].recalled);
    int guard = 0;
    while (!m.expeditions()[0].told_full && guard++ < 2000) run_ticks(m, 1);
    CHECK(m.expeditions()[0].told_full);
    uint32_t pending = 0;
    for (const auto& p : m.expeditions()[0].pending) pending += p.second;
    CHECK(pending >= 20 && pending <= 45);  // flint 20-30, scraps 5-10, salt 2-5 for half an hour
    // room again
    W.chest(CHEST_B, {CENTER[0] - 5, CENTER[1], CENTER[2] - 5}, 40);
    guard = 0;
    while (!m.expeditions().empty() && guard++ < 2000) run_ticks(m, 1);
    CHECK(m.expeditions().empty());
    CHECK(W.count(CHEST_B, item_id("Material_T1_Stone_Flint")) >= 10);
}

static void test_settings_and_broken_file() {
    std::printf("settings, broken file\n");
    clean_dir();
    features(false, false, false, false);
    scene();
    Mod m;
    m.set_clock(fake_clock);
    m.load(&kApi, kDir);
    m.join(&kApi, reinterpret_cast<void*>(1), 5);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "settings", "click"));
    CHECK(last_open()["window"]["id"].as_string() == "settings");
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("settings", "mult_up", "click"));
    CHECK(m.config().multiplier == 1.5);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("settings", "mult_up", "click"));
    CHECK(m.config().multiplier == 2);
    CHECK(last_open().dump().find("\"toggle\"") != std::string::npos);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui_change("settings", "walk_away", false));
    CHECK(!m.config().walk_away);
    std::string text;
    platform::read_file(kDir + "expeditions_config.json", text);
    bool ok = false;
    const json::Value saved = json::parse(text, &ok);
    CHECK(ok && saved["loot_multiplier"].as_number() == 2 && !saved["walk_away"].as_bool(true));
    CHECK(saved["professions"].size() == 6);  // the rest of the file is kept
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui_change("settings", "events", true));
    CHECK(m.config().events);
    platform::read_file(kDir + "expeditions_config.json", text);
    CHECK(json::parse(text)["events"].as_bool(false) && !json::parse(text)["supplies"]["enabled"].as_bool(true));

    // without walking away nobody moves
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("settings", "back", "click"));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "survivors", "click", 0));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "go", "click"));
    CHECK(m.expeditions().size() == 1 && !m.expeditions()[0].walked);
    CHECK(std::fabs(home_x(SMITH) - static_cast<float>(CENTER[0] + 10)) < 0.01f);

    // a broken file: defaults run, the file is left alone
    platform::write_file_atomic(kDir + "expeditions_config.json", "{ broken");
    Mod b;
    b.load(&kApi, kDir);
    CHECK(!b.config().professions.empty());
    CHECK(!b.config().problems.empty());
    platform::read_file(kDir + "expeditions_config.json", text);
    CHECK(text == "{ broken");
}

static void test_page_text() {
    std::printf("page text\n");
    using namespace exped::page;
    // widths measured on the game's font (UI units): "Настройки" 146, "Обновить" 129
    CHECK(std::fabs(text_units("Настройки") - 146) < 12);
    CHECK(std::fabs(text_units("Обновить") - 129) < 12);
    // a padded cell's column (13.7 a character, as the page counts) holds the real text
    for (const char* cell : {"Охотник", "Алхимик", "Высокогорье кочевников", "Bob", "Колыбель Пепла (Cinder's Cradle)"}) {
        const std::string padded = pad_cell(cell);
        CHECK(padded.compare(0, std::strlen(cell), cell) == 0);
        CHECK(code_points(padded).size() * 13.7 >= text_units(cell) + 4);
    }
    CHECK(pad_cell("abc").size() <= 4);  // Latin needs next to nothing
    // labels: spaces on both sides, the same on each
    const std::string label = pad_label("Настройки");
    CHECK(label == "  Настройки  ");
    CHECK(pad_label("OK") == "OK");
    const std::string row = cells({"Охотник", "Весенние земли", "вернётся через 15 мин"});
    CHECK(std::count(row.begin(), row.end(), '\t') == 3 && row.back() == '\t');
    CHECK(row.find("вернётся через 15 мин") != std::string::npos);
    // the history row of the screenshot: the column 30-60 units wider than the text (it was 77)
    const double column = code_points(pad_cell("Высокогорье кочевников")).size() * 13.7 + 36;
    CHECK(column - 344 >= 36 && column - 344 <= 60);
    // long text: lines that fit, nothing lost
    const std::string text =
        "Рядом нет выживших, которых можно отправить. Встаньте на базе рядом с кузнецом, плотником, охотником, алхимиком, фермером или рыбаком.";
    const auto lines = wrap_lines(text);
    CHECK(lines.size() == 2);
    std::string joined;
    for (const auto& l : lines) {
        CHECK(text_units(l) <= 1200);
        joined += (joined.empty() ? "" : " ") + l;
    }
    CHECK(joined == text);
    CHECK(wrap_lines("Куда?").size() == 1);
    CHECK(short_region("Высокогорье кочевников (Nomad Highlands)") == "Высокогорье кочевников");
    CHECK(short_region("Весенние земли") == "Весенние земли");
    CHECK(short_region("Пепел (старый)") == "Пепел (старый)");
    CHECK(lang::routes_word(1) == "маршрут" && lang::routes_word(3) == "маршрута" && lang::routes_word(7) == "маршрутов" && lang::routes_word(12) == "маршрутов");
}

static std::array<double, 3> somewhere_in(int region) {
    for (double z = 4; z < 10240; z += 64) {
        for (double x = 4; x < 10240; x += 64) {
            if (regions::at(x, z) == region) return {x, 800, z};
        }
    }
    return {-1, -1, -1};
}

static bool hud_rows_with(const std::string& text, bool* empty_after = nullptr) {
    bool found = false;
    for (const auto& s : W.sent) {
        const json::Value v = json::parse(s);
        if (v["op"].as_string() != "hud") continue;
        if (empty_after) *empty_after = v["rows"].size() == 0;
        if (s.find(text) != std::string::npos) found = true;
    }
    return found;
}

static void test_regions_and_food() {
    std::printf("regions, food\n");
    CHECK(regions::at(3744, 1431) == regions::kSpringlands);  // the player's base
    CHECK(regions::at(-5, 100) == -1 && regions::at(100, 20000) == -1);
    CHECK(regions::by_english("nomad highlands") == 3);
    CHECK(somewhere_in(1)[0] >= 0);
    CHECK(supplies::kind_of(item_id("Food_T1_raw_fruit_Red_Berries")) == 0);
    CHECK(supplies::kind_of(item_id("Food_T1_processed_meat_Grilled_LeanMeat")) == 1);
    CHECK(supplies::kind_of(item_id("Food_T3_dish_meat_ChickenSoup")) == 2);
    CHECK(supplies::kind_of(item_id("Food_T1_Bunny_Bait")) == -1);
    CHECK(supplies::kind_of(item_id("Food_T1_raw_herb_StJohnsWort")) == -1);
    CHECK(supplies::kind_of(item_id("Food_T1_raw_bad_meat_LeanMeat")) == -1);
    CHECK(supplies::kind_of(item_id("Material_T1_Wood_Logs")) == -1);
    CHECK(supplies::tier_of(item_id("Food_T2_processed_meat_GrilledGame")) == 2);
    // config: the area from the English name in brackets, danger by region
    const Config c = parse_config(json::parse(kDefaultConfig));
    CHECK(c.professions[0].regions[1].area == 1 && std::fabs(c.professions[0].regions[1].danger - regions::default_danger(1)) < 1e-9);
    CHECK(c.supplies && c.events && c.experience && c.unlock_by_visiting && c.hud);
}

static void test_features() {
    std::printf("levels, rations, regions, repeat, hud, report\n");
    clean_dir();
    features(true, true, true, true);
    scene();
    put(CHEST_B, 0, "Food_T1_raw_fruit_Red_Berries", 5);
    put(CHEST_B, 1, "Food_T2_processed_meat_GrilledGame", 3);
    put(CHEST_B, 2, "Food_T1_processed_meat_Grilled_LeanMeat", 1);
    Mod m;
    m.set_seed(11);
    m.set_clock(fake_clock);
    m.load(&kApi, kDir);
    m.join(&kApi, reinterpret_cast<void*>(1), 5);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("", "open", "menu"));
    std::string open = last_open().dump();
    CHECK(open.find("ур. 1") != std::string::npos);
    CHECK(open.find("Нажмите") == std::string::npos || open.find("\"type\":\"row\"") != std::string::npos);  // hints are quiet rows

    // the send window: Revelwood is closed, rations by kind with what the chests hold
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "survivors", "click", 0));
    open = last_open().dump();
    CHECK(open.find("закрыт ") != std::string::npos && open.find("закрыт\" ") == std::string::npos);
    CHECK(open.find("Закрытый маршрут откроется") != std::string::npos);
    CHECK(open.find("опасность низкая") != std::string::npos);
    CHECK(open.find("»  Весенние земли") != std::string::npos);
    CHECK(open.find("Простая еда (5)") != std::string::npos && open.find("Жареное (4)") != std::string::npos);
    CHECK(open.find("\"Уровень\"") != std::string::npos && open.find("\"Риск ранения\"") != std::string::npos);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "regions", "click", 1));
    CHECK(last_open().dump().find("Маршрут закрыт") != std::string::npos);
    // an hour: 2 rations; no dishes in the chests
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "durations", "click", 2));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "rations", "click", 2));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "go", "click"));
    CHECK(m.expeditions().empty());
    CHECK(last_open().dump().find("Не хватает припасов") != std::string::npos);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "rations", "click", 1));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui_change("send", "repeat", true));
    W.sent.clear();
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "go", "click"));
    CHECK(m.expeditions().size() == 1 && m.expeditions()[0].repeat && m.expeditions()[0].rations == 2);
    // the plainest first: the T1 meat, then one T2
    CHECK(W.count(CHEST_B, item_id("Food_T1_processed_meat_Grilled_LeanMeat")) == 0);
    CHECK(W.count(CHEST_B, item_id("Food_T2_processed_meat_GrilledGame")) == 2);
    CHECK(W.count(CHEST_B, item_id("Food_T1_raw_fruit_Red_Berries")) == 5);
    CHECK(hud_rows_with("Кузнец"));

    // a trip to Revelwood opens its routes
    const auto pos = W.pos[PLAYER];
    W.pos[PLAYER] = somewhere_in(1);
    W.sent.clear();
    run_ticks(m, 6);
    CHECK(m.visited().count(1) == 1);
    CHECK(sent_contains("открыты маршруты в регион «Ревелвуд»"));
    W.pos[PLAYER] = pos;

    // back after an hour: level 2 (30 min a level), loot in, off again with 2 more rations
    m.advance(3600);
    W.sent.clear();
    int guard = 0;
    while (m.history().empty() && guard++ < 100) run_ticks(m, 1);
    CHECK(m.history().size() == 1);
    CHECK(m.level_of(BLACKSMITH) == 2 && m.history()[0].level_up && m.history()[0].level == 2);
    CHECK(!m.history()[0].loot.empty() && m.history()[0].items > 0);
    CHECK(sent_contains("2 уровня"));
    CHECK(sent_contains("снова уходит в поход"));
    CHECK(m.expeditions().size() == 1 && m.expeditions()[0].repeat && m.expeditions()[0].level == 2);
    CHECK(W.count(CHEST_B, item_id("Food_T2_processed_meat_GrilledGame")) == 0);
    {
        const uint8_t state = W.comps[{SMITH, sl::comp::ActiveNpcState}][0];
        CHECK(state == sl::npc_state::DecideTask);
        CHECK(std::fabs(home_x(SMITH) - static_cast<float>(CENTER[0] + 10)) > 1);  // walking off again
    }

    // the report of the first trip
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "history", "click", 0));
    open = last_open().dump();
    CHECK(last_open()["window"]["id"].as_string() == "report" && open.find("report_items") != std::string::npos);
    CHECK(open.find("2 (новый!)") != std::string::npos);

    // stop repeating; it comes back once more and stays; the panel goes away
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("report", "back", "click"));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "active", "click", 0));
    CHECK(last_open().dump().find("stop_repeat") != std::string::npos);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("recall", "stop_repeat", "click"));
    CHECK(!m.expeditions()[0].repeat);
    m.advance(3600);
    W.sent.clear();
    guard = 0;
    while (!m.expeditions().empty() && guard++ < 100) run_ticks(m, 1);
    CHECK(m.expeditions().empty() && m.history().size() == 2);
    bool empty = false;
    hud_rows_with("", &empty);
    CHECK(empty);
    CHECK(std::fabs(home_x(SMITH) - static_cast<float>(CENTER[0] + 10)) < 0.01f);

    // all of it survives a restart
    {
        Mod again;
        again.load(&kApi, kDir);
        CHECK(again.level_of(BLACKSMITH) == m.level_of(BLACKSMITH));
        CHECK(again.visited().count(1) == 1 && again.history().size() == 2 && !again.history()[0].loot.empty());
        CHECK(again.survivors().at(BLACKSMITH).trips == 2);
    }

    // risk: 12% an hour -> 22.6% for two hours; lower with levels
    Region r;
    r.danger = 0.12;
    CHECK(std::fabs(m.hurt_chance(r, 2, 1) - (1 - 0.88 * 0.88)) < 1e-9);
    CHECK(std::fabs(m.hurt_chance(r, 2, 3) - (1 - 0.88 * 0.88) * (1 - 0.14)) < 1e-9);
}

static void test_dangerous_region() {
    std::printf("events\n");
    clean_dir();
    // the Springlands made deadly: nearly every trip ends hurt, with half the loot
    json::Value v = json::parse(kDefaultConfig);
    v.set("supplies", json::parse(R"({"enabled": false})"));
    json::Value profs = v["professions"];
    json::Value smith = profs[0];
    json::Value regs = smith["regions"];
    json::Value spring = regs[0];
    spring.set("danger", 0.95);
    json::Value newregs = json::Value::array();
    newregs.push(spring);
    smith.set("regions", newregs);
    json::Value newprofs = json::Value::array();
    newprofs.push(smith);
    v.set("professions", newprofs);
    platform::write_file_atomic(kDir + "expeditions_config.json", v.dump(true));
    int hurt = 0;
    for (uint32_t seed = 1; seed <= 10; ++seed) {
        std::remove((kDir + "expeditions_state.json").c_str());
        scene();
        Mod m;
        m.set_seed(seed);
        m.set_clock(fake_clock);
        m.load(&kApi, kDir);
        m.join(&kApi, reinterpret_cast<void*>(1), 5);
        m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("", "open", "menu"));
        m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "survivors", "click", 0));
        m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "durations", "click", 2));
        m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "go", "click"));
        m.advance(3600);
        int guard = 0;
        while (m.history().empty() && guard++ < 100) run_ticks(m, 1);
        if (!m.history().empty() && m.history()[0].event == "hurt") ++hurt;
        if (std::getenv("EXPED_DEBUG")) std::printf("    seed %u: %zu %zu %s %s\n", seed, m.expeditions().size(), m.history().size(),
                                                     m.history().empty() ? "-" : m.history()[0].event.c_str(), last_open().dump().substr(0, 300).c_str());
    }
    CHECK(hurt >= 8);
}

// Cyrillic letters are 0xD0/0xD1 lead bytes in UTF-8 («» are not)
static bool cyrillic(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) == 0xD0 || static_cast<unsigned char>(c) == 0xD1; });
}

static void test_english() {
    std::printf("english\n");
    clean_dir();
    features(true, true, true, true);
    scene();
    put(CHEST_B, 0, "Food_T1_raw_fruit_Red_Berries", 5);
    Mod m;
    m.set_seed(5);
    m.set_clock(fake_clock);
    m.load(&kApi, kDir);
    W.sent.clear();
    m.join(&kApi, reinterpret_cast<void*>(1), 5);
    CHECK(sent_contains("Экспедиции"));
    // the language from the settings window: the menu entry follows at once
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "settings", "click"));
    CHECK(last_open().dump().find("\"language\"") != std::string::npos);
    W.sent.clear();
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("settings", "language", "click", 2));
    CHECK(m.config().language == "en" && lang::current() == lang::Lang::En);
    CHECK(sent_contains("\"label\":\"Expeditions\""));
    std::string open = last_open().dump();
    CHECK(open.find("Expeditions: settings") != std::string::npos && open.find("Repeat") == std::string::npos);
    CHECK(open.find("Regions open once you have been there") != std::string::npos);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("settings", "back", "click"));
    open = last_open().dump();
    CHECK(open.find("Send out") != std::string::npos && open.find("Blacksmith") != std::string::npos && open.find("lv. 1") != std::string::npos);
    CHECK(open.find("ore, stone and gems") != std::string::npos);
    if (cyrillic(open)) std::printf("    main: %s\n", open.c_str());
    CHECK(cyrillic(open) == false);
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "survivors", "click", 0));
    open = last_open().dump();
    CHECK(open.find("Blacksmith: expedition") != std::string::npos && open.find("»  Springlands") != std::string::npos);
    CHECK(open.find("Plain food (5)") != std::string::npos && open.find("Risk of injury") != std::string::npos);
    CHECK(cyrillic(open) == false);
    W.sent.clear();
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("send", "go", "click"));
    CHECK(m.expeditions().size() == 1);
    CHECK(sent_contains("Blacksmith sets out: Springlands, 30 min."));
    run_ticks(m, 6);
    CHECK(hud_rows_with("\"title\":\"Expeditions\""));
    m.advance(3600);
    W.sent.clear();
    int guard = 0;
    while (m.history().empty() && guard++ < 100) run_ticks(m, 1);
    CHECK(sent_contains("Blacksmith is back (Springlands)"));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "history", "click", 0));
    open = last_open().dump();
    if (cyrillic(open)) std::printf("    report: %s\n", open.c_str());
    CHECK(open.find("Blacksmith: report") != std::string::npos && cyrillic(open) == false);
    // and back to Russian; the saved file keeps the choice
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("report", "back", "click"));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("main", "settings", "click"));
    m.message(&kApi, reinterpret_cast<void*>(1), 5, ui("settings", "language", "click", 1));
    CHECK(lang::current() == lang::Lang::Ru);
    std::string text;
    platform::read_file(kDir + "expeditions_config.json", text);
    CHECK(json::parse(text)["language"].as_string() == "ru");
    // an old saved event text reads as the event
    CHECK(lang::event_id("нашёл богатое место, добычи на четверть больше") == "rich");
    CHECK(lang::trade(2216351573, "Охотник") == "Охотник" && lang::trade(2216351573, "Hunter") == "Охотник" && lang::trade(2216351573, "Bob") == "Bob");
    CHECK(lang::region(3, "Высокогорье кочевников (Nomad Highlands)") == "Высокогорье кочевников");
    lang::set("en");
    CHECK(lang::region(3, "Высокогорье кочевников (Nomad Highlands)") == "Nomad Highlands" && lang::minutes(5400) == "1 h 30 min");
    lang::set("ru");
}


// Who may change the settings in the game: the host (whose addon shares the process and keeps
// its session in SHROUDLINK_CLIENT_SESSION), everyone, or nobody.
static void test_settings_access() {
    std::printf("settings access\n");
    clean_dir();
    features(false, false, false, false);
    scene();
    void* ctx = reinterpret_cast<void*>(1);
    {
        Mod m;
        m.set_clock(fake_clock);
        m.load(&kApi, kDir);
        CHECK(m.config().settings_in_game == Config::Access::Host);
        CHECK(m.host_client() == 5 && m.may_change_settings(5) && !m.may_change_settings(7));
        // another player: the window shows the values without the controls, clicks change nothing
        m.join(&kApi, ctx, 7);
        m.message(&kApi, ctx, 7, ui("main", "settings", "click"));
        std::string open = last_open().dump();
        CHECK(open.find("mult_up") == std::string::npos && open.find("\"toggle\"") == std::string::npos);
        CHECK(open.find("только тот, кто запустил мир") != std::string::npos && open.find("×1") != std::string::npos);
        CHECK(open.find("\"reload\"") != std::string::npos);
        m.message(&kApi, ctx, 7, ui("settings", "mult_up", "click"));
        m.message(&kApi, ctx, 7, ui("settings", "mult_up", "click"));
        CHECK(m.config().multiplier == 1);
        m.message(&kApi, ctx, 7, ui_change("settings", "walk_away", false));
        CHECK(m.config().walk_away);
        m.message(&kApi, ctx, 7, ui("settings", "language", "click", 2));
        CHECK(m.config().language == "auto");
        std::string text;
        platform::read_file(kDir + "expeditions_config.json", text);
        CHECK(json::parse(text)["loot_multiplier"].as_number() == 1);
        // the host can
        m.join(&kApi, ctx, 5);
        m.message(&kApi, ctx, 5, ui("main", "settings", "click"));
        CHECK(last_open().dump().find("mult_up") != std::string::npos);
        m.message(&kApi, ctx, 5, ui("settings", "mult_up", "click"));
        CHECK(m.config().multiplier == 1.5);
        platform::read_file(kDir + "expeditions_config.json", text);
        CHECK(json::parse(text)["settings_in_game"].as_string() == "host");
    }
    {
        // a dedicated server: no addon in the process, so nobody changes them in the game
        unsetenv("SHROUDLINK_CLIENT_SESSION");
        Mod m;
        m.set_clock(fake_clock);
        m.load(&kApi, kDir);
        CHECK(m.host_client() == 0 && !m.may_change_settings(5));
        m.join(&kApi, ctx, 5);
        m.message(&kApi, ctx, 5, ui("main", "settings", "click"));
        CHECK(last_open().dump().find("администратор сервера") != std::string::npos);
        m.message(&kApi, ctx, 5, ui("settings", "mult_up", "click"));
        CHECK(m.config().multiplier == 1.5);
        // the admin edits the file and reloads it (anyone may press reload: it applies the file)
        std::string text;
        platform::read_file(kDir + "expeditions_config.json", text);
        json::Value v = json::parse(text);
        v.set("loot_multiplier", 3.0);
        platform::write_file_atomic(kDir + "expeditions_config.json", v.dump(true));
        m.message(&kApi, ctx, 5, ui("settings", "reload", "click"));
        CHECK(m.config().multiplier == 3);
        // everyone
        v.set("settings_in_game", "everyone");
        platform::write_file_atomic(kDir + "expeditions_config.json", v.dump(true));
        m.message(&kApi, ctx, 5, ui("settings", "reload", "click"));
        CHECK(m.may_change_settings(5) && m.may_change_settings(7));
        m.message(&kApi, ctx, 5, ui("settings", "mult_down", "click"));
        CHECK(m.config().multiplier == 2);
        platform::read_file(kDir + "expeditions_config.json", text);
        CHECK(json::parse(text)["settings_in_game"].as_string() == "everyone");
        setenv("SHROUDLINK_CLIENT_SESSION", "0:5", 1);
    }
    {
        // nobody, and a word the mod does not know (then host, and it says so)
        std::string text;
        platform::read_file(kDir + "expeditions_config.json", text);
        json::Value v = json::parse(text);
        v.set("settings_in_game", "nobody");
        platform::write_file_atomic(kDir + "expeditions_config.json", v.dump(true));
        Mod m;
        m.set_clock(fake_clock);
        m.load(&kApi, kDir);
        CHECK(!m.may_change_settings(5));
        m.join(&kApi, ctx, 5);
        m.message(&kApi, ctx, 5, ui("main", "settings", "click"));
        m.message(&kApi, ctx, 5, ui_change("settings", "events", true));
        CHECK(!m.config().events);
        CHECK(last_open().dump().find("только в файле expeditions_config.json") != std::string::npos);
        v.set("settings_in_game", "admins");
        platform::write_file_atomic(kDir + "expeditions_config.json", v.dump(true));
        Mod b;
        b.load(&kApi, kDir);
        CHECK(b.config().settings_in_game == Config::Access::Host && !b.config().problems.empty());
    }
    // a session text that is not "slot:hex"
    setenv("SHROUDLINK_CLIENT_SESSION", "garbage", 1);
    {
        Mod m;
        m.load(&kApi, kDir);
        CHECK(m.host_client() == 0);
    }
    setenv("SHROUDLINK_CLIENT_SESSION", "0:5", 1);
}

int main() {
    setenv("EXPED_LANG", "ru", 1);
    // singleplayer: the addon in this process is client 5, the one the tests play
    setenv("SHROUDLINK_CLIENT_SESSION", "0:5", 1);
#ifndef _WIN32
    if (std::system(("mkdir -p " + kDir).c_str()) != 0) return 2;
#endif
    test_json();
    test_page_text();
    test_regions_and_food();
    test_config();
    test_send_and_return();
    test_full_chests_and_recall();
    test_settings_and_broken_file();
    test_settings_access();
    test_features();
    test_dangerous_region();
    test_english();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
