Expeditions for EMF (Enshrouded Modding Framework) 1.0.0
========================================================
The same mod as the shroudlink version, for players who use EMF.
Send a survivor of your base on an expedition. They come back with loot of their
trade: the Blacksmith brings ore, stone and gems, the Carpenter wood, resin and fibre,
the Hunter hides, bones and meat, the Alchemist herbs, mushrooms and spores, the Farmer
crops, grain and wool, the Fisher fish.

WHAT YOU NEED
  EMF 0.5.0 or newer (https://www.nexusmods.com/enshrouded/mods/246).
  Singleplayer, or a world you host: the mod runs in the game that runs the world.
  On a dedicated server EMF has no windows; use the shroudlink version there.
  Do not install the shroudlink version (shroudlink_mods\expeditions.dll) at the same
  time: this one then stays off and says so.

INSTALL
  Unpack the zip into the game folder (next to enshrouded.exe):
  emf\mods\expeditions\expeditions.dll and mod.json.

HOW TO USE
1. Stand at your base near a survivor.
2. Press F10 (or type "expeditions" in EMF's console) and pick a survivor.
   The key can be changed in EMF's Mods tab (the key under Esc > Mods > Expeditions).
3. Pick a route, a duration and rations, look at the expected loot, click Send.
4. The survivor walks out past the edge of the base and waits there. When the time is
   up and you are at the base, they walk home and the loot goes into the base's chests
   (chests that already hold the item first). Without room the loot waits.
5. Click an expedition in the list to call the survivor back early or to stop repeating.
6. Click an entry under "Recently back" to see what they brought.

Experience, rations, events, regions that open once you have been there, repeat and the
panel on the screen work as in the shroudlink version (see README-expeditions.txt there,
or the Nexus page). Settings: Expeditions > Settings in the window, and
expeditions_config.json in the game folder. The panel's corner is in EMF's Mods tab.
Item names in the window are the mod's own (EMF cannot show the game's item names yet).

UNINSTALL
  Wait until everyone is back (a survivor still away stays outside the base; the
  Summoning Staff brings them back), then delete emf\mods\expeditions.

Open source (MIT): https://github.com/GronGM/enshrouded-expeditions
