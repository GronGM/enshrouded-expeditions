// Readable item names for windows that cannot show the game's own (the EMF build): the loot
// items by their debug names in English and Russian, and a readable guess for any other item.
#pragma once
#include <string>

namespace exped::items {

// "Material_T2_Ore_Copper" -> "Copper Ore" / "Медная руда"; an unknown name -> a readable guess
std::string readable(const std::string& debug_name, bool russian);
// "Material_T1_Wood_Logs_Dead" -> "Wood Logs Dead": the debug name without its kind, tier and kind words
std::string guess(const std::string& debug_name);

}  // namespace exped::items
