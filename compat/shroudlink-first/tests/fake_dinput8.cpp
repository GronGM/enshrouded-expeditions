// Stands in for shroudlink's dinput8.dll in the tests: only its name and folder matter.
#include <windows.h>
extern "C" __declspec(dllexport) int FakeShroudlinkAddon() { return 1; }
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
