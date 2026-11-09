// The EMF build against a fake EMF core: a small world (a player, a flame altar, a blacksmith,
// a chest with food), EMF's item requests applied one step later as the core does, and its
// window, HUD and notes captured.
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "../src/defaults.h"
#include "../src/emf_host.h"
#include "../src/emf_ui.h"
#include "../src/expeditions.h"
#include "../src/item_names.h"
#include "../src/lang.h"
#include "../src/page_text.h"
#include "../src/sdk.h"

static int g_checks = 0, g_failed = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(c)) {                                                      \
            ++g_failed;                                                  \
            std::fprintf(stderr, "%s:%d: FAILED %s\n", __FILE__, __LINE__, #c); \
        }                                                                \
    } while (0)

namespace fake {

EmfWorld* const kWorld = reinterpret_cast<EmfWorld*>(0x1000);
std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> comps;  // (entity, hash) -> data
std::vector<uint32_t> entities;
EmfWorldCallback tick_cb = nullptr;
EmfCommandCallback command_cb = nullptr;
struct Put {
    uint32_t entity, item, count;
    EmfPutItemsCallback cb;
    void* user;
};
std::vector<Put> puts;
std::vector<EmfItemsRequest> takes;
uint32_t put_shortfall = 0;  // the next put adds this many fewer (a chest that changed meanwhile)
std::vector<std::pair<uint32_t, uint32_t>> dropped;  // give_item: (item, count)
struct Shown {
    bool open = false;
    std::string title;
    std::vector<exped::emfui::Widget> widgets;
    EmfUiCallback cb = nullptr;
    uint32_t close_key = 0;
} window;
std::map<std::string, std::string> hud;
std::vector<std::string> notes;
double now = 1000;

void set(uint32_t e, uint32_t hash, size_t size) {
    comps[{e, hash}] = std::vector<uint8_t>(size, 0);
    if (std::find(entities.begin(), entities.end(), e) == entities.end()) entities.push_back(e);
}
uint8_t* data(uint32_t e, uint32_t hash) {
    auto it = comps.find({e, hash});
    return it == comps.end() ? nullptr : it->second.data();
}
void place(uint32_t e, double x, double y, double z) {
    set(e, 0x027ac564, 56);
    auto* p = reinterpret_cast<int64_t*>(data(e, 0x027ac564));
    p[0] = static_cast<int64_t>(x * 4294967296.0);
    p[1] = static_cast<int64_t>(y * 4294967296.0);
    p[2] = static_cast<int64_t>(z * 4294967296.0);
}
struct Stack {
    uint32_t item, count, pide;
};
Stack* slots(uint32_t inv) { return reinterpret_cast<Stack*>(data(inv, 0xb19528a9)); }
uint32_t count_in(uint32_t inv, uint32_t item) {
    uint32_t n = 0;
    for (int i = 0; i < 8; ++i) n += slots(inv)[i].item == item ? slots(inv)[i].count : 0;
    return n;
}

// ---- the API ----
void log(const char*, const char* fmt, ...) {
    if (!std::getenv("EXPED_DEBUG")) return;
    va_list a;
    va_start(a, fmt);
    std::vfprintf(stderr, fmt, a);
    va_end(a);
    std::fputc('\n', stderr);
}
EmfWorld* server_world() { return kWorld; }
uint8_t world_kind(EmfWorld*) { return EMF_WORLD_SERVER; }
uint32_t local_player() { return 1; }
void* get_component(EmfWorld*, uint32_t hash, uint32_t entity, uint64_t* size) {
    auto it = comps.find({entity, hash});
    if (it == comps.end()) return nullptr;
    if (size) *size = it->second.size();
    return it->second.data();
}
void* get_template_component(EmfWorld*, uint32_t, uint32_t, uint64_t*) { return nullptr; }
void for_each_entity(EmfWorld*, EmfEntityCallback cb, void* user) {
    for (uint32_t e : std::vector<uint32_t>(entities)) cb(e, user);
}
void on_tick(EmfWorldCallback cb, void*) { tick_cb = cb; }
bool register_command(const char*, const char*, EmfCommandCallback cb, void*) {
    command_cb = cb;
    return true;
}
void command_reply(const char*, ...) {}
bool item_info(EmfWorld*, uint32_t id, EmfItemInfo* out) {
    if (!exped::item_name(id)) return false;
    out->item_id = id;
    out->max_stack = 50;
    return true;
}
bool put_items(EmfWorld*, uint32_t entity, uint32_t item, uint32_t count, EmfPutItemsCallback cb, void* user) {
    puts.push_back({entity, item, count, cb, user});
    return true;
}
bool take_items(EmfWorld*, const EmfItemsRequest* r) {
    takes.push_back(*r);
    return true;
}
uint32_t give_item(EmfWorld*, uint32_t, uint32_t item, uint32_t count) {
    dropped.emplace_back(item, count);
    return 77;
}
uint32_t server_session() { return 1; }
void notify(const char*, ...) {}
bool hud_set(const char* id, const char* text, uint32_t, uint32_t) {
    if (text && *text) hud[id] = text;
    else hud.erase(id);
    return true;
}
int64_t setting_int(const char*, const char*, int64_t fallback) { return fallback; }
double setting_float(const char*, const char*, double fallback) { return fallback; }
uint32_t setting_text(const char*, const char*, char* buf, uint32_t size) {
    if (size) buf[0] = 0;
    return 0;
}
void capture(const EmfWindowDef* def) {
    window.title = def->title ? def->title : "";
    window.widgets.clear();
    for (uint32_t i = 0; i < def->count; ++i) {
        const auto& w = def->widgets[i];
        exped::emfui::Widget x;
        x.type = w.type;
        x.flags = w.flags;
        x.key = w.key ? w.key : "";
        x.text = w.text ? w.text : "";
        window.widgets.push_back(x);
    }
}
bool refuse_windows = false;  // the overlay cannot draw (EMF's Vulkan layer is not in the game)
int open_tries = 0;
bool window_open(const EmfWindowDef* def) {
    ++open_tries;
    if (refuse_windows) return false;
    capture(def);
    window.open = true;
    window.cb = def->cb;
    window.close_key = def->close_key;
    return true;
}
bool window_update(const EmfWindowDef* def) {
    if (!window.open) return false;
    capture(def);
    return true;
}
bool window_close(const char*) {
    const bool was = window.open;
    window.open = false;
    return was;
}

EmfApi api() {
    EmfApi a{};
    a.version = 26;
    a.size = sizeof(EmfApi);
    a.log = log;
    a.server_world = server_world;
    a.world_kind = world_kind;
    a.local_player = local_player;
    a.get_component = get_component;
    a.get_template_component = get_template_component;
    a.for_each_entity = for_each_entity;
    a.on_tick = on_tick;
    a.register_command = register_command;
    a.command_reply = command_reply;
    a.item_info = item_info;
    a.put_items = put_items;
    a.take_items = take_items;
    a.give_item = give_item;
    a.server_session = server_session;
    a.notify = notify;
    a.hud_set = hud_set;
    a.setting_int = setting_int;
    a.setting_float = setting_float;
    a.setting_text = setting_text;
    a.window_open = window_open;
    a.window_update = window_update;
    a.window_close = window_close;
    return a;
}

// the core's next server step: requests applied, results reported
void step_requests() {
    auto p = puts;
    puts.clear();
    for (const auto& r : p) {
        const uint32_t inv = 3000;
        uint32_t left = r.count - std::min(r.count, put_shortfall);
        put_shortfall = 0;
        const uint32_t want = left;
        Stack* s = slots(inv);
        for (int i = 0; i < 8 && left; ++i)
            if (s[i].item == r.item && s[i].count < 50) {
                const uint32_t k = std::min(left, 50 - s[i].count);
                s[i].count += k;
                left -= k;
            }
        for (int i = 0; i < 8 && left; ++i)
            if (!s[i].item) {
                const uint32_t k = std::min(left, 50u);
                s[i] = {r.item, k, 0};
                left -= k;
            }
        if (r.cb) r.cb(kWorld, r.entity, r.item, r.count, want - left, r.user);
    }
    auto t = takes;
    takes.clear();
    for (const auto& r : t) {
        Stack* s = slots(static_cast<uint32_t>(r.slot & 0xFFFFFFFF));
        Stack& st = s[(r.slot >> 32) & 0xFF];
        const uint32_t done = st.item == r.item && st.count >= r.count ? r.count : 0;
        st.count -= done;
        if (!st.count) st = {0, 0, 0};
        if (r.cb) r.cb(kWorld, r.entity, r.item, r.count, done, r.user);
    }
}

void tick(double seconds = 0.25) {
    now += seconds;
    tick_cb(kWorld, nullptr);
}
double clock() { return now; }

const exped::emfui::Widget* find(const std::string& key) {
    for (const auto& w : window.widgets)
        if (w.key == key) return &w;
    return nullptr;
}
bool has_text(const std::string& part) {
    for (const auto& w : window.widgets)
        if (w.text.find(part) != std::string::npos) return true;
    return false;
}
void click(const std::string& key) {
    EmfUiEvent e{};
    e.size = sizeof e;
    e.kind = EMF_UI_CLICK;
    e.window = "expeditions.window";
    e.key = key.c_str();
    e.from_key = "";
    window.cb(kWorld, &e, nullptr);
}

}  // namespace fake

static void test_translate() {
    using namespace exped::emfui;
    bool ok = false;
    const json::Value w = json::parse(R"({"title":"T","content":[
        {"type":"row","children":[{"type":"heading","text":"Level"},{"type":"text","text":"2"},{"type":"text","text":"  "},{"type":"heading","text":"Bonus"},{"type":"text","text":"+6%"}]},
        {"type":"row","children":[{"type":"heading","text":"A quiet caption"}]},
        {"type":"list","id":"regions","entries":["»  Springlands   \tdanger low  \t","    Revelwood\tclosed\t"]},
        {"type":"tabs","id":"durations","tabs":["  15 min  ","30 min"],"selected":1},
        {"type":"toggle","id":"hud","label":"Panel","value":true},
        {"type":"row","children":[{"type":"heading","text":"Multiplier"},{"type":"text","text":"x1"},{"type":"button","id":"mult_down","label":"Less"}]},
        {"type":"item_grid","id":"preview","items":[{"id":722268629,"count":12}]},
        {"type":"text","text":"Make room — the loot will wait…"}]})", &ok);
    CHECK(ok);
    const Window x = translate(w, [](uint32_t id) { return id == 722268629u ? std::string("Wood Logs") : std::string("?"); });
    CHECK(x.title == "T");
    CHECK(x.widgets.size() >= 10);
    CHECK(x.widgets[0].type == Text && x.widgets[0].text == "Level: 2   Bonus: +6%");
    CHECK(x.widgets[1].type == Text && x.widgets[1].flags == Dim && x.widgets[1].text == "A quiet caption");
    CHECK(x.widgets[2].type == Row && x.widgets[2].key == "l:regions:0" && x.widgets[2].text == "Springlands (danger low)" && (x.widgets[2].flags & Selected));
    CHECK(x.widgets[3].text == "Revelwood (closed)" && !(x.widgets[3].flags & Selected));
    CHECK(x.widgets[4].type == Button && x.widgets[4].key == "t:durations:0" && x.widgets[4].text == "15 min" && !(x.widgets[4].flags & Selected));
    CHECK(x.widgets[5].key == "t:durations:1" && (x.widgets[5].flags & Selected) && (x.widgets[5].flags & SameLine));
    CHECK(x.widgets[6].key == "g:hud" && (x.widgets[6].flags & Selected));
    CHECK(x.widgets[7].type == Text && x.widgets[7].text == "Multiplier");
    CHECK(x.widgets[8].type == Text && x.widgets[8].text == "x1" && (x.widgets[8].flags & SameLine));
    CHECK(x.widgets[9].type == Button && x.widgets[9].key == "b:mult_down" && (x.widgets[9].flags & SameLine));
    CHECK(x.widgets[10].type == Row && x.widgets[10].text == "Wood Logs (12)" && x.widgets[10].key.empty());
    CHECK(x.widgets[11].text == "Make room - the loot will wait...");
    // events back to the mod
    const json::Value c = json::parse(click_event("l:regions:1", x));
    CHECK(c["op"].as_string() == "ui_event" && c["event"]["node"].as_string() == "regions" && c["event"]["event"]["kind"].as_string() == "click" &&
          c["event"]["event"]["index"].as_number() == 1);
    const json::Value g = json::parse(click_event("g:hud", x));
    CHECK(g["event"]["event"]["kind"].as_string() == "change" && g["event"]["event"]["value"].is_bool() && !g["event"]["event"]["value"].as_bool());
    const json::Value b = json::parse(click_event("b:mult_down", x));
    CHECK(b["event"]["node"].as_string() == "mult_down" && b["event"]["event"]["kind"].as_string() == "click");
    CHECK(click_event("x", x).empty() && click_event("l:regions:", x).empty() && click_event("l:regions:a1", x).empty());
    CHECK(json::parse(menu_event())["event"]["event"]["kind"].as_string() == "menu");
    // drawable text
    CHECK(drawable("Кузнец · ×2 «ок» ё") == "Кузнец · ×2 «ок» ё");
    CHECK(drawable("a\xE2\x80\x94" "b \xF0\x9F\x98\x80") == "a-b ?");
    CHECK(drawable("абвгд", 7) == "абв");  // cut at a whole letter
    // HUD
    const auto lines = hud_lines(json::parse(R"({"title":"Expeditions","rows":[{"label":"Blacksmith","value":"25 min","fraction":0.4}]})"));
    CHECK(lines.size() == 2 && lines[0] == "Expeditions" && lines[1] == "Blacksmith · 25 min");
    CHECK(hud_lines(json::parse(R"({"title":"Expeditions","rows":[]})")).empty());
}

static void test_names() {
    using exped::items::readable;
    CHECK(readable("Material_T2_Ore_Copper", false) == "Copper Ore");
    CHECK(readable("Material_T2_Ore_Copper", true) == "Медная руда");
    CHECK(exped::items::guess("Food_T9_raw_fruit_StarFruit") == "Star Fruit");
    CHECK(exped::items::guess("Material_T3_Fog_Essence07_ShroudCore") == "Shroud Core");
    CHECK(exped::items::guess("Weird") == "Weird");
    CHECK(readable("_Food_raw_bad_T4_fish_rare_02", false) == "Rare fish 2");
    CHECK(readable("_Food_raw_T3_fish_common_03", true) == "Обычная рыба 3");
    // every item in the default loot has a name of its own
    const exped::Config c = exped::parse_config(json::parse(kDefaultConfig));
    int missing = 0;
    for (const auto& p : c.professions)
        for (const auto& r : p.regions)
            for (const auto& l : r.loot) {
                const std::string n = exped::item_name(l.item) ? exped::item_name(l.item) : "";
                if (readable(n, true) == exped::items::guess(n)) {
                    ++missing;
                    std::fprintf(stderr, "no Russian name: %s\n", n.c_str());
                }
            }
    CHECK(missing == 0);
}

static void test_host() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / ("exped_emf_" + std::to_string(::getpid()));
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string d = dir.string() + "/";

    // the world: player 1 at the altar 2000, the blacksmith 2001, a chest 2002 with its inventory 3000
    const uint32_t berries = exped::item_id("Food_T1_raw_fruit_Red_Berries");
    const uint32_t flint = exped::item_id("Material_T1_Stone_Flint");
    CHECK(berries && flint);
    fake::place(1, 1000, 100, 1000);
    fake::set(1, 0x6ff967bb, 96);  // PlayerState
    fake::place(2000, 1000, 100, 1002);
    fake::set(2000, sl::comp::BaseIdComponent, 48);
    fake::set(2000, sl::comp::NetworkLevel, 2);
    fake::data(2000, sl::comp::NetworkLevel)[0] = 1;  // a 40 m build zone
    fake::place(2001, 1005, 100, 1000);
    fake::set(2001, 0x9e65507a, 136);  // DynamicActiveNpcState
    fake::data(2001, 0x9e65507a)[0] = sl::npc_state::Idle;
    fake::set(2001, sl::comp::ActiveNpcStateSaved, 48);
    float home[3] = {1005, 100, 1000};
    std::memcpy(fake::data(2001, sl::comp::ActiveNpcStateSaved), home, 12);
    fake::set(2001, sl::comp::Crafting, 20);
    const uint32_t blacksmith = 1103055054u;
    std::memcpy(fake::data(2001, sl::comp::Crafting) + 0x10, &blacksmith, 4);
    fake::place(2002, 1003, 100, 1003);
    fake::set(2002, sl::comp::InventorySetup, 84);
    {
        uint8_t* s = fake::data(2002, sl::comp::InventorySetup);
        const uint32_t inv = 3000;
        std::memcpy(s, &inv, 4);
        s[64] = 4;  // Generic
        s[81] = 8;
        s[82] = 8;
        s[83] = 1;
    }
    fake::set(3000, 0xb19528a9, 96);
    fake::slots(3000)[0] = {berries, 10, 0};

    const EmfApi a = fake::api();
    exped::emfhost::set_clock_for_tests(fake::clock);
    CHECK(exped::emfhost::start(&a, d));
    exped::emfhost::mod_for_tests().set_clock(fake::clock);
    exped::emfhost::mod_for_tests().set_seed(7);
    CHECK(fake::tick_cb && fake::command_cb);
    CHECK(fs::exists(dir / "expeditions_config.json"));
    fake::tick();

    // the console command opens the main window: the blacksmith is free
    fake::command_cb(fake::kWorld, 1, nullptr, nullptr);
    CHECK(fake::window.open);
    CHECK(fake::window.title == "Expeditions");
    CHECK(fake::find("l:survivors:0") && fake::find("l:survivors:0")->text.rfind("Blacksmith (", 0) == 0);
    CHECK(fake::find("b:settings"));

    // the send window
    fake::click("l:survivors:0");
    CHECK(fake::window.title == "Blacksmith: expedition");
    CHECK(fake::find("l:regions:0") && (fake::find("l:regions:0")->flags & exped::emfui::Selected));
    CHECK(fake::find("t:durations:0") && fake::find("t:durations:3"));
    CHECK(fake::find("t:rations:0") && fake::find("t:rations:0")->text == "Plain food (10)");
    CHECK(fake::has_text("Flint ("));  // the expected loot, by readable name
    CHECK(fake::find("g:repeat") && fake::find("b:go"));
    fake::click("g:repeat");
    CHECK(fake::find("g:repeat") && (fake::find("g:repeat")->flags & exped::emfui::Selected));
    fake::click("g:repeat");
    CHECK(fake::find("g:repeat") && !(fake::find("g:repeat")->flags & exped::emfui::Selected));

    // send: 15 minutes, 1 ration
    fake::click("t:durations:0");
    CHECK(fake::find("t:durations:0") && (fake::find("t:durations:0")->flags & exped::emfui::Selected));
    fake::click("b:go");
    const auto& list = exped::emfhost::mod_for_tests().expeditions();
    CHECK(list.size() == 1);
    CHECK(fake::window.title == "Expeditions" && fake::has_text("Away"));
    CHECK(fake::takes.size() == 1 && fake::takes[0].item == berries && fake::takes[0].count == 1 && fake::takes[0].entity == 2002);
    float now_home[3];
    std::memcpy(now_home, fake::data(2001, sl::comp::ActiveNpcStateSaved), 12);
    CHECK(std::abs(now_home[0] - 1005) > 10 || std::abs(now_home[2] - 1000) > 10);  // walks off
    CHECK(fake::data(2001, sl::comp::ActiveNpcStateSaved)[0xc] == 1);
    CHECK(fake::data(2001, 0x9e65507a)[0] == sl::npc_state::DecideTask);
    fake::step_requests();
    CHECK(fake::count_in(3000, berries) == 9);

    // the HUD shows them away
    fake::tick(6);
    CHECK(fake::hud.count("expeditions.hud.00") && fake::hud["expeditions.hud.00"] == "Expeditions");
    CHECK(fake::hud.count("expeditions.hud.01") && fake::hud["expeditions.hud.01"].rfind("Blacksmith · ", 0) == 0);

    // time is up: the loot into the chest, one step later
    exped::emfhost::mod_for_tests().advance(60 * 60);
    fake::tick(6);
    CHECK(!fake::puts.empty());
    uint32_t asked = 0;
    for (const auto& p : fake::puts) asked += p.entity == 2002 ? p.count : 0;
    CHECK(asked > 0);
    fake::step_requests();
    CHECK(fake::count_in(3000, flint) > 0);
    fake::tick(6);  // the next step: delivered, home again
    CHECK(list.empty());
    CHECK(exped::emfhost::mod_for_tests().history().size() == 1);
    CHECK(exped::emfhost::mod_for_tests().history().size() == 1 && exped::emfhost::mod_for_tests().history()[0].minutes == 15);
    std::memcpy(now_home, fake::data(2001, sl::comp::ActiveNpcStateSaved), 12);
    CHECK(now_home[0] == 1005 && now_home[2] == 1000);
    CHECK(fake::data(2001, sl::comp::ActiveNpcStateSaved)[0xc] == 0);
    CHECK(fake::hud.empty());
    fake::command_cb(fake::kWorld, 1, nullptr, nullptr);  // open again (it was open: it closes)
    CHECK(!fake::window.open);
    fake::command_cb(fake::kWorld, 1, nullptr, nullptr);
    CHECK(fake::window.open && fake::find("l:history:0"));
    fake::click("l:history:0");
    CHECK(fake::window.title == "Blacksmith: report" && fake::has_text("Flint ("));
    fake::click("b:back");

    // a chest that changed meanwhile: what does not fit drops at the player's feet
    fake::click("l:survivors:0");
    fake::click("b:go");
    fake::step_requests();
    exped::emfhost::mod_for_tests().advance(60 * 60);
    fake::tick(6);
    CHECK(!fake::puts.empty());
    fake::put_shortfall = 3;
    fake::step_requests();
    CHECK(!fake::dropped.empty() && fake::dropped[0].second == 3);

    // a full chest: nothing is asked for, the loot waits
    fake::tick(6);
    fake::tick(6);
    for (int i = 0; i < 8; ++i) fake::slots(3000)[i] = {berries, 50, 0};
    fake::click("b:back");
    fake::click("l:survivors:0");
    fake::click("b:go");
    fake::step_requests();
    exped::emfhost::mod_for_tests().advance(60 * 60);
    fake::puts.clear();
    fake::tick(6);
    CHECK(fake::puts.empty());
    CHECK(exped::emfhost::mod_for_tests().expeditions().size() == 1 && exped::emfhost::mod_for_tests().expeditions()[0].told_full);

    // EMF cannot draw windows: the mod is told it is closed and does not try again every refresh
    fake::command_cb(fake::kWorld, 1, nullptr, nullptr);
    if (fake::window.open) fake::command_cb(fake::kWorld, 1, nullptr, nullptr);
    fake::window.open = false;
    fake::refuse_windows = true;
    fake::open_tries = 0;
    fake::command_cb(fake::kWorld, 1, nullptr, nullptr);
    CHECK(fake::open_tries == 1 && !fake::window.open);
    for (int i = 0; i < 10; ++i) fake::tick(6);
    CHECK(fake::open_tries == 1);
    fake::refuse_windows = false;
    fake::command_cb(fake::kWorld, 1, nullptr, nullptr);
    CHECK(fake::window.open);

    fs::remove_all(dir);
}

int main() {
    exped::lang::set("en");
    test_translate();
    test_names();
    test_host();
    exped::page::set_plain(false);
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
