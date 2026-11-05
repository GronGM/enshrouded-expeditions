// Rations for an expedition: food from the base's chests, by kind (raw food, roasted or
// dried food, cooked dishes). Better rations bring more loot.
#pragma once
#include <cstdint>

namespace exped::supplies {

constexpr int kKinds = 3;

// "простая еда", "жареное", "блюда" (for "Припасы: ...")
const char* kind_name(int kind);
// Extra loot for a kind of rations (0.25: +25%).
double bonus(int kind);
// The kind of rations an item is, or -1 (not food a survivor takes along: ingredients,
// bait, herbs, raw meat …).
int kind_of(uint32_t item);
// The item's tier (1-7 from "Food_T3_..."), 9 when unknown: rations are taken lowest first.
int tier_of(uint32_t item);

}  // namespace exped::supplies
