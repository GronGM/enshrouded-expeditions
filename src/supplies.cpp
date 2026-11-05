#include "supplies.h"

#include <cstring>
#include <unordered_map>
#include <utility>

#include "items_table.h"
#include "lang.h"

namespace exped::supplies {
namespace {

bool has(const char* name, const char* part) { return std::strstr(name, part) != nullptr; }

int classify(const char* n) {
    if (std::strncmp(n, "Food_T", 6) != 0 || n[6] < '1' || n[6] > '9' || n[7] != '_') return -1;
    if (has(n, "_Bait") || has(n, "_bad_") || has(n, "DEPRECATED") || has(n, "Anniversary") || has(n, "Sugar")) return -1;
    if (has(n, "_dish_") || has(n, "Skeleton_Slurp")) return 2;
    if (has(n, "_processed_") || has(n, "_proccessed_")) return 1;
    static const char* const kRaw[] = {"_raw_fruit_", "_raw_vegetable_", "_raw_grain_", "_raw_mushroom_", "_raw_sweets_"};
    for (const char* r : kRaw) {
        if (has(n, r)) return 0;
    }
    return -1;  // raw herbs, eggs, meat, fish: ingredients
}

const std::unordered_map<uint32_t, std::pair<int, int>>& table() {
    static const auto t = [] {
        std::unordered_map<uint32_t, std::pair<int, int>> m;
        for (const auto& i : kItems) {
            const int k = classify(i.name);
            if (k >= 0) m.emplace(i.id, std::make_pair(k, i.name[6] - '0'));
        }
        return m;
    }();
    return t;
}

}  // namespace

const char* kind_name(int kind) {
    switch (kind) {
    case 0: return lang::L("простая еда", "plain food");
    case 1: return lang::L("жареное", "roasted food");
    case 2: return lang::L("блюда", "dishes");
    default: return "";
    }
}

double bonus(int kind) {
    switch (kind) {
    case 1: return 0.1;
    case 2: return 0.25;
    default: return 0;
    }
}

int kind_of(uint32_t item) {
    const auto& t = table();
    const auto it = t.find(item);
    return it == t.end() ? -1 : it->second.first;
}

int tier_of(uint32_t item) {
    const auto& t = table();
    const auto it = t.find(item);
    return it == t.end() ? 9 : it->second.second;
}

}  // namespace exped::supplies
