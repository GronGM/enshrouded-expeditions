shroudlink first (a Greeneye Loader mod) 1.0.0
=============================================
For players who use shroudlink together with mods that run on the Greeneye Loader:
GreeneyeDS's mods such as The Worst Map Mod, Town Portals or Hollow Halls Progression.

Why: at start shroudlink (dinput8.dll) and these mods hook the same game function (the UI
in the world), at the same moment, from two threads. When a Greeneye mod writes its hook in
the middle of shroudlink's, the game crashes on its first frame in the world. This mod
starts first among the Greeneye mods and holds the others back until shroudlink's hook is
in (usually under a second, 30 s at most). The other mods then chain onto it and all of
them work. Without shroudlink it does nothing.

Install: copy the greeneye folder from the zip into the game folder (next to
enshrouded.exe), so you get greeneye\0-shroudlink-first\ (shroudlink-first.dll and
mod.json). Keep the folder name: the loader starts its mods in alphabetical order. The
Greeneye Loader itself (dbgcore.dll) comes with the Greeneye mods.
Log: greeneye\loader.log ("shroudlink first: waited ... ms") and
greeneye\0-shroudlink-first\shroudlink-first.log (a check 20 s after start:
"hook chain is sound" means all is well).
Uninstall: delete greeneye\0-shroudlink-first.

----
shroudlink first (мод для загрузчика Greeneye)
Для тех, у кого shroudlink стоит вместе с модами на загрузчике Greeneye: моды GreeneyeDS,
например The Worst Map Mod, Town Portals или Hollow Halls Progression. При запуске
shroudlink и эти моды перехватывают одну и ту же функцию игры (интерфейс в мире)
одновременно, и если мод Greeneye влезает посреди перехвата shroudlink, игра падает при
входе в мир. Этот мод запускается первым и держит остальные моды Greeneye, пока shroudlink
не закончит (обычно меньше секунды, не дольше 30 с). Без shroudlink ничего не делает.
Установка: папку greeneye из архива скопировать в папку игры (рядом с enshrouded.exe),
получится greeneye\0-shroudlink-first\; имя папки не менять.
Удаление: удалите папку greeneye\0-shroudlink-first.

Source / исходники (MIT): https://github.com/GronGM/enshrouded-expeditions
(folder compat/shroudlink-first)
