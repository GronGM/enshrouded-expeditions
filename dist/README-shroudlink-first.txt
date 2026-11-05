shroudlink first (a Greeneye Loader mod) 1.0.0
=============================================
For players of shroudlink AND The Worst Map Mod (or another Greeneye Loader mod that
hooks the in-world UI).

Why: at start shroudlink (dinput8.dll) and the map mod hook the same game function (the
UI in the world), at the same moment, from two threads. When the map mod writes its hook
in the middle of shroudlink's, the game crashes on its first frame in the world. This mod
starts first among the Greeneye mods and holds the others back until shroudlink's hook is
in (usually under a second, 30 s at most). The map mod then chains onto it and both work.
Without shroudlink it does nothing.

Install: greeneye\0-shroudlink-first\ (shroudlink-first.dll and mod.json) in the game
folder. Keep the folder name: the loader starts its mods in alphabetical order.
Log: greeneye\loader.log ("shroudlink first: waited ... ms") and
greeneye\0-shroudlink-first\shroudlink-first.log (a check 20 s after start:
"hook chain is sound" means all is well).
Uninstall: delete greeneye\0-shroudlink-first.

----
shroudlink first (мод для загрузчика Greeneye)
Для тех, у кого стоят shroudlink И The Worst Map Mod. При запуске оба мода перехватывают
одну и ту же функцию игры (интерфейс в мире) одновременно, и если мод карты влезает
посреди перехвата shroudlink, игра падает при входе в мир. Этот мод запускается первым
и держит остальные моды Greeneye, пока shroudlink не закончит (обычно меньше секунды,
не дольше 30 с). Без shroudlink ничего не делает.
Установка: greeneye\0-shroudlink-first\ в папке игры; имя папки не менять.
Удаление: удалите папку greeneye\0-shroudlink-first.

Source / исходники (MIT): https://github.com/GronGM/enshrouded-expeditions
