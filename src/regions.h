// The world's regions, from the game's biome map (data: The Worst Map Mod, MIT,
// third_party/greeneye_regions.h). Positions are world metres (x east, z north).
#pragma once
#include <string>

namespace exped::regions {

constexpr int kCount = 8;
constexpr int kSpringlands = 0;

// The region at world (x, z), or -1 off the map.
int at(double x, double z);
// "Springlands", "Revelwood", ... (kCount names in level order); "" for -1.
const char* english(int region);
// The region of an English name ("Nomad Highlands", any case), or -1.
int by_english(const std::string& name);
// Default danger of a region (0..1): later regions are more dangerous.
double default_danger(int region);

}  // namespace exped::regions
