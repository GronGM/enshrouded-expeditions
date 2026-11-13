shroudlink inside ShroudForge 1.0.0
===================================
Run shroudlink and its mods (Expeditions and others) together with ShroudForge.

ShroudForge (https://www.nexusmods.com/enshrouded/mods/147) and shroudlink
(https://www.nexusmods.com/enshrouded/mods/278) both start through a dinput8.dll in the
game folder, so the second one installed replaces the first. ShroudForge can load other
DLLs from mods\<folder>\ when a native-plugin.ini says so. This zip is that folder for
shroudlink: shroudlink then starts as a ShroudForge native DLL mod.

INSTALL
1. Install ShroudForge as usual (its dinput8.dll, winmm.dll, dbghelp.dll and the
   shroudforge folder in the game folder).
2. Copy the mods folder from this zip into the game folder: mods\shroudlink\native-plugin.ini.
3. From shroudlink-singleplayer.zip:
   - dinput8.dll goes into mods\shroudlink\ (NOT into the game folder: that one is
     ShroudForge's);
   - shroudlink_core.dll goes into the game folder, next to enshrouded.exe, as usual.
4. shroudlink mods go into shroudlink_mods\ in the game folder, as usual
   (Expeditions: shroudlink_mods\expeditions.dll).
5. Only with Greeneye Loader mods (The Worst Map Mod, Town Portals, Hollow Halls
   Progression and the other mods by GreeneyeDS):
   - move ShroudForge's dbghelp.dll out of the game folder. ShroudForge still starts,
     through its dinput8.dll. With dbghelp.dll there the Greeneye Loader starts late,
     inside the Windows loader, its mods hook the game's UI before shroudlink can, and
     shroudlink's MODS tab does not appear;
   - install shroudlink first 1.1.0 or newer (greeneye\0-shroudlink-first): it starts
     shroudlink before the other Greeneye mods.
   A ShroudForge update may put dbghelp.dll back: check after updating.

CHECK
  F9 > Compatibility > DLL Manager: shroudlink is "Loaded".
  In the game: K shows the MODS tab.
  Logs: shroudlink_client.log ("native UI ready"), shroudlink.log,
  greeneye\loader.log ("start-up load", "shroudlink's jump is in").

UNINSTALL
  Delete mods\shroudlink. Put back dbghelp.dll if you moved it.

Tested in singleplayer with ShroudForge 1.7.9, shroudlink 0.8.0, Greeneye Loader 1.2.0
and The Worst Map Mod 1.3.34. Not made or checked by the authors of ShroudForge or
shroudlink.

----
shroudlink внутри ShroudForge
ShroudForge и shroudlink оба запускаются через dinput8.dll в папке игры, поэтому
поставленный вторым заменяет первый. ShroudForge умеет загружать dll из mods\<папка>\,
если там есть native-plugin.ini. Этот архив и есть такая папка для shroudlink.

УСТАНОВКА
1. Поставьте ShroudForge как обычно.
2. Скопируйте папку mods из архива в папку игры: mods\shroudlink\native-plugin.ini.
3. Из shroudlink-singleplayer.zip: dinput8.dll положите в mods\shroudlink\ (НЕ в папку
   игры: там dinput8.dll от ShroudForge), shroudlink_core.dll в папку игры, как обычно.
4. Моды shroudlink, как обычно, в shroudlink_mods\ (Экспедиции:
   shroudlink_mods\expeditions.dll).
5. Только если есть моды на загрузчике Greeneye (The Worst Map Mod, Town Portals,
   Hollow Halls Progression и другие моды GreeneyeDS):
   - уберите dbghelp.dll от ShroudForge из папки игры (ShroudForge запустится через свой
     dinput8.dll). С ним загрузчик Greeneye стартует слишком рано, его моды занимают
     интерфейс игры раньше shroudlink, и вкладки MODS не будет;
   - поставьте shroudlink first 1.1.0 или новее (greeneye\0-shroudlink-first).
   Обновление ShroudForge может вернуть dbghelp.dll: проверьте после обновления.

ПРОВЕРКА
  F9 > Compatibility > DLL Manager: у shroudlink «Loaded». В игре на K есть вкладка MODS.

УДАЛЕНИЕ
  Удалите mods\shroudlink. Верните dbghelp.dll, если убирали.

Source / исходники (MIT): https://github.com/GronGM/enshrouded-expeditions
