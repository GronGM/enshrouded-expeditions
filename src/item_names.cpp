#include "item_names.h"

#include <cctype>
#include <cstring>
#include <vector>

namespace exped::items {
namespace {

struct Name {
    const char* debug;
    const char* en;
    const char* ru;
};

// The items in the default loot tables. The names follow the game's wording where it is known
// and are a plain description otherwise.
constexpr Name kNames[] = {
    {"Food_T1_raw_bad_meat_LeanMeat", "Lean Meat", "Постное мясо"},
    {"Food_T1_raw_bad_meat_WolfMeat", "Wolf Meat", "Волчье мясо"},
    {"Food_T1_raw_fruit_Red_Berries", "Red Berries", "Красные ягоды"},
    {"Food_T1_raw_grain_Corncob", "Corncob", "Кукурузный початок"},
    {"Food_T1_raw_herb_StJohnsWort", "St. John's Wort", "Зверобой"},
    {"Food_T1_raw_mushroom_Mushroom_brown", "Brown Mushroom", "Бурый гриб"},
    {"Food_T1_raw_nightplant_NightLily", "Night Lily", "Ночная лилия"},
    {"Food_T1_raw_sweets_Honey", "Honey", "Мёд"},
    {"Food_T1_raw_vegetable_Tomato", "Tomato", "Помидор"},
    {"Food_T2_raw_bad_meat_Game", "Game Meat", "Дичь"},
    {"Food_T2_raw_bad_mushroom_AzureRussula", "Azure Russula", "Лазурная сыроежка"},
    {"Food_T2_raw_fruit_Strawberry", "Strawberry", "Клубника"},
    {"Food_T2_raw_grain_Hazelnut", "Hazelnut", "Лесной орех"},
    {"Food_T2_raw_herb_StingingNettle", "Stinging Nettle", "Крапива"},
    {"Food_T2_raw_vegetable_ForestBeet", "Forest Beet", "Лесная свёкла"},
    {"Food_T3_raw_bad_meat_Bird", "Bird Meat", "Мясо птицы"},
    {"Food_T3_raw_egg_BirdEggs", "Bird Eggs", "Птичьи яйца"},
    {"Food_T3_raw_herb_Sage_Leaves", "Sage Leaves", "Листья шалфея"},
    {"Food_T3_raw_sweets_SugarCane", "Sugar Cane", "Сахарный тростник"},
    {"Food_T3_raw_vegetable_RedPepper", "Red Pepper", "Красный перец"},
    {"Food_T4_raw_bad_meat_SanddiggerMeat", "Sanddigger Meat", "Мясо песчаного копателя"},
    {"Food_T4_raw_fruit_Date", "Dates", "Финики"},
    {"Food_T4_raw_fruit_YuccaFruit", "Yucca Fruit", "Плод юкки"},
    {"Food_T4_raw_herb_DevilsClaw", "Devil's Claw", "Коготь дьявола"},
    {"Food_T5_raw_bad_meat_fat", "Fat", "Жир"},
    {"Food_T5_raw_bad_mushroom_YellowFogMushroom", "Yellow Fog Mushroom", "Жёлтый туманный гриб"},
    {"Food_T5_raw_herb_Gentian", "Gentian", "Горечавка"},
    {"Food_T5_raw_vegetables_Pumpkin", "Pumpkin", "Тыква"},
    {"Food_T6_raw_bad_meat_CrabMeat", "Crab Meat", "Крабовое мясо"},
    {"Food_T6_raw_fruit_YellowBanana", "Banana", "Банан"},
    {"Food_T6_raw_mushroom_Shiitake", "Shiitake", "Шиитаке"},
    {"Food_T6_raw_vegetable_Ginger", "Ginger", "Имбирь"},
    {"Food_T7_raw_bad_vegetable_Beans", "Beans", "Бобы"},
    {"Food_T7_raw_fruit_Artichoke", "Artichoke", "Артишок"},
    {"Food_T7_raw_fruit_Pomegranate", "Pomegranate", "Гранат"},
    {"Food_T7_raw_herb_Thyme", "Thyme", "Тимьян"},
    {"Food_T7_raw_vegetable_Cabbage", "Cabbage", "Капуста"},
    {"Material_T1_Animal_Fur", "Animal Fur", "Шкура животного"},
    {"Material_T1_Bones", "Bones", "Кости"},
    {"Material_T1_Fog_Essence01_ShroudSpores", "Shroud Spores", "Споры Покрова"},
    {"Material_T1_Metal_Scraps", "Metal Scraps", "Металлолом"},
    {"Material_T1_MushroomDrop_ShroudLiquid", "Shroud Liquid", "Жидкость Покрова"},
    {"Material_T1_Plant_Fibre", "Plant Fibre", "Растительное волокно"},
    {"Material_T1_Resin", "Resin", "Смола"},
    {"Material_T1_Salt", "Salt", "Соль"},
    {"Material_T1_Stone_Flint", "Flint", "Кремень"},
    {"Material_T1_Twigs", "Twigs", "Ветки"},
    {"Material_T1_Wood_Logs", "Wood Logs", "Брёвна"},
    {"Material_T1_Wood_Logs_Dead", "Dead Wood Logs", "Сухие брёвна"},
    {"Material_T2_Amber", "Amber", "Янтарь"},
    {"Material_T2_BrittleShroudPetals", "Brittle Shroud Petals", "Хрупкие лепестки Покрова"},
    {"Material_T2_Chamomile", "Chamomile", "Ромашка"},
    {"Material_T2_Clay", "Clay", "Глина"},
    {"Material_T2_Flax", "Flax", "Лён"},
    {"Material_T2_Giant_Bones", "Giant Bones", "Кости великана"},
    {"Material_T2_Indigo_Plant", "Indigo", "Индиго"},
    {"Material_T2_MushroomDrop_ShroudMush", "Shroud Mush", "Грибная масса Покрова"},
    {"Material_T2_Ore_Copper", "Copper Ore", "Медная руда"},
    {"Material_T2_Reptile_Hide", "Reptile Hide", "Шкура рептилии"},
    {"Material_T2_Wood_Hardwood", "Hardwood", "Твёрдая древесина"},
    {"Material_T3_Feather", "Feathers", "Перья"},
    {"Material_T3_Fog_Essence02_ShroudFragment", "Shroud Fragment", "Осколок Покрова"},
    {"Material_T3_Fossilized_Bone", "Fossilized Bone", "Окаменелая кость"},
    {"Material_T3_MushroomDrop_GreenMushroomMeat", "Green Mushroom Flesh", "Мякоть зелёного гриба"},
    {"Material_T3_Night_AurumLily", "Aurum Lily", "Золотая лилия"},
    {"Material_T3_Ore_Tin", "Tin Ore", "Оловянная руда"},
    {"Material_T3_Rooibos", "Rooibos", "Ройбуш"},
    {"Material_T3_Straw", "Straw", "Солома"},
    {"Material_T3_Wheat_Grains", "Wheat", "Пшеница"},
    {"Material_T4_Aloe", "Aloe", "Алоэ"},
    {"Material_T4_Desert_Flower", "Desert Flower", "Пустынный цветок"},
    {"Material_T4_Lapislazuli", "Lapis Lazuli", "Лазурит"},
    {"Material_T4_MushroomDrop_BrittleShroudFlakes", "Brittle Shroud Flakes", "Хрупкие хлопья Покрова"},
    {"Material_T4_Nitrate", "Nitrate", "Селитра"},
    {"Material_T4_Ore_Iron", "Iron Ore", "Железная руда"},
    {"Material_T4_Saffron", "Saffron", "Шафран"},
    {"Material_T4_Scales", "Scales", "Чешуя"},
    {"Material_T4_Sulfur", "Sulfur", "Сера"},
    {"Material_T4_Wood_Logs_Palm", "Palm Logs", "Пальмовые брёвна"},
    {"Material_T5_Amethyst", "Amethyst", "Аметист"},
    {"Material_T5_Coal", "Coal", "Уголь"},
    {"Material_T5_Giant_Bones", "Huge Bones", "Огромные кости"},
    {"Material_T5_MushroomDrop_FrozenShroudSolids", "Frozen Shroud Solids", "Мёрзлые сгустки Покрова"},
    {"Material_T5_Night_Blackflower", "Blackflower", "Чёрный цветок"},
    {"Material_T5_Obsidian", "Obsidian", "Обсидиан"},
    {"Material_T5_Silver_Ore", "Silver Ore", "Серебряная руда"},
    {"Material_T5_Wolfsbane", "Wolfsbane", "Волчий аконит"},
    {"Material_T5_Wood_Logs_Pine", "Pine Logs", "Сосновые брёвна"},
    {"Material_T5_raw_Wool", "Wool", "Шерсть"},
    {"Material_T6_Aquamarine", "Aquamarine", "Аквамарин"},
    {"Material_T6_Daylily_Flower", "Daylily", "Лилейник"},
    {"Material_T6_Fog_Essence03_ShroudRelic", "Shroud Relic", "Реликвия Покрова"},
    {"Material_T6_Gold_Ore", "Gold Ore", "Золотая руда"},
    {"Material_T6_LizardClaws", "Lizard Claws", "Когти ящера"},
    {"Material_T6_LizardScales", "Lizard Scales", "Чешуя ящера"},
    {"Material_T6_MotherOfPearl", "Mother of Pearl", "Перламутр"},
    {"Material_T6_MushroomDrop_PinkMushroomMeat", "Pink Mushroom Flesh", "Мякоть розового гриба"},
    {"Material_T6_Night_GleamRoot_under_water", "Gleam Root", "Светящийся корень"},
    {"Material_T6_PalmLeaves", "Palm Leaves", "Пальмовые листья"},
    {"Material_T6_PaperReed", "Paper Reed", "Папирус"},
    {"Material_T6_Pearl", "Pearl", "Жемчуг"},
    {"Material_T6_Water_Reptile_Hide", "Water Reptile Hide", "Шкура водной рептилии"},
    {"Material_T6_Wood_Logs_Bamboo", "Bamboo", "Бамбук"},
    {"Material_T6_Wood_Logs_Tropical", "Tropical Logs", "Тропические брёвна"},
    {"Material_T6_raw_CoffeeBeans", "Coffee Beans", "Кофейные зёрна"},
    {"Material_T6_raw_Rice", "Rice", "Рис"},
    {"Material_T7_Celesthium_Ore", "Celesthium Ore", "Целестиевая руда"},
    {"Material_T7_Diamond", "Diamond", "Алмаз"},
    {"Material_T7_LavaPlant_Magma_Gel", "Magma Gel", "Магмовый гель"},
    {"Material_T7_Moonstone", "Moonstone", "Лунный камень"},
    {"Material_T7_Sparkling_Dust", "Sparkling Dust", "Искрящаяся пыль"},
    {"Material_T7_WildBeast_Scales", "Wild Beast Scales", "Чешуя дикого зверя"},
};

// words of a debug name that say what kind of item it is, not which
bool kind_word(const std::string& w) {
    static const char* const kWords[] = {"Food", "Material", "raw", "bad", "fruit", "grain", "herb", "mushroom", "nightplant", "sweets",
                                         "vegetable", "vegetables", "egg", "MushroomDrop", "Fog", "Night", "Item", "Consumable"};
    for (const char* k : kWords) {
        if (w == k) return true;
    }
    if (w.size() >= 2 && w.size() <= 3 && w[0] == 'T' && std::isdigit(static_cast<unsigned char>(w[1]))) return true;  // T1..T12
    if (w.rfind("Essence", 0) == 0) return true;                                                                         // Essence01
    return false;
}

}  // namespace

std::string guess(const std::string& debug_name) {
    std::vector<std::string> words;
    size_t at = 0;
    while (at <= debug_name.size()) {
        size_t end = debug_name.find('_', at);
        if (end == std::string::npos) end = debug_name.size();
        const std::string w = debug_name.substr(at, end - at);
        if (!w.empty() && !kind_word(w)) words.push_back(w);
        at = end + 1;
    }
    if (words.empty()) return debug_name;
    std::string out;
    for (const auto& w : words) {
        // CamelCase -> separate words
        for (size_t i = 0; i < w.size(); ++i) {
            const auto c = static_cast<unsigned char>(w[i]);
            if (i > 0 && std::isupper(c) && std::islower(static_cast<unsigned char>(w[i - 1]))) out += ' ';
            out += static_cast<char>(c);
        }
        out += ' ';
    }
    out.pop_back();
    if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

// the game's fish have no species in their debug names: "_Food_raw_bad_T4_fish_rare_02" -> "Rare fish 2"
std::string fish(const std::string& debug_name, bool russian) {
    static const char* const kKinds[][3] = {{"common", "Common fish", "Обычная рыба"},
                                            {"uncommon", "Uncommon fish", "Необычная рыба"},
                                            {"rare", "Rare fish", "Редкая рыба"},
                                            {"epic", "Epic fish", "Эпическая рыба"},
                                            {"legendary", "Legendary fish", "Легендарная рыба"}};
    const size_t at = debug_name.find("_fish_");
    if (at == std::string::npos) return "";
    const std::string rest = debug_name.substr(at + 6);  // "rare_02"
    const size_t us = rest.find('_');
    const std::string kind = rest.substr(0, us);
    std::string number = us == std::string::npos ? "" : rest.substr(us + 1);
    while (number.size() > 1 && number[0] == '0') number.erase(0, 1);
    for (const auto& k : kKinds) {
        if (kind == k[0]) return std::string(russian ? k[2] : k[1]) + (number.empty() ? "" : " " + number);
    }
    return "";
}

std::string readable(const std::string& debug_name, bool russian) {
    for (const auto& n : kNames) {
        if (debug_name == n.debug) return russian ? n.ru : n.en;
    }
    const std::string f = fish(debug_name, russian);
    return f.empty() ? guess(debug_name) : f;
}

}  // namespace exped::items
