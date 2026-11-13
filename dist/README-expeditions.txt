Expeditions (a shroudlink mod for Enshrouded) 1.2.0
====================================================
Send a survivor of your base on an expedition. They come back with loot of their
trade: the Blacksmith brings ore, stone and gems, the Carpenter wood, resin and fibre,
the Hunter hides, bones and meat, the Alchemist herbs, mushrooms and spores, the Farmer
crops, grain and wool, the Fisher fish.

WHAT YOU NEED
  shroudlink (https://www.nexusmods.com/enshrouded/mods/278):
    singleplayer: shroudlink-singleplayer.zip in the game folder
    (dinput8.dll and shroudlink_core.dll next to enshrouded.exe);
    a server: the shroudlink core on the server, the addon (dinput8.dll) for players.
  This mod: shroudlink_mods\expeditions.dll (in singleplayer: in the game folder;
  on a server: next to the other shroudlink mods).
  With The Worst Map Mod (Greeneye Loader) also install the optional file
  "shroudlink first" (greeneye\0-shroudlink-first), or the game may crash when you
  enter the world.
  With ShroudForge: shroudlink runs as one of its native DLL mods, see the optional
  file "shroudlink inside ShroudForge" (README-shroudlink-in-shroudforge.txt).

HOW TO USE
1. Stand at your base near a survivor.
2. K (the MODS tab) > Expeditions > pick a survivor.
3. Pick a route, a duration and rations, look at the expected loot, click Send.
4. The survivor walks out past the edge of the base and waits there. When the time is
   up and you are at the base, they walk home and the loot goes into the base's chests
   (chests that already hold the item first). Without room the loot waits.
5. Click an expedition in the list to call the survivor back early (loot for the time
   gone only) or to stop repeating.
6. Click an entry under "Recently back" to see what they brought.

WHAT ELSE
  Panel        Everyone away, the time left, a progress bar. Drag it while a menu is open.
  Experience   Every minute away counts. A level gives +6% loot and -7% risk, and caches
               come more often. Level n -> n+1 takes n x 30 minutes away, up to level 10.
  Rations      2 portions of food an hour from the base's chests. Plain food (berries,
               vegetables, mushrooms): no bonus, roasted or dried food: +10% loot,
               dishes: +25%. The plainest food of the kind goes first. Herbs, eggs, raw
               meat and fish and bait are never taken.
  Events       A cache (x1.5), a rich spot (x1.25), bad weather (x0.8), an injury (x0.5).
               The risk of injury depends on the region and the length and is shown
               before you send someone.
  Regions      A route opens once you have been to its region yourself. The Springlands
               are always open.
  Repeat       "Repeat until I stop it": back and delivered, the survivor sets out again
               on the same route (with rations). Loot is only delivered while you are at
               the base.
  Language     Russian or English, as Windows; Settings > Language changes it.

SETTINGS
In the game: Expeditions > Settings: the language, the loot multiplier and a switch for
every feature. The rest is in expeditions_config.json in the game folder:
  durations_minutes             the durations to choose from, in minutes
  loot_multiplier               multiplies all loot
  time_passes_while_game_closed true: expeditions go on while the game is closed
  walk_away                     true: survivors leave the base while away
  walk_beyond_base_m            how far past the edge of the base they wait
  hud_panel, events, unlock_regions_by_visiting, language
  supplies                      enabled, rations_per_hour
  experience                    enabled, minutes_per_level, max_level,
                                loot_bonus_per_level, risk_reduction_per_level
  professions                   the trades (workshop: the survivor's workshop id),
                                their routes (regions) and loot:
                                item: the game's item name, per_hour: [least, most] an
                                hour away, chance: how likely per started hour.
                                A route can have danger (0..1, the chance of an injury
                                an hour) and area (the region's English name on the map).
After editing, click "Reload the settings file" in the settings. A broken file is left
alone: the mod runs with the defaults and shows the error in the settings.

WHO CAN CHANGE THE SETTINGS IN THE GAME
  settings_in_game              host (the default): only the player whose game runs the
                                world (singleplayer, or a world you host from the game).
                                On a dedicated server nobody: the admin edits this file
                                and anyone can press "Reload the settings file".
                                everyone: every player (as before 1.2.0).
                                nobody: only the file.
  The others see the settings without the buttons. The file itself is only for whoever
  can open the game's (or the server's) folder.

FILES
  expeditions_config.json   the settings
  expeditions_state.json    expeditions away, the survivors' experience, open regions
                            (do not delete it while someone is away)
  shroudlink.log            shroudlink's log, with the mod's lines ("expeditions: ...")

UNINSTALL
Wait until everyone is back (otherwise a survivor stays outside the base; the Summoning
Staff brings them back), then delete shroudlink_mods\expeditions.dll.

Source and license (MIT): https://github.com/GronGM/enshrouded-expeditions
The region map comes from The Worst Map Mod by Greeneye0 (MIT).
