// Expeditions: a survivor at the base is sent away by the player, and comes back after a
// while with loot of their trade, which goes into the base's chests.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "json.h"
#include "platform.h"
#include "sdk.h"

namespace exped {

constexpr const char* kId = "expeditions";
constexpr const char* kVersion = "1.2.0";

struct LootEntry {
    uint32_t item = 0;
    std::string name;
    double min = 0, max = 0;  // per hour
    double chance = 1.0;      // per started hour
};

struct Region {
    std::string name;
    int area = -1;        // regions:: index (from "area" or the English name in brackets); -1: none
    double danger = 0.1;  // chance of getting hurt per hour away (before the survivor's level)
    std::vector<LootEntry> loot;
};

struct Profession {
    uint32_t workshop = 0;
    std::string name;
    std::vector<Region> regions;
};

struct Config {
    std::vector<int> durations_min;
    double multiplier = 1.0;
    bool offline_time = false;
    bool walk_away = true;
    double walk_beyond_m = 12;
    std::vector<Profession> professions;
    std::vector<std::string> problems;  // unknown items, a broken file …

    bool hud = true;                // a panel on the screen while expeditions are away
    bool supplies = true;           // expeditions take rations from the base's chests
    double rations_per_hour = 2;
    bool events = true;             // found a cache, got hurt …
    bool experience = true;         // survivors level up by time away
    double minutes_per_level = 30;  // level n -> n+1 takes n times this
    int max_level = 10;
    double loot_per_level = 0.06;   // +6% loot a level above 1
    double risk_per_level = 0.07;   // -7% risk a level above 1
    bool unlock_by_visiting = true; // a region's routes open once a player has been there
    std::string language = "auto";  // "auto" (Windows' language), "ru" or "en"
    // who may change the settings in the game's window (the file is always open to whoever has it)
    enum class Access { Host, Everyone, Nobody } settings_in_game = Access::Host;

    const Profession* profession(uint32_t workshop) const;
};

// Reads a config; problems go to `config.problems`, defaults fill what is missing.
Config parse_config(const json::Value& v);
json::Value config_to_json(const Config& c, const json::Value& original);
uint32_t item_id(const std::string& name);  // 0: unknown
const char* item_name(uint32_t id);          // nullptr: unknown

// A survivor's home point (`ecs::ActiveNpcStateSaved`).
struct Home {
    float pos[3] = {0, 0, 0};
    bool override_range = false;
    float range = 0;
};

struct Expedition {
    uint32_t workshop = 0;
    std::string who;
    std::string region;
    double duration_s = 0;
    double elapsed_s = 0;
    bool recalled = false;
    bool rolled = false;
    std::vector<std::pair<uint32_t, uint32_t>> pending;  // loot not in a chest yet
    uint32_t delivered = 0;
    bool walked = false;
    bool home_restored = false;
    Home original;
    float site[3] = {0, 0, 0};
    bool has_base = false;
    double base_center[3] = {0, 0, 0};
    double base_half = 0;
    double start_pos[3] = {0, 0, 0};
    bool told_full = false;

    // features
    int level = 1;           // the survivor's level when they left
    int rations_kind = -1;   // supplies:: kind taken along, -1: none
    uint32_t rations = 0;
    bool repeat = false;     // goes again after delivering
    std::string event;       // what happened ("" nothing), decided with the loot
    double loot_factor = 1;  // level, rations and event together
    std::vector<std::pair<uint32_t, uint32_t>> loot;  // all they brought (pending is what is left)

    bool time_up() const { return recalled || elapsed_s >= duration_s; }
};

struct Finished {
    std::string who, region;
    uint32_t items = 0;
    int64_t at = 0;
    uint32_t workshop = 0;
    double minutes = 0;
    int level = 1;      // after this expedition
    bool level_up = false;
    std::string event;
    std::vector<std::pair<uint32_t, uint32_t>> loot;
};

// What a survivor has learned on expeditions (by their workshop).
struct Survivor {
    double minutes = 0;  // time away
    uint32_t trips = 0;
    uint64_t items = 0;
};

// What one player's MODS window shows.
struct View {
    enum class Screen { Main, Send, Recall, Settings, Report } screen = Screen::Main;
    uint32_t workshop = 0;
    int region = -1;  // -1: the first open one
    int duration = 1;
    int rations = 0;
    bool repeat = false;
    size_t report = 0;  // history index
    std::string message;
    std::vector<uint32_t> survivors;  // list index -> workshop
    std::vector<uint32_t> active;     // list index -> workshop
    std::string open_window;
};

class Mod {
public:
    // files: config and state; empty dir: the game's folder
    void load(const sl::HostApi* api, const std::string& dir = std::string());
    void join(const sl::HostApi* api, void* ctx, uint64_t client);
    void leave(uint64_t client);
    void message(const sl::HostApi* api, void* ctx, uint64_t client, const std::string& text);
    void tick(const sl::HostApi* api, void* ctx);

    // for the tests
    const std::vector<Expedition>& expeditions() const { return list_; }
    const Config& config() const { return config_; }
    void set_seed(uint32_t seed) { rng_.seed(seed); }
    void set_clock(double (*clock)()) { clock_ = clock; last_tick_ = clock_(); }
    // The client of the player whose game runs the world. 0 (shroudlink): found in this process,
    // where that player's addon keeps its session; none on a dedicated server.
    void set_host_client(uint64_t client) { fixed_host_ = client; }
    uint64_t host_client() const;
    bool may_change_settings(uint64_t client) const;
    void advance(double seconds);  // as if the game ran that long (ticks do the rest)
    const std::vector<Finished>& history() const { return history_; }
    const std::map<uint32_t, Survivor>& survivors() const { return survivors_; }
    const std::set<int>& visited() const { return visited_; }
    int level_of(uint32_t workshop) const;
    bool region_open(const Region& r) const;
    double hurt_chance(const Region& r, double hours, int level) const;

private:
    struct World;

    void log(const std::string& text) const;
    void send(void* ctx, uint64_t client, const json::Value& message) const;
    void notify_all(void* ctx, const std::string& text) const;
    void show(void* ctx, uint64_t client);
    json::Value window(uint64_t client);
    void main_window(View& view, json::Value& content, uint32_t player);
    void send_window(View& view, json::Value& content, std::string& title, uint32_t player);
    void recall_window(View& view, json::Value& content, std::string& title);
    void settings_window(View& view, json::Value& content, uint64_t client);
    std::string settings_locked_text() const;
    void refuse_settings(uint64_t client, const std::string& what);
    void report_window(View& view, json::Value& content, std::string& title);
    void update_hud(void* ctx, bool force = false);
    void send_entries(void* ctx, uint64_t client) const;
    // names for the player, in their language
    std::string who(uint32_t workshop, const std::string& configured) const;
    std::string where(uint32_t workshop, const std::string& region_name) const;
    void track_visits(void* ctx);

    uint32_t player_of(uint64_t client) const;
    std::vector<std::pair<uint32_t, uint32_t>> survivors_near(uint32_t player) const;  // (entity, workshop)
    Expedition* find(uint32_t workshop);
    std::string start(void* ctx, uint64_t client, uint32_t workshop, int region, int duration, int rations, bool repeat);
    // sends the survivor `npc` away; "" when they left, else why not
    std::string launch(void* ctx, uint32_t npc, uint32_t player, const Profession& prof, const Region& region, double duration_s,
                       int rations, bool repeat, Expedition& out);
    void recall(void* ctx, uint32_t workshop);
    void finish(void* ctx, Expedition& x);
    // rations of each kind in the chests around an entity
    std::vector<uint32_t> chests_around(uint32_t entity) const;
    std::array<uint32_t, 3> rations_in(const std::vector<uint32_t>& chests) const;
    uint32_t rations_for(double hours) const;
    const Region* region_of(const Expedition& x) const;
    void step(void* ctx);  // returns, deliveries
    std::vector<std::pair<uint32_t, uint32_t>> roll(const Region& region, double hours, double factor);
    std::vector<std::pair<uint32_t, uint32_t>> expected(const Region& region, double hours, double factor) const;
    double level_bonus(int level) const;

    void load_config();
    void save_config();
    void load_state();
    void save_state();

    const sl::HostApi* api_ = nullptr;
    std::string dir_;
    Config config_;
    json::Value config_json_;
    bool config_broken_ = false;
    std::vector<Expedition> list_;
    std::vector<Finished> history_;
    std::map<uint32_t, Survivor> survivors_;
    std::set<int> visited_;
    bool visits_known_ = false;  // false: the state predates region tracking
    std::map<uint32_t, int> player_region_;  // player entity -> region they were last in
    std::map<uint64_t, std::string> hud_sent_;
    std::map<uint64_t, View> views_;
    uint64_t fixed_host_ = 0;
    std::set<uint64_t> refused_logged_;
    std::mt19937 rng_{std::random_device{}()};
    double (*clock_)() = &platform::steady_seconds;
    double last_tick_ = 0;
    double since_step_ = 0;
    double since_save_ = 0;
    double since_refresh_ = 0;
    bool dirty_ = false;
};

// Russian plural of "item" for a count: предмет / предмета / предметов.
std::string items_word(uint32_t n);
std::string format_minutes(double seconds);

}  // namespace exped
