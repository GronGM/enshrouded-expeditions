// The shroudlink mod ABI (shroudlink mod/sdk/src/lib.rs, MIT): a mod DLL exports
// `shroudlink_mod_v2` returning a ModDecl; the host passes a HostApi table to every callback.
// Only C types cross the boundary (UTF-8 text with a length, caller-owned buffers).
#pragma once
#include <cstddef>
#include <cstdint>

namespace sl {

constexpr uint32_t kAbiVersion = 8;

struct Slot {
    uint64_t id;  // page entity | slot index << 32
    uint32_t item;
    uint32_t count;
};

struct InventoryEvent {
    uint32_t kind;
    uint32_t from;
    uint32_t to;
};

extern "C" {
struct HostApi {
    uint32_t version;
    void (*send)(void* ctx, uint64_t client, const uint8_t* json, size_t len);
    void (*log)(const uint8_t* text, size_t len);
    // v2
    size_t (*slots)(void* game, uint32_t entity, Slot* out, size_t cap);
    bool (*is_player)(void* game, uint32_t entity);
    size_t (*chest_name)(void* game, uint32_t entity, uint8_t* buf, size_t cap);
    uint32_t (*max_stack)(void* game, uint32_t item);
    uint32_t (*move_stack)(void* game, uint64_t slot, uint32_t to);
    // v4: the world, from any callback
    size_t (*players)(uint32_t* out, size_t cap);
    bool (*position)(uint32_t entity, double* out);
    bool (*teleport)(uint32_t entity, double x, double y, double z);
    uint32_t (*player_of)(uint64_t client);
    size_t (*read_component)(uint32_t entity, uint32_t type_hash, uint8_t* out, size_t cap);
    bool (*write_component)(uint32_t entity, uint32_t type_hash, const uint8_t* data, size_t len);
    size_t (*inventories_near)(uint32_t entity, double radius, uint32_t* out, size_t cap);
    size_t (*inventory)(uint32_t entity, Slot* out, size_t cap);
    uint32_t (*move_stack_between)(uint32_t from, uint64_t slot, uint32_t to);
    // v5
    bool (*apply_buff)(uint32_t entity, const uint8_t* buff, double seconds);
    size_t (*player_name)(uint32_t entity, uint8_t* buf, size_t cap);
    // v6
    size_t (*entities_near)(uint32_t entity, double radius, uint32_t type_hash, uint32_t* out, size_t cap);
    uint32_t (*move_items)(uint32_t from, uint64_t slot, uint32_t to, uint32_t count);
    // v7
    uint32_t (*give_items)(uint32_t entity, uint32_t item, uint32_t count);
    uint32_t (*remove_items)(uint32_t entity, uint64_t slot, uint32_t count);
    // v8
    size_t (*world_chest_name)(uint32_t entity, uint8_t* buf, size_t cap);
};

struct ModDecl {
    uint32_t version;
    void* state;
    const uint8_t* (*id)(void* state, size_t* len);
    void (*on_join)(void* state, const HostApi* api, void* ctx, uint64_t client);
    void (*on_message)(void* state, const HostApi* api, void* ctx, uint64_t client, const uint8_t* json, size_t len);
    void (*on_leave)(void* state, const HostApi* api, void* ctx, uint64_t client);
    // v2
    void (*on_load)(void* state, const HostApi* api);
    size_t (*keep)(void* state, const HostApi* api, void* game, const InventoryEvent* event, uint64_t* out, size_t cap);
    uint32_t (*on_transfer)(void* state, const HostApi* api, void* game, const InventoryEvent* event);
    // v3
    size_t (*game_events)(void* state, uint32_t* out, size_t cap);
    void (*on_game_event)(void* state, const HostApi* api, uint32_t hash, const uint8_t* data, size_t len);
    // v5
    void (*on_tick)(void* state, const HostApi* api, void* ctx);
};
}

// Component type ids (`qualifiedHash`, shroudlink mod/sdk/src/component_types.rs) and the
// fields this mod uses (shroudlink mods/assistants/src/base.rs).
namespace comp {
constexpr uint32_t ActiveNpcState = 0x1a8b5909;       // live state: +0x0 state byte
constexpr uint32_t ActiveNpcStateSaved = 0x1e9f0a88;  // 48 bytes: +0x0 spawnPosition float3, +0xc overrideRoamingRange, +0x10 roamingRange
constexpr uint32_t Crafting = 0xad776575;             // +0x10 workshopId
constexpr uint32_t InventorySetup = 0xc24df82a;       // assistants have it
constexpr uint32_t BaseIdComponent = 0x4927f4e2;      // the flame altar
constexpr uint32_t NetworkLevel = 0xaf520f3f;         // +0x0 level (u16)
// entities with an inventory that are no storage
constexpr uint32_t NotStorage[] = {
    0x05599679,  // FactoryStation
    0x072dbdb7,  // NpcCommon
    0x1a8b5909,  // ActiveNpcState
    0x6158772c,  // DestroyOnLoot
    0x9e424a03,  // InteractionLootRefill
    0xded05768,  // Growth
    0x061dda01,  // RandomLootContainer
    0xa7784b56,  // AnimalTarget
};
}  // namespace comp

namespace npc_state {
constexpr uint8_t Idle = 3, DecideTask = 4, Roam = 7, WaitingCloseToPlayer = 13;
}

}  // namespace sl
