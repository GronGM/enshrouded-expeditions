#include "lang.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "page_text.h"
#include "platform.h"
#include "regions.h"

namespace exped::lang {
namespace {

Lang g_lang = Lang::Ru;

struct Trade {
    uint32_t workshop;
    const char *ru, *en, *brings_ru, *brings_en;
};
constexpr Trade kTrades[] = {
    {1103055054, "Кузнец", "Blacksmith", "руда, камни и самоцветы", "ore, stone and gems"},
    {3185407206, "Плотник", "Carpenter", "дерево, смола и волокно", "wood, resin and fibre"},
    {2216351573, "Охотник", "Hunter", "шкуры, кости и мясо", "hides, bones and meat"},
    {3362288633, "Алхимик", "Alchemist", "травы, грибы и споры", "herbs, mushrooms and spores"},
    {3651361391, "Фермер", "Farmer", "урожай, зерно и шерсть", "crops, grain and wool"},
    {3299655940, "Рыбак", "Fisher", "рыба", "fish"},
};

// regions:: order: Springlands, Revelwood, Blackmire, Nomad Highlands, Kindlewastes,
// Albaneve Summits, Veilwater Basin, Cinder's Cradle
constexpr const char* kRegionRu[regions::kCount] = {"Весенние земли",   "Ревелвуд",          "Черная топь",      "Высокогорье кочевников",
                                                    "Тлеющие пустоши", "Вершины Альбанев", "Бассейн Вейлвотер", "Колыбель Пепла"};

const Trade* trade_of(uint32_t workshop) {
    for (const auto& t : kTrades) {
        if (t.workshop == workshop) return &t;
    }
    return nullptr;
}

}  // namespace

Lang current() { return g_lang; }

void set(const std::string& setting) {
    if (setting == "ru") g_lang = Lang::Ru;
    else if (setting == "en") g_lang = Lang::En;
    else g_lang = platform::system_reads_russian() ? Lang::Ru : Lang::En;
}

std::string trade(uint32_t workshop, const std::string& configured) {
    const Trade* t = trade_of(workshop);
    if (!t) return configured;
    if (!configured.empty() && configured != t->ru && configured != t->en) return configured;
    return ru() ? t->ru : t->en;
}

std::string region(int area, const std::string& configured) {
    const std::string own = page::short_region(configured);
    if (area < 0 || area >= regions::kCount) return own;
    const std::string en = regions::english(area);
    if (!configured.empty() && own != kRegionRu[area] && own != en) return own;
    return ru() ? kRegionRu[area] : en;
}

std::string brings(uint32_t workshop) {
    const Trade* t = trade_of(workshop);
    return t ? (ru() ? t->brings_ru : t->brings_en) : "";
}

std::string event_id(const std::string& saved) {
    if (saved.empty() || saved == "hurt" || saved == "cache" || saved == "rich" || saved == "weather") return saved;
    if (saved.find("ранен") != std::string::npos) return "hurt";
    if (saved.find("тайник") != std::string::npos) return "cache";
    if (saved.find("богатое") != std::string::npos) return "rich";
    if (saved.find("непогод") != std::string::npos) return "weather";
    return saved;
}

std::string event_text(const std::string& id) {
    const std::string e = event_id(id);
    if (e == "hurt") return L("был ранен, добычи вдвое меньше", "got hurt, half the loot");
    if (e == "cache") return L("нашёл тайник, добычи в полтора раза больше", "found a cache, half as much loot again");
    if (e == "rich") return L("нашёл богатое место, добычи на четверть больше", "found a rich spot, a quarter more loot");
    if (e == "weather") return L("попал в непогоду, добычи меньше", "ran into bad weather, less loot");
    return e;
}

std::string items_word(uint32_t n) {
    if (!ru()) return n == 1 ? "item" : "items";
    const uint32_t m100 = n % 100, m10 = n % 10;
    if (m100 >= 11 && m100 <= 14) return "предметов";
    if (m10 == 1) return "предмет";
    if (m10 >= 2 && m10 <= 4) return "предмета";
    return "предметов";
}

std::string routes_word(size_t n) {
    if (!ru()) return n == 1 ? "route" : "routes";
    const size_t m100 = n % 100, m10 = n % 10;
    if (m100 >= 11 && m100 <= 14) return "маршрутов";
    if (m10 == 1) return "маршрут";
    if (m10 >= 2 && m10 <= 4) return "маршрута";
    return "маршрутов";
}

std::string minutes(double seconds) {
    const long total = std::max(0L, static_cast<long>(std::ceil(seconds / 60.0)));
    const char* h = L("ч", "h");
    const char* m = L("мин", "min");
    char buf[48];
    if (total >= 60) {
        if (total % 60) std::snprintf(buf, sizeof buf, "%ld %s %ld %s", total / 60, h, total % 60, m);
        else std::snprintf(buf, sizeof buf, "%ld %s", total / 60, h);
    } else {
        std::snprintf(buf, sizeof buf, "%ld %s", total, m);
    }
    return buf;
}

}  // namespace exped::lang
