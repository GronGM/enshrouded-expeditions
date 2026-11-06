// A Windows program that loads build/emf/expeditions.dll the way EMF's core does (LoadLibrary +
// emf_mod_load) with a small fake world, opens the window and picks the survivor. Run in Wine:
// it checks the DLL loads, its export, and that calls across the boundary work.
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../third_party/emf/emf_api.h"

static std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> comps;
static std::vector<uint32_t> ents;
static EmfWorldCallback tick_cb;
static EmfCommandCallback command_cb;
static EmfUiCallback ui_cb;
static std::string title;
static std::vector<std::string> keys, texts;
static int puts_n = 0, takes_n = 0, huds = 0;
static EmfWorld* const W = reinterpret_cast<EmfWorld*>(0x1000);

static void set(uint32_t e, uint32_t h, size_t n) {
    comps[{e, h}] = std::vector<uint8_t>(n, 0);
    bool known = false;
    for (auto x : ents) known = known || x == e;
    if (!known) ents.push_back(e);
}
static uint8_t* at(uint32_t e, uint32_t h) { return comps[{e, h}].data(); }
static void place(uint32_t e, double x, double y, double z) {
    set(e, 0x027ac564, 56);
    auto* p = reinterpret_cast<int64_t*>(at(e, 0x027ac564));
    p[0] = static_cast<int64_t>(x * 4294967296.0), p[1] = static_cast<int64_t>(y * 4294967296.0), p[2] = static_cast<int64_t>(z * 4294967296.0);
}

static void f_log(const char* mod, const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::printf("[%s] ", mod);
    std::vprintf(fmt, a);
    std::printf("\n");
    va_end(a);
}
static void f_notify(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::printf("note: ");
    std::vprintf(fmt, a);
    std::printf("\n");
    va_end(a);
}
static void f_reply(const char*, ...) {}
static uint8_t f_kind(EmfWorld*) { return EMF_WORLD_SERVER; }
static uint32_t f_local() { return 1; }
static void* f_get(EmfWorld*, uint32_t h, uint32_t e, uint64_t* s) {
    auto it = comps.find({e, h});
    if (it == comps.end()) return nullptr;
    if (s) *s = it->second.size();
    return it->second.data();
}
static void* f_tmpl(EmfWorld*, uint32_t, uint32_t, uint64_t*) { return nullptr; }
static void f_each(EmfWorld*, EmfEntityCallback cb, void* u) {
    for (auto e : std::vector<uint32_t>(ents)) cb(e, u);
}
static void f_on_tick(EmfWorldCallback cb, void*) { tick_cb = cb; }
static bool f_cmd(const char*, const char*, EmfCommandCallback cb, void*) {
    command_cb = cb;
    return true;
}
static bool f_info(EmfWorld*, uint32_t id, EmfItemInfo* o) {
    o->item_id = id;
    o->max_stack = 50;
    return true;
}
static bool f_put(EmfWorld*, uint32_t, uint32_t, uint32_t, EmfPutItemsCallback, void*) { return ++puts_n, true; }
static bool f_take(EmfWorld*, const EmfItemsRequest* r) { return ++takes_n, r->size == sizeof(EmfItemsRequest); }
static uint32_t f_give(EmfWorld*, uint32_t, uint32_t, uint32_t) { return 0; }
static uint32_t f_session() { return 1; }
static bool f_hud(const char* id, const char* text, uint32_t, uint32_t) {
    if (text && *text) std::printf("hud %s: %s\n", id, text);
    ++huds;
    return true;
}
static int64_t f_int(const char*, const char*, int64_t fb) { return fb; }
static double f_float(const char*, const char*, double fb) { return fb; }
static uint32_t f_text(const char*, const char* key, char* buf, uint32_t size) {
    std::snprintf(buf, size, "%s", std::strcmp(key, "open_key") == 0 ? "F10" : "");
    return static_cast<uint32_t>(std::strlen(buf));
}
static void capture(const EmfWindowDef* d) {
    title = d->title ? d->title : "";
    keys.clear();
    texts.clear();
    for (uint32_t i = 0; i < d->count; ++i) {
        const auto* w = reinterpret_cast<const EmfUiWidget*>(reinterpret_cast<const uint8_t*>(d->widgets) + i * d->widget_size);
        keys.push_back(w->key ? w->key : "");
        texts.push_back(w->text ? w->text : "");
    }
}
static bool open_ = false;
static bool f_open(const EmfWindowDef* d) {
    if (d->size != sizeof(EmfWindowDef) || d->widget_size != sizeof(EmfUiWidget)) return false;
    capture(d);
    ui_cb = d->cb;
    open_ = true;
    std::printf("window opened, close key 0x%x\n", d->close_key);
    return true;
}
static bool f_update(const EmfWindowDef* d) {
    if (!open_) return false;
    capture(d);
    return true;
}
static bool f_close(const char*) { return open_ = false, true; }

int main(int argc, char** argv) {
    const char* dll = argc > 1 ? argv[1] : "expeditions.dll";
    HMODULE m = LoadLibraryA(dll);
    if (!m) {
        std::printf("FAIL: LoadLibrary %s: %lu\n", dll, GetLastError());
        return 1;
    }
    auto load = reinterpret_cast<EmfModLoadFn>(reinterpret_cast<void*>(GetProcAddress(m, "emf_mod_load")));
    if (!load) {
        std::printf("FAIL: no emf_mod_load\n");
        return 1;
    }
    // the world: player 1, altar 2000, blacksmith 2001, chest 2002 / inventory 3000
    place(1, 1000, 100, 1000);
    set(1, 0x6ff967bb, 96);
    place(2000, 1000, 100, 1002);
    set(2000, 0x4927f4e2, 48);
    set(2000, 0xaf520f3f, 2);
    at(2000, 0xaf520f3f)[0] = 1;
    place(2001, 1005, 100, 1000);
    set(2001, 0x9e65507a, 136);
    at(2001, 0x9e65507a)[0] = 3;
    set(2001, 0x1e9f0a88, 48);
    set(2001, 0xad776575, 20);
    const uint32_t ws = 1103055054u;
    std::memcpy(at(2001, 0xad776575) + 16, &ws, 4);
    place(2002, 1003, 100, 1003);
    set(2002, 0xc24df82a, 84);
    const uint32_t inv = 3000;
    std::memcpy(at(2002, 0xc24df82a), &inv, 4);
    at(2002, 0xc24df82a)[64] = 4, at(2002, 0xc24df82a)[81] = 8, at(2002, 0xc24df82a)[82] = 8, at(2002, 0xc24df82a)[83] = 1;
    set(3000, 0xb19528a9, 96);

    EmfApi a{};
    a.version = 26, a.size = sizeof a;
    a.log = f_log, a.notify = f_notify, a.command_reply = f_reply, a.world_kind = f_kind, a.local_player = f_local;
    a.get_component = f_get, a.get_template_component = f_tmpl, a.for_each_entity = f_each, a.on_tick = f_on_tick;
    a.register_command = f_cmd, a.item_info = f_info, a.put_items = f_put, a.take_items = f_take, a.give_item = f_give;
    a.server_session = f_session, a.hud_set = f_hud, a.setting_int = f_int, a.setting_float = f_float, a.setting_text = f_text;
    a.window_open = f_open, a.window_update = f_update, a.window_close = f_close;

    if (!load(&a)) {
        std::printf("FAIL: emf_mod_load returned false\n");
        return 1;
    }
    if (!tick_cb || !command_cb) {
        std::printf("FAIL: no tick or command\n");
        return 1;
    }
    tick_cb(W, nullptr);
    command_cb(W, 1, nullptr, nullptr);
    if (!open_ || !ui_cb) {
        std::printf("FAIL: the window did not open\n");
        return 1;
    }
    std::printf("title: %s\n", title.c_str());
    for (size_t i = 0; i < keys.size(); ++i) std::printf("  [%s] %s\n", keys[i].c_str(), texts[i].c_str());
    EmfUiEvent e{};
    e.size = sizeof e, e.kind = EMF_UI_CLICK, e.window = "expeditions.window", e.key = "l:survivors:0", e.from_key = "";
    ui_cb(W, &e, nullptr);
    std::printf("after the click: %s (%zu widgets)\n", title.c_str(), keys.size());
    bool send = false;
    for (auto& k : keys) send = send || k == "b:go";
    std::printf("%s\n", send ? "OK" : "FAIL: no send window");
    return send ? 0 : 1;
}
