// Test harness (runs under Wine): plays the Greeneye Loader and the shroudlink addon around
// shroudlink-first.dll. The fake play-state UI function lives in this exe's .text.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

asm(".text\n.globl fake_psu\nfake_psu:\n"
    ".byte 0x4C,0x8B,0xDC,0x55,0x49,0x8D,0xAB,0xE8,0xFD,0xFF,0xFF,0x48,0x81,0xEC,0x10,0x03,0x00,0x00\n"
    ".byte 0x48,0x8B,0x05,0,0,0,0,0x48,0x8B,0x88,0xD0,0,0,0,0x48,0x8B,0x01\n"
    ".byte 0x48,0x85,0xC0,0x0F,0x84,0,0,0,0,0x4D,0x89,0x73,0xC8,0x4C,0x8B,0x70,0x38\n"
    "ret\n"
    ".globl fake_relay\nfake_relay:\nret\nret\n"
    ".globl fake_other\nfake_other:\nret\nret\n");
extern "C" void fake_psu();
extern "C" void fake_relay();
extern "C" void fake_other();

struct GreeneyeHost {
    uint32_t size;
    uint32_t apiVersion;
    int side;
    const wchar_t* modDir;
    const wchar_t* gameDir;
    void (*log)(const char* line);
};

static ULONGLONG g_t0;
static void HostLog(const char* line)
{
    printf("[loader +%llu ms] %s\n", GetTickCount64() - g_t0, line);
    fflush(stdout);
}

static uint8_t* Fn() { return reinterpret_cast<uint8_t*>(&fake_psu); }

static void Write(uint8_t* at, const void* bytes, size_t n)
{
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
}

static void JumpTo(uint8_t* at, void* to)
{
    uint8_t j[5] = {0xE9};
    int32_t d = static_cast<int32_t>(reinterpret_cast<uint8_t*>(to) - (at + 5));
    memcpy(j + 1, &d, 4);
    Write(at, j, 5);
}

static std::wstring g_dir;
static void AppendLog(const char* text, long long stamp = -1)
{
    if (stamp < 0) stamp = static_cast<long long>(time(nullptr));
    FILE* f = _wfopen((g_dir + L"shroudlink_client.log").c_str(), L"ab");
    fprintf(f, "[%lld] %s\n", stamp, text);
    fclose(f);
}

struct Addon {
    DWORD hookAfterMs; // 0: never hooks
    const char* finalLine;
    DWORD lineAfterMs;
};

static DWORD WINAPI AddonThread(void* p)
{
    auto* a = static_cast<Addon*>(p);
    if (a->hookAfterMs) {
        Sleep(a->hookAfterMs);
        JumpTo(Fn(), reinterpret_cast<void*>(&fake_relay));
    }
    if (a->finalLine) {
        Sleep(a->lineAfterMs);
        AppendLog(a->finalLine);
    }
    return 0;
}

int wmain(int argc, wchar_t** argv)
{
    g_t0 = GetTickCount64();
    std::wstring mode = argc > 1 ? argv[1] : L"normal";
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g_dir = exe;
    g_dir = g_dir.substr(0, g_dir.rfind(L'\\') + 1);
    std::wstring modDir = g_dir + L"greeneye\\0-shroudlink-first\\";
    DeleteFileW((g_dir + L"shroudlink_client.log").c_str());
    AppendLog("shroudlink client 0.8.0 starting (an older session)", 1000);
    AppendLog("native UI ready: game ui +0x283f4a8, builder +0x283f408 (an older session)", 1000);

    if (mode != L"nodinput") {
        if (!LoadLibraryW((g_dir + L"dinput8.dll").c_str())) {
            printf("could not load the fake dinput8.dll\n");
            return 2;
        }
    }
    Addon addon{400, "native UI ready: game ui +0x283f4a8, builder +0x283f408", 30};
    if (mode == L"disabled") addon = {0, "native UI disabled: pattern not found: 4C 8B DC", 300};
    if (mode == L"child") addon = {0, "this copy has already started in this process; not starting again", 50};
    if (mode == L"loaderfirst") {
        const uint8_t movRax[18] = {0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8, 0xFF, 0xE0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        Write(Fn(), movRax, sizeof movRax);
        addon = {0, nullptr, 0};
    }
    if (mode == L"broken") {
        const uint8_t movRax[18] = {0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8, 0xFF, 0xE0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        Write(Fn(), movRax, sizeof movRax);
        JumpTo(Fn(), reinterpret_cast<void*>(&fake_relay));
        addon = {0, nullptr, 0};
    }
    if (mode == L"blind") {
        const uint8_t junk[3] = {0x90, 0x90, 0x90};
        Write(Fn() + 20, junk, sizeof junk);
        addon = {0, nullptr, 0};
    }
    if (mode == L"nolog") addon = {700, nullptr, 0};
    if (mode == L"logfirst") addon = {500, "native UI ready: (logged before the jump settles)", 0};
    CreateThread(nullptr, 0, AddonThread, &addon, 0, nullptr);

    HMODULE mod = LoadLibraryW((modDir + L"shroudlink-first.dll").c_str());
    if (!mod) {
        printf("could not load shroudlink-first.dll (%lu)\n", GetLastError());
        return 2;
    }
    auto start = reinterpret_cast<bool (*)(const GreeneyeHost*)>(GetProcAddress(mod, "GreeneyeModStart"));
    GreeneyeHost host{sizeof(GreeneyeHost), 2, 1, modDir.c_str(), g_dir.c_str(), HostLog};
    // "loaderlock": the loader calls the mod from inside a DllMain (a late load)
    RTL_CRITICAL_SECTION* loaderLock = nullptr;
    if (mode == L"loaderlock") {
        const auto* peb = reinterpret_cast<const uint8_t*>(__readgsqword(0x60));
        loaderLock = *reinterpret_cast<RTL_CRITICAL_SECTION* const*>(peb + 0x110);
        if (loaderLock) EnterCriticalSection(loaderLock);
        printf("holding the loader lock: %s\n", loaderLock ? "yes" : "no (not found)");
    }
    const ULONGLONG before = GetTickCount64();
    const bool ok = start(&host);
    const ULONGLONG took = GetTickCount64() - before;
    if (loaderLock) LeaveCriticalSection(loaderLock);
    printf("RESULT mode=%ls ok=%d took=%llu first=%02X\n", mode.c_str(), ok, took, Fn()[0]);

    if (mode == L"normal" || mode == L"chain") {
        if (mode == L"chain") {
            // the next mod chains: only the jump's target changes
            Sleep(100);
            JumpTo(Fn(), reinterpret_cast<void*>(&fake_other));
        }
        Sleep(21000);
        FILE* f = _wfopen((modDir + L"shroudlink-first.log").c_str(), L"rb");
        char buf[2048] = {};
        if (f) {
            fread(buf, 1, sizeof buf - 1, f);
            fclose(f);
        }
        printf("OWNLOG:\n%s", buf);
    }
    return 0;
}
