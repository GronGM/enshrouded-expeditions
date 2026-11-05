#include "regions.h"

#include <cctype>
#include <cstddef>
#include <cstdint>

namespace exped::regions {
namespace map {
#include "../third_party/greeneye_regions.h"
}  // namespace map

static_assert(map::kRegionCount == kCount, "the region table changed");

int at(double x, double z) {
    if (!(x >= 0 && z >= 0)) return -1;
    const int cx = static_cast<int>(x) / map::kRegionCellMeters, cz = static_cast<int>(z) / map::kRegionCellMeters;
    if (cx >= map::kRegionGridSize || cz >= map::kRegionGridSize) return -1;
    int along = 0;
    for (uint32_t r = map::kRegionRowStart[cz]; r < map::kRegionRowStart[cz + 1]; ++r) {
        along += map::kRegionRuns[r].cells;
        if (cx < along) return map::kRegionRuns[r].region < kCount ? map::kRegionRuns[r].region : -1;
    }
    return -1;
}

const char* english(int region) { return region >= 0 && region < kCount ? map::kRegionNameEnglish[region] : ""; }

int by_english(const std::string& name) {
    for (int r = 0; r < kCount; ++r) {
        const std::string e = map::kRegionNameEnglish[r];
        if (e.size() != name.size()) continue;
        bool same = true;
        for (size_t i = 0; i < e.size() && same; ++i)
            same = std::tolower(static_cast<unsigned char>(e[i])) == std::tolower(static_cast<unsigned char>(name[i]));
        if (same) return r;
    }
    return -1;
}

double default_danger(int region) {
    // Springlands, Revelwood, Blackmire, Nomad Highlands, Kindlewastes, Albaneve, Veilwater, Cinder's Cradle
    static constexpr double kDanger[kCount] = {0.04, 0.08, 0.12, 0.12, 0.16, 0.2, 0.24, 0.3};
    return region >= 0 && region < kCount ? kDanger[region] : 0.12;
}

}  // namespace exped::regions
