# Changelog

## shroudlink first 1.1.0, shroudlink inside ShroudForge 1.0.0
- `shroudlink-in-shroudforge-1.0.0.zip`: runs shroudlink (and so this mod) inside ShroudForge, as one of
  its native DLL mods (`mods\shroudlink\native-plugin.ini`). Tested in singleplayer with ShroudForge 1.7.9.
- shroudlink first 1.1.0 (the Greeneye Loader mod): finds shroudlink installed as a ShroudForge mod and
  starts it before the other Greeneye mods. When the loader starts its mods inside the Windows loader
  (ShroudForge's `dbghelp.dll` makes it load late), it no longer waits 30 s for a hook that cannot come,
  and its log says to move that `dbghelp.dll` out of the game folder.

## 1.1.0
- A build for EMF (Enshrouded Modding Framework 0.5.0+): `expeditions-emf-<version>.zip`, the window on F10
  (EMF's mod windows), the panel as EMF HUD lines, items into chests through EMF's inventory transactions
  (what does not fit after all drops at the player's feet), readable item names in English and Russian.
- A chest that has not loaded yet (right after a world loads) no longer makes the mod say the chests are full:
  the loot waits quietly until the chests are there.
- EMF build: when EMF cannot draw windows (the game runs as administrator, for example with UAC off), the log says
  so once and why; the guide has the one-line fix.

## 1.0.0
First release.
- Send a survivor of the base on an expedition: route (region), duration, rations. They walk
  out past the edge of the base and come back with loot of their trade into the base's chests.
- Survivors level up with time away: more loot, less risk.
- Rations from the base's chests: plain food, roasted food (+10% loot) or dishes (+25%).
- Events on the way: a cache, a rich spot, bad weather, an injury (risk by region and length).
- Routes open once you have been to their region yourself.
- Repeat: the survivor delivers and sets out again.
- A panel on the screen with everyone away; a report of each trip with the items.
- Russian and English (as Windows, or chosen in the settings).
- Optional: `shroudlink first` for players of The Worst Map Mod (fixes a crash on entering the world).
