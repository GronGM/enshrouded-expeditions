// The EMF build: the mod runs as an EMF mod (Enshrouded Modding Framework). EMF gives a C API
// (third_party/emf/emf_api.h); this file puts the shroudlink HostApi the mod is written against
// on top of it, and draws the mod's windows with EMF's mod windows. The game logic stays as it is.
#pragma once
#include <string>

#include "../third_party/emf/emf_api.h"

namespace exped {
class Mod;
}

namespace exped::emfhost {

constexpr const char* kModId = "expeditions";
constexpr uint32_t kMinApi = 24;  // mod windows

// emf_mod_load: false when the core is too old. `dir` holds the settings and state files ("": the game's folder).
bool start(const EmfApi* api, const std::string& dir = std::string());

// for the tests: the key that opens the window was pressed; the mod; the clock
void open_key_pressed(EmfWorld* world);
exped::Mod& mod_for_tests();
void set_clock_for_tests(double (*clock)());

}  // namespace exped::emfhost
