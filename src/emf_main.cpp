// The EMF export: EMF loads <game>\emf\mods\expeditions\expeditions.dll and calls emf_mod_load.
#include "emf_host.h"

extern "C" __declspec(dllexport) bool emf_mod_load(const EmfApi* api) {
    try {
        return exped::emfhost::start(api);
    } catch (...) {
        return false;
    }
}
