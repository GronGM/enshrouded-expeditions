// The shroudlink export: `shroudlink_mod_v2` returns the mod's declaration (ABI v8).
#include <string>

#include "expeditions.h"
#include "sdk.h"

namespace {

exped::Mod& mod(void* state) { return *static_cast<exped::Mod*>(state); }

const uint8_t* id(void*, size_t* len) {
    *len = std::char_traits<char>::length(exped::kId);
    return reinterpret_cast<const uint8_t*>(exped::kId);
}
// nothing may unwind into the host (Rust): a failure is logged and the call is dropped
template <class F>
void guarded(const sl::HostApi* api, const char* what, F&& f) {
    try {
        f();
    } catch (...) {
        if (api && api->log) {
            const std::string line = std::string("expeditions: ") + what + " failed";
            api->log(reinterpret_cast<const uint8_t*>(line.data()), line.size());
        }
    }
}

void on_join(void* state, const sl::HostApi* api, void* ctx, uint64_t client) {
    guarded(api, "on_join", [&] { mod(state).join(api, ctx, client); });
}
void on_message(void* state, const sl::HostApi* api, void* ctx, uint64_t client, const uint8_t* json, size_t len) {
    guarded(api, "on_message", [&] { mod(state).message(api, ctx, client, std::string(reinterpret_cast<const char*>(json), len)); });
}
void on_leave(void* state, const sl::HostApi* api, void*, uint64_t client) {
    guarded(api, "on_leave", [&] { mod(state).leave(client); });
}
void on_load(void* state, const sl::HostApi* api) {
    guarded(api, "on_load", [&] { mod(state).load(api); });
}
size_t keep(void*, const sl::HostApi*, void*, const sl::InventoryEvent*, uint64_t*, size_t) { return 0; }
uint32_t on_transfer(void*, const sl::HostApi*, void*, const sl::InventoryEvent*) { return 0; }
size_t game_events(void*, uint32_t*, size_t) { return 0; }
void on_game_event(void*, const sl::HostApi*, uint32_t, const uint8_t*, size_t) {}
void on_tick(void* state, const sl::HostApi* api, void* ctx) {
    guarded(api, "on_tick", [&] { mod(state).tick(api, ctx); });
}

}  // namespace

extern "C" __declspec(dllexport) const sl::ModDecl* shroudlink_mod_v2() {
    static exped::Mod instance;
    static const sl::ModDecl decl = {
        sl::kAbiVersion, &instance, id, on_join, on_message, on_leave, on_load, keep, on_transfer, game_events, on_game_event, on_tick,
    };
    return &decl;
}
