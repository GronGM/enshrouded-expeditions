// The config written on the first start (expeditions_config.json next to the game). Items are
// the game's debug names (shroudlink mod/categories/items.tsv); amounts are per hour away,
// [least, most], times "loot_multiplier"; "chance" (0..1) is per started hour.
#pragma once

inline constexpr const char* kDefaultConfig = R"JSON({
  "_help": "Expeditions: send a survivor away; they come back with loot by their trade. Edit this file and press 'Reload the settings file' in MODS > Expeditions > Settings. per_hour = [least, most] items per hour away; chance = how likely an item is per started hour (1 = always). Item names: shroudlink categories/items.tsv. Supplies: rations taken from the base's chests per hour away (raw food, roasted food, dishes: +0/+10/+25% loot). Events: caches, bad weather, getting hurt; a region's 'danger' (0..1, chance per hour of getting hurt) defaults by region, 'area' (the English region name) ties a route to the region on the map: with unlock_regions_by_visiting a route opens once a player has been there. Experience: time away levels a survivor up (more loot, less risk). language: auto (as Windows), ru or en. settings_in_game: who may change the settings in the game's window: host (the player whose game runs the world; on a dedicated server nobody, the admin edits this file), everyone, or nobody. Names of known trades and regions are shown in the player's language; write your own name to show that instead.",
  "durations_minutes": [15, 30, 60, 120],
  "loot_multiplier": 1.0,
  "time_passes_while_game_closed": false,
  "walk_away": true,
  "walk_beyond_base_m": 12,
  "hud_panel": true,
  "supplies": {"enabled": true, "rations_per_hour": 2},
  "events": true,
  "experience": {"enabled": true, "minutes_per_level": 30, "max_level": 10, "loot_bonus_per_level": 0.06, "risk_reduction_per_level": 0.07},
  "unlock_regions_by_visiting": true,
  "language": "auto",
  "settings_in_game": "host",
  "professions": [
    {
      "name": "Blacksmith", "workshop": 1103055054,
      "regions": [
        {"name": "Springlands", "loot": [
          {"item": "Material_T1_Stone_Flint", "per_hour": [40, 60]},
          {"item": "Material_T1_Metal_Scraps", "per_hour": [10, 20]},
          {"item": "Material_T1_Salt", "per_hour": [5, 10]}]},
        {"name": "Revelwood", "loot": [
          {"item": "Material_T2_Ore_Copper", "per_hour": [30, 50]},
          {"item": "Material_T2_Clay", "per_hour": [10, 20]},
          {"item": "Material_T1_Stone_Flint", "per_hour": [20, 30]},
          {"item": "Material_T2_Amber", "per_hour": [2, 5], "chance": 0.5}]},
        {"name": "Nomad Highlands", "loot": [
          {"item": "Material_T3_Ore_Tin", "per_hour": [30, 50]},
          {"item": "Material_T2_Ore_Copper", "per_hour": [15, 25]},
          {"item": "Material_T3_Fossilized_Bone", "per_hour": [3, 6]}]},
        {"name": "Kindlewastes", "loot": [
          {"item": "Material_T4_Ore_Iron", "per_hour": [30, 50]},
          {"item": "Material_T4_Sulfur", "per_hour": [15, 25]},
          {"item": "Material_T4_Nitrate", "per_hour": [10, 15]},
          {"item": "Material_T4_Lapislazuli", "per_hour": [2, 5], "chance": 0.5}]},
        {"name": "Albaneve Summits", "loot": [
          {"item": "Material_T5_Silver_Ore", "per_hour": [25, 40]},
          {"item": "Material_T5_Coal", "per_hour": [20, 30]},
          {"item": "Material_T5_Obsidian", "per_hour": [3, 6]},
          {"item": "Material_T5_Amethyst", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Veilwater Basin", "loot": [
          {"item": "Material_T6_Gold_Ore", "per_hour": [20, 35]},
          {"item": "Material_T6_MotherOfPearl", "per_hour": [3, 6]},
          {"item": "Material_T6_Aquamarine", "per_hour": [2, 4], "chance": 0.5},
          {"item": "Material_T6_Pearl", "per_hour": [1, 2], "chance": 0.3}]},
        {"name": "Cinder's Cradle", "loot": [
          {"item": "Material_T7_Celesthium_Ore", "per_hour": [15, 25]},
          {"item": "Material_T7_Moonstone", "per_hour": [2, 4], "chance": 0.5},
          {"item": "Material_T7_Diamond", "per_hour": [1, 3], "chance": 0.3}]}
      ]
    },
    {
      "name": "Carpenter", "workshop": 3185407206,
      "regions": [
        {"name": "Springlands", "loot": [
          {"item": "Material_T1_Wood_Logs", "per_hour": [60, 90]},
          {"item": "Material_T1_Twigs", "per_hour": [20, 30]},
          {"item": "Material_T1_Plant_Fibre", "per_hour": [20, 30]},
          {"item": "Material_T1_Resin", "per_hour": [10, 15]},
          {"item": "Material_T1_Wood_Logs_Dead", "per_hour": [10, 20]}]},
        {"name": "Revelwood", "loot": [
          {"item": "Material_T2_Wood_Hardwood", "per_hour": [30, 45]},
          {"item": "Material_T1_Wood_Logs", "per_hour": [40, 60]},
          {"item": "Material_T1_Resin", "per_hour": [15, 20]}]},
        {"name": "Nomad Highlands", "loot": [
          {"item": "Material_T2_Wood_Hardwood", "per_hour": [40, 60]},
          {"item": "Material_T3_Straw", "per_hour": [15, 25]},
          {"item": "Material_T1_Resin", "per_hour": [10, 15]}]},
        {"name": "Kindlewastes", "loot": [
          {"item": "Material_T4_Wood_Logs_Palm", "per_hour": [40, 60]},
          {"item": "Material_T1_Wood_Logs_Dead", "per_hour": [20, 30]}]},
        {"name": "Albaneve Summits", "loot": [
          {"item": "Material_T5_Wood_Logs_Pine", "per_hour": [40, 60]},
          {"item": "Material_T1_Resin", "per_hour": [15, 20]}]},
        {"name": "Veilwater Basin", "loot": [
          {"item": "Material_T6_Wood_Logs_Bamboo", "per_hour": [30, 50]},
          {"item": "Material_T6_Wood_Logs_Tropical", "per_hour": [30, 50]},
          {"item": "Material_T6_PalmLeaves", "per_hour": [10, 20]},
          {"item": "Material_T6_PaperReed", "per_hour": [10, 15]}]},
        {"name": "Cinder's Cradle", "loot": [
          {"item": "Material_T5_Wood_Logs_Pine", "per_hour": [30, 45]},
          {"item": "Material_T6_Wood_Logs_Tropical", "per_hour": [20, 30]}]}
      ]
    },
    {
      "name": "Hunter", "workshop": 2216351573,
      "regions": [
        {"name": "Springlands", "loot": [
          {"item": "Material_T1_Animal_Fur", "per_hour": [10, 20]},
          {"item": "Material_T1_Bones", "per_hour": [15, 25]},
          {"item": "Food_T1_raw_bad_meat_LeanMeat", "per_hour": [5, 10]},
          {"item": "Food_T1_raw_bad_meat_WolfMeat", "per_hour": [3, 6]}]},
        {"name": "Revelwood", "loot": [
          {"item": "Material_T2_Reptile_Hide", "per_hour": [8, 14]},
          {"item": "Food_T2_raw_bad_meat_Game", "per_hour": [6, 10]},
          {"item": "Material_T1_Bones", "per_hour": [10, 20]},
          {"item": "Material_T2_Giant_Bones", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Nomad Highlands", "loot": [
          {"item": "Material_T3_Feather", "per_hour": [10, 20]},
          {"item": "Food_T3_raw_bad_meat_Bird", "per_hour": [5, 10]},
          {"item": "Food_T3_raw_egg_BirdEggs", "per_hour": [4, 8]},
          {"item": "Material_T2_Reptile_Hide", "per_hour": [5, 10]}]},
        {"name": "Kindlewastes", "loot": [
          {"item": "Material_T4_Scales", "per_hour": [8, 14]},
          {"item": "Food_T4_raw_bad_meat_SanddiggerMeat", "per_hour": [5, 10]},
          {"item": "Material_T1_Bones", "per_hour": [10, 15]}]},
        {"name": "Albaneve Summits", "loot": [
          {"item": "Material_T5_raw_Wool", "per_hour": [10, 20]},
          {"item": "Food_T5_raw_bad_meat_fat", "per_hour": [5, 10]},
          {"item": "Material_T5_Giant_Bones", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Veilwater Basin", "loot": [
          {"item": "Material_T6_Water_Reptile_Hide", "per_hour": [8, 14]},
          {"item": "Material_T6_LizardScales", "per_hour": [6, 10]},
          {"item": "Food_T6_raw_bad_meat_CrabMeat", "per_hour": [5, 10]},
          {"item": "Material_T6_LizardClaws", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Cinder's Cradle", "loot": [
          {"item": "Material_T7_WildBeast_Scales", "per_hour": [6, 10]},
          {"item": "Material_T5_Giant_Bones", "per_hour": [3, 6]}]}
      ]
    },
    {
      "name": "Alchemist", "workshop": 3362288633,
      "regions": [
        {"name": "Springlands", "loot": [
          {"item": "Material_T1_Fog_Essence01_ShroudSpores", "per_hour": [10, 20]},
          {"item": "Food_T1_raw_mushroom_Mushroom_brown", "per_hour": [8, 12]},
          {"item": "Food_T1_raw_herb_StJohnsWort", "per_hour": [5, 10]},
          {"item": "Material_T1_MushroomDrop_ShroudLiquid", "per_hour": [5, 10]},
          {"item": "Food_T1_raw_nightplant_NightLily", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Revelwood", "loot": [
          {"item": "Food_T2_raw_bad_mushroom_AzureRussula", "per_hour": [6, 10]},
          {"item": "Food_T2_raw_herb_StingingNettle", "per_hour": [5, 10]},
          {"item": "Material_T2_MushroomDrop_ShroudMush", "per_hour": [5, 10]},
          {"item": "Material_T2_BrittleShroudPetals", "per_hour": [5, 10]},
          {"item": "Material_T2_Chamomile", "per_hour": [4, 8]}]},
        {"name": "Nomad Highlands", "loot": [
          {"item": "Material_T3_Fog_Essence02_ShroudFragment", "per_hour": [10, 15]},
          {"item": "Material_T3_MushroomDrop_GreenMushroomMeat", "per_hour": [5, 10]},
          {"item": "Material_T3_Rooibos", "per_hour": [4, 8]},
          {"item": "Food_T3_raw_herb_Sage_Leaves", "per_hour": [4, 8]},
          {"item": "Material_T3_Night_AurumLily", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Kindlewastes", "loot": [
          {"item": "Material_T4_Aloe", "per_hour": [5, 8]},
          {"item": "Material_T4_Saffron", "per_hour": [3, 6]},
          {"item": "Material_T4_Desert_Flower", "per_hour": [3, 6]},
          {"item": "Material_T4_MushroomDrop_BrittleShroudFlakes", "per_hour": [5, 10]},
          {"item": "Food_T4_raw_herb_DevilsClaw", "per_hour": [4, 8]}]},
        {"name": "Albaneve Summits", "loot": [
          {"item": "Material_T5_Wolfsbane", "per_hour": [4, 8]},
          {"item": "Material_T5_MushroomDrop_FrozenShroudSolids", "per_hour": [5, 10]},
          {"item": "Food_T5_raw_herb_Gentian", "per_hour": [4, 8]},
          {"item": "Food_T5_raw_bad_mushroom_YellowFogMushroom", "per_hour": [4, 8]},
          {"item": "Material_T5_Night_Blackflower", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Veilwater Basin", "loot": [
          {"item": "Material_T6_Fog_Essence03_ShroudRelic", "per_hour": [8, 12]},
          {"item": "Material_T6_MushroomDrop_PinkMushroomMeat", "per_hour": [5, 10]},
          {"item": "Material_T6_Daylily_Flower", "per_hour": [3, 6]},
          {"item": "Food_T6_raw_mushroom_Shiitake", "per_hour": [4, 8]},
          {"item": "Material_T6_Night_GleamRoot_under_water", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Cinder's Cradle", "loot": [
          {"item": "Material_T7_LavaPlant_Magma_Gel", "per_hour": [4, 8]},
          {"item": "Food_T7_raw_herb_Thyme", "per_hour": [4, 8]},
          {"item": "Material_T7_Sparkling_Dust", "per_hour": [2, 4], "chance": 0.5}]}
      ]
    },
    {
      "name": "Farmer", "workshop": 3651361391,
      "regions": [
        {"name": "Springlands", "loot": [
          {"item": "Food_T1_raw_fruit_Red_Berries", "per_hour": [10, 20]},
          {"item": "Food_T1_raw_vegetable_Tomato", "per_hour": [5, 10]},
          {"item": "Food_T1_raw_grain_Corncob", "per_hour": [5, 10]},
          {"item": "Material_T1_Plant_Fibre", "per_hour": [15, 25]},
          {"item": "Food_T1_raw_sweets_Honey", "per_hour": [2, 4], "chance": 0.5}]},
        {"name": "Revelwood", "loot": [
          {"item": "Food_T2_raw_fruit_Strawberry", "per_hour": [8, 12]},
          {"item": "Food_T2_raw_grain_Hazelnut", "per_hour": [8, 12]},
          {"item": "Food_T2_raw_vegetable_ForestBeet", "per_hour": [5, 10]},
          {"item": "Material_T2_Flax", "per_hour": [10, 20]},
          {"item": "Material_T2_Indigo_Plant", "per_hour": [3, 6]}]},
        {"name": "Nomad Highlands", "loot": [
          {"item": "Material_T3_Wheat_Grains", "per_hour": [10, 20]},
          {"item": "Food_T3_raw_vegetable_RedPepper", "per_hour": [5, 10]},
          {"item": "Food_T3_raw_sweets_SugarCane", "per_hour": [5, 10]},
          {"item": "Material_T3_Straw", "per_hour": [10, 20]}]},
        {"name": "Kindlewastes", "loot": [
          {"item": "Food_T4_raw_fruit_Date", "per_hour": [6, 10]},
          {"item": "Food_T4_raw_fruit_YuccaFruit", "per_hour": [5, 10]}]},
        {"name": "Albaneve Summits", "loot": [
          {"item": "Food_T5_raw_vegetables_Pumpkin", "per_hour": [4, 8]},
          {"item": "Material_T5_raw_Wool", "per_hour": [5, 10]}]},
        {"name": "Veilwater Basin", "loot": [
          {"item": "Material_T6_raw_Rice", "per_hour": [8, 12]},
          {"item": "Food_T6_raw_fruit_YellowBanana", "per_hour": [5, 10]},
          {"item": "Food_T6_raw_vegetable_Ginger", "per_hour": [4, 8]},
          {"item": "Material_T6_raw_CoffeeBeans", "per_hour": [4, 8]}]},
        {"name": "Cinder's Cradle", "loot": [
          {"item": "Food_T7_raw_fruit_Artichoke", "per_hour": [4, 8]},
          {"item": "Food_T7_raw_fruit_Pomegranate", "per_hour": [4, 8]},
          {"item": "Food_T7_raw_vegetable_Cabbage", "per_hour": [4, 8]},
          {"item": "Food_T7_raw_bad_vegetable_Beans", "per_hour": [5, 10]}]}
      ]
    },
    {
      "name": "Fisher", "workshop": 3299655940,
      "regions": [
        {"name": "Springlands", "loot": [
          {"item": "_Food_raw_bad_T1_fish_common_01", "per_hour": [3, 6]},
          {"item": "_Food_raw_bad_T1_fish_common_02", "per_hour": [3, 6]}]},
        {"name": "Revelwood", "loot": [
          {"item": "_Food_raw_bad_T2_fish_uncommon_01", "per_hour": [3, 5]},
          {"item": "_Food_raw_bad_T2_fish_uncommon_02", "per_hour": [3, 5]}]},
        {"name": "Nomad Highlands", "loot": [
          {"item": "_Food_raw_T3_fish_common_03", "per_hour": [3, 6]},
          {"item": "_Food_raw_T3_fish_uncommon_03", "per_hour": [2, 4]}]},
        {"name": "Kindlewastes", "loot": [
          {"item": "_Food_raw_bad_T4_fish_rare_01", "per_hour": [2, 4]},
          {"item": "_Food_raw_bad_T4_fish_rare_02", "per_hour": [2, 4]}]},
        {"name": "Albaneve Summits", "loot": [
          {"item": "_Food_raw_T5_fish_rare_03", "per_hour": [2, 4]},
          {"item": "_Food_raw_bad_T5_fish_rare_04", "per_hour": [2, 4]}]},
        {"name": "Veilwater Basin", "loot": [
          {"item": "_Food_raw_bad_T6_fish_epic_01", "per_hour": [1, 3]},
          {"item": "_Food_raw_T6_fish_epic_02", "per_hour": [1, 3]},
          {"item": "_Food_raw_bad_T6_fish_epic_03", "per_hour": [1, 3]}]},
        {"name": "Cinder's Cradle", "loot": [
          {"item": "_Food_raw_bad_T7_fish_epic_04", "per_hour": [1, 3]},
          {"item": "_Food_raw_bad_T7_fish_epic_05", "per_hour": [1, 3]}]}
      ]
    }
  ]
}
)JSON";
