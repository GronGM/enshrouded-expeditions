// Stands in for the shroudlink addon installed as a ShroudForge native DLL mod.
#include <windows.h>
extern "C" __declspec(dllexport) void shroudlink_shutdown() {}
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
