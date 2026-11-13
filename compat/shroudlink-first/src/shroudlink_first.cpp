// shroudlink first: a Greeneye Loader mod that holds the loader's other mods back until the
// shroudlink addon (dinput8.dll, or a ShroudForge native DLL mod) has hooked the game's
// play-state UI function.
//
// Why: the addon hooks doPlayStateUi from its own thread at startup (retour: a 5 byte jump,
// its trampoline runs the first 11 bytes and returns to +11). The Worst Map Mod hooks the same
// function through the loader, at the same moment, from the game's main thread (18 bytes). When
// the loader writes first and the addon's jump lands on top of it, the addon's trampoline
// returns into the middle of the loader's jump: "sub rsp, 310h" is skipped and the game crashes
// on its first frame in the world. When the addon's jump is there first, the loader chains onto
// it (only the jump's target changes) and both mods work. Loader mods start one after another
// in folder order, so this one (folder "0-...") runs first and waits for the addon's jump.
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {

// Greeneye Loader API 1 (the fields this mod reads; see the loader's greeneye.h).
struct GreeneyeHost {
    uint32_t size;
    uint32_t apiVersion;
    int side;
    const wchar_t* modDir;
    const wchar_t* gameDir;
    void (*log)(const char* line);
};

constexpr DWORD kWaitMs = 30000;      // the addon normally needs well under 2 s
constexpr DWORD kWaitBlindMs = 5000;  // when the function is not found (other game build)
constexpr DWORD kSettleMs = 150;      // the jump must stay the same this long
constexpr DWORD kCheckAfterMs = 20000;

// doPlayStateUi starts with mov r11, rsp; push rbp; lea rbp, [r11-218h]; sub rsp, 310h (18 bytes).
constexpr uint8_t kPrologue[] = {0x4C, 0x8B, 0xDC, 0x55, 0x49, 0x8D, 0xAB, 0xE8, 0xFD,
                                 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x10, 0x03, 0x00, 0x00};
constexpr size_t kAddonJump = 5; // retour's jmp rel32
// What follows the prologue (-1: any byte). No hook of up to 18 bytes touches it, so the
// function is found whoever hooked it (the Worst Map Mod finds it the same way).
constexpr int kBody[] = {0x48, 0x8B, 0x05, -1,   -1,   -1,   -1,   0x48, 0x8B, 0x88, 0xD0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x01,
                         0x48, 0x85, 0xC0, 0x0F, 0x84, -1,   -1,   -1,   -1,   0x4D, 0x89, 0x73, 0xC8, 0x4C, 0x8B, 0x70, 0x38};

const GreeneyeHost* g_host = nullptr;

// dir + name into out (MAX_PATH)
void PathIn(wchar_t* out, const wchar_t* dir, const wchar_t* name)
{
    out[0] = 0;
    if (wcslen(dir) + wcslen(name) >= MAX_PATH) return;
    wcscpy(out, dir);
    wcscat(out, name);
}
wchar_t g_ownLog[MAX_PATH];
uint8_t* g_fn = nullptr;
uint8_t* g_hookTarget = nullptr; // where the addon's jump led when we let the others start

void Log(const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    if (g_host && g_host->log) {
        char prefixed[560];
        snprintf(prefixed, sizeof prefixed, "shroudlink first: %s", line);
        g_host->log(prefixed);
    }
}

// Our own file for what happens after the loader moved on (the check thread).
void LogFile(const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    if (n < 0) return;
    if (n > static_cast<int>(sizeof line) - 3) n = sizeof line - 3;
    line[n++] = '\r';
    line[n++] = '\n';
    HANDLE f = CreateFileW(g_ownLog, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[48];
    int s = snprintf(stamp, sizeof stamp, "[%04u-%02u-%02u %02u:%02u:%02u.%03u] ", t.wYear, t.wMonth, t.wDay, t.wHour,
                     t.wMinute, t.wSecond, t.wMilliseconds);
    DWORD written;
    WriteFile(f, stamp, s, &written, nullptr);
    WriteFile(f, line, n, &written, nullptr);
    CloseHandle(f);
}

bool FileExists(const wchar_t* path)
{
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// The game's code section.
bool GameText(uint8_t*& text, size_t& size)
{
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    if (!base) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::memcmp(sec->Name, ".text", 5) == 0) {
            text = base + sec->VirtualAddress;
            size = sec->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

enum class Start { Unknown, Original, AddonJump, OtherHook, Broken };

Start StartOf(const uint8_t* fn)
{
    if (std::memcmp(fn, kPrologue, sizeof kPrologue) == 0) return Start::Original;
    const bool restIntact = std::memcmp(fn + kAddonJump, kPrologue + kAddonJump, sizeof kPrologue - kAddonJump) == 0;
    if (fn[0] == 0xE9) return restIntact ? Start::AddonJump : Start::Broken;
    return Start::OtherHook; // "mov rax, imm64; jmp rax" (Greeneye) or another hook
}

const char* Name(Start s)
{
    switch (s) {
    case Start::Original: return "the game's own code";
    case Start::AddonJump: return "shroudlink's jump";
    case Start::OtherHook: return "another mod's hook";
    case Start::Broken: return "a jump over another mod's hook (the crash case)";
    default: return "?";
    }
}

// doPlayStateUi, by the code right after its prologue.
uint8_t* FindPlayStateUi()
{
    uint8_t* text = nullptr;
    size_t size = 0;
    constexpr size_t n = sizeof kBody / sizeof kBody[0];
    if (!GameText(text, size) || size < sizeof kPrologue + n) return nullptr;
    uint8_t* found = nullptr;
    for (uint8_t* p = text + sizeof kPrologue; p + n <= text + size; ++p) {
        if (p[0] != kBody[0]) continue;
        size_t k = 1;
        while (k < n && (kBody[k] < 0 || p[k] == kBody[k])) ++k;
        if (k != n) continue;
        if (found) return nullptr; // not unique: not the build this was made for
        found = p - sizeof kPrologue;
    }
    return found;
}

uint8_t* JumpTarget(const uint8_t* fn)
{
    int32_t d;
    std::memcpy(&d, fn + 1, 4);
    return const_cast<uint8_t*>(fn) + 5 + d;
}

// The addon's log: what it says in this session (lines stamped from the process start on).
struct AddonLog {
    wchar_t path[MAX_PATH];
    uint64_t since = 0; // unix seconds
    LONGLONG offset = -1;
    char partial[1024];
    size_t partialLen = 0;

    // 1: done (the addon finished or gave up), 0: nothing yet
    int Poll(char* what, size_t whatSize)
    {
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return 0;
        LARGE_INTEGER size{};
        GetFileSizeEx(f, &size);
        if (offset < 0 || offset > size.QuadPart) offset = size.QuadPart > 65536 ? size.QuadPart - 65536 : 0;
        int result = 0;
        while (offset < size.QuadPart && !result) {
            char buf[4096];
            LARGE_INTEGER at;
            at.QuadPart = offset;
            SetFilePointerEx(f, at, nullptr, FILE_BEGIN);
            DWORD got = 0;
            if (!ReadFile(f, buf, sizeof buf, &got, nullptr) || got == 0) break;
            offset += got;
            for (DWORD i = 0; i < got && !result; ++i) {
                if (buf[i] != '\n') {
                    if (partialLen < sizeof partial - 1) partial[partialLen++] = buf[i];
                    continue;
                }
                partial[partialLen] = 0;
                result = Line(partial, what, whatSize);
                partialLen = 0;
            }
        }
        CloseHandle(f);
        return result;
    }

    int Line(const char* line, char* what, size_t whatSize)
    {
        if (line[0] != '[') return 0;
        uint64_t stamp = 0;
        const char* p = line + 1;
        while (*p >= '0' && *p <= '9') stamp = stamp * 10 + (*p++ - '0');
        if (*p != ']' || stamp < since) return 0;
        static const char* const kDone[] = {"native UI ready", "native UI disabled", "addon disabled",
                                            "not starting again", "could not inspect"};
        for (const char* d : kDone) {
            if (std::strstr(line, d)) {
                snprintf(what, whatSize, "%s", p + (p[1] == ' ' ? 2 : 1));
                size_t n = std::strlen(what);
                while (n && (what[n - 1] == '\r' || what[n - 1] == ' ')) what[--n] = 0;
                return 1;
            }
        }
        return 0;
    }
};

uint64_t ProcessStartUnix()
{
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0;
    ULARGE_INTEGER t;
    t.LowPart = created.dwLowDateTime;
    t.HighPart = created.dwHighDateTime;
    return (t.QuadPart - 116444736000000000ULL) / 10000000ULL;
}

// Is this thread inside the Windows loader (a DllMain)? Then no new thread can start before we
// return, the addon's included: waiting for its hook is pointless. The Greeneye Loader starts its
// mods there when it is loaded late, as with ShroudForge's dbghelp.dll proxy, which loads the
// system dbghelp.dll (and with it dbgcore.dll) from its own DllMain.
bool InsideLoaderLock()
{
#if defined(_M_X64) || defined(__x86_64__)
    const auto* peb = reinterpret_cast<const uint8_t*>(__readgsqword(0x60));
    const auto* lock = *reinterpret_cast<RTL_CRITICAL_SECTION* const*>(peb + 0x110);
    return lock && reinterpret_cast<uintptr_t>(lock->OwningThread) == GetCurrentThreadId();
#else
    return false;
#endif
}

// Does this file hold the addon? (it exports shroudlink_shutdown; the name is in the file as text)
bool IsAddonFile(const wchar_t* path)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    static const char kName[] = "shroudlink_shutdown";
    const size_t nameLen = sizeof kName - 1;
    bool found = false;
    LARGE_INTEGER size;
    if (GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < 64ll * 1024 * 1024) {
        if (char* data = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(size.QuadPart)))) {
            DWORD got = 0;
            if (ReadFile(f, data, static_cast<DWORD>(size.QuadPart), &got, nullptr) && got >= nameLen) {
                for (size_t i = 0; i + nameLen <= got && !found; ++i)
                    found = data[i] == kName[0] && std::memcmp(data + i, kName, nameLen) == 0;
            }
            HeapFree(GetProcessHeap(), 0, data);
        }
    }
    CloseHandle(f);
    return found;
}

// The addon the usual way: it is the dinput8.dll of the game folder, next to its core.
bool AddonPresent(const wchar_t* gameDir)
{
    HMODULE m = GetModuleHandleW(L"dinput8.dll");
    if (!m) return false;
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return false;
    size_t dirLen = wcslen(gameDir);
    if (_wcsnicmp(path, gameDir, dirLen) != 0 || wcschr(path + dirLen, L'\\')) return false;
    wchar_t core[MAX_PATH];
    if (!GetProcAddress(m, "shroudlink_shutdown")) {
        // not the addon itself: a loader that chains it (EMF) counts, ShroudForge's does not,
        // it starts the addon much later (see AddonUnderShroudForge)
        PathIn(core, gameDir, L"shroudforge\\shroudforge-runtime.dll");
        if (core[0] && FileExists(core)) return false;
    }
    PathIn(core, gameDir, L"shroudlink_core.dll");
    return FileExists(core);
}

// "Dll=" of the [Plugin] section of a ShroudForge native-plugin.ini, when it is not switched off.
bool PluginDll(const wchar_t* ini, wchar_t* out, size_t outLen)
{
    wchar_t enabled[16] = L"";
    GetPrivateProfileStringW(L"Plugin", L"Enabled", L"1", enabled, 16, ini);
    if (!_wcsicmp(enabled, L"0") || !_wcsicmp(enabled, L"false") || !_wcsicmp(enabled, L"no") || !_wcsicmp(enabled, L"off"))
        return false;
    DWORD n = GetPrivateProfileStringW(L"Plugin", L"Dll", L"", out, static_cast<DWORD>(outLen), ini);
    if (!n || n >= outLen - 1) return false;
    for (wchar_t* c = out; *c; ++c) {
        if (*c == L'/') *c = L'\\';
    }
    return !wcsstr(out, L"..") && !wcschr(out, L':') && out[0] != L'\\';
}

// With ShroudForge the game folder's dinput8.dll is ShroudForge's, and the addon is one of its
// native DLL mods (mods\<folder>\native-plugin.ini). ShroudForge starts those only after this
// loader has started its mods, so the addon is found here and started first; ShroudForge then
// finds it already loaded. Returns its path in out, or false.
bool AddonUnderShroudForge(const wchar_t* gameDir, wchar_t* out)
{
    wchar_t path[MAX_PATH];
    PathIn(path, gameDir, L"shroudforge\\shroudforge-runtime.dll");
    if (!path[0] || !FileExists(path)) return false;
    PathIn(path, gameDir, L"shroudlink_core.dll");
    if (!path[0] || !FileExists(path)) return false;
    wchar_t pattern[MAX_PATH];
    PathIn(pattern, gameDir, L"mods\\*");
    if (!pattern[0]) return false;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
        wchar_t ini[MAX_PATH], dll[MAX_PATH], full[MAX_PATH];
        if (swprintf(ini, MAX_PATH, L"%lsmods\\%ls\\native-plugin.ini", gameDir, fd.cFileName) < 0) continue;
        if (!FileExists(ini) || !PluginDll(ini, dll, MAX_PATH)) continue;
        if (swprintf(full, MAX_PATH, L"%lsmods\\%ls\\%ls", gameDir, fd.cFileName, dll) < 0) continue;
        if (!FileExists(full) || !IsAddonFile(full)) continue;
        wcscpy(out, full);
        found = true;
    } while (!found && FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

// Later, once every mod has started: is the play-state UI hook chain sound?
DWORD WINAPI CheckLater(void*)
{
    Sleep(kCheckAfterMs);
    if (!g_fn) return 0;
    const Start start = StartOf(g_fn);
    if (start == Start::AddonJump) {
        uint8_t* target = JumpTarget(g_fn);
        if (target == g_hookTarget)
            LogFile("check: the play-state UI hook chain is sound (shroudlink's jump, no other mod hooked it)");
        else
            LogFile("check: the play-state UI hook chain is sound (shroudlink's jump now leads to %p: the next mod chained onto it)",
                    target);
    } else if (start == Start::Original) {
        LogFile("check: the play-state UI function is not hooked at all");
    } else {
        LogFile("check: WARNING: the play-state UI function starts with %s; the game may crash in the world", Name(start));
    }
    return 0;
}

} // namespace

extern "C" __declspec(dllexport) bool GreeneyeModStart(const GreeneyeHost* host)
{
    g_host = host;
    if (!host || !host->gameDir || !host->modDir) return false;
    PathIn(g_ownLog, host->modDir, L"shroudlink-first.log");
    DeleteFileW(g_ownLog);

    if (!AddonPresent(host->gameDir)) {
        wchar_t addon[MAX_PATH];
        if (!AddonUnderShroudForge(host->gameDir, addon)) {
            Log("shroudlink is not installed in this game folder; nothing to wait for");
            LogFile("shroudlink is not installed in this game folder; nothing to wait for");
            return true;
        }
        // LoadLibrary: the addon starts its own thread; a later load by ShroudForge finds it loaded
        HMODULE m = LoadLibraryExW(addon, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!m) {
            Log("shroudlink is a ShroudForge mod here (%ls) but did not load (error %lu); not waiting", addon, GetLastError());
            LogFile("shroudlink is a ShroudForge mod here (%ls) but did not load (error %lu); not waiting", addon, GetLastError());
            return true;
        }
        Log("shroudlink is a ShroudForge mod here: started it first (%ls)", addon);
        LogFile("shroudlink is a ShroudForge mod here: started it first (%ls)", addon);
    }

    if (InsideLoaderLock()) {
        g_fn = FindPlayStateUi();
        Log("the loader started its mods inside the Windows loader (a late load): shroudlink cannot start "
            "before the next mods, so not waiting. With ShroudForge, move its dbghelp.dll out of the game folder "
            "(its dinput8.dll starts it too); the loader then starts its mods at the game's entry point");
        LogFile("not waiting: started inside the Windows loader (late load, e.g. ShroudForge's dbghelp.dll); "
                "move ShroudForge's dbghelp.dll out of the game folder");
        if (g_fn) {
            if (HANDLE t = CreateThread(nullptr, 0, CheckLater, nullptr, 0, nullptr)) CloseHandle(t);
        }
        return true;
    }

    const ULONGLONG t0 = GetTickCount64();
    g_fn = FindPlayStateUi();
    AddonLog addonLog;
    PathIn(addonLog.path, host->gameDir, L"shroudlink_client.log");
    addonLog.since = ProcessStartUnix();

    if (g_fn && StartOf(g_fn) != Start::Original && StartOf(g_fn) != Start::AddonJump) {
        Log("the play-state UI already starts with %s: another mod started before this one (keep this mod's folder name first)",
            Name(StartOf(g_fn)));
        LogFile("too late: the play-state UI already starts with %s", Name(StartOf(g_fn)));
        return true;
    }
    const DWORD limit = g_fn ? kWaitMs : kWaitBlindMs;
    char what[256] = "";
    ULONGLONG jumpSince = 0;
    uint8_t jump[5] = {};
    const char* outcome = "timed out";
    for (;;) {
        const ULONGLONG now = GetTickCount64();
        if (g_fn) {
            Start s = StartOf(g_fn);
            if (s == Start::AddonJump) {
                if (!jumpSince || std::memcmp(jump, g_fn, 5) != 0) {
                    std::memcpy(jump, g_fn, 5);
                    jumpSince = now;
                } else if (now - jumpSince >= kSettleMs) {
                    outcome = "shroudlink's jump is in";
                    break;
                }
            } else if (s == Start::OtherHook || s == Start::Broken) {
                outcome = "another hook appeared there instead";
                break;
            } else {
                jumpSince = 0;
            }
        }
        if (addonLog.Poll(what, sizeof what)) {
            // the addon is done (its hooks are all in) or gave up; let its jump settle
            if (g_fn && StartOf(g_fn) == Start::AddonJump && !jumpSince) jumpSince = now;
            if (!g_fn || StartOf(g_fn) != Start::AddonJump || now - jumpSince >= kSettleMs) {
                outcome = "shroudlink says";
                break;
            }
        }
        if (now - t0 >= limit) break;
        Sleep(10);
    }
    const unsigned waited = static_cast<unsigned>(GetTickCount64() - t0);
    const Start final = g_fn ? StartOf(g_fn) : Start::Unknown;
    if (final == Start::AddonJump) g_hookTarget = JumpTarget(g_fn);
    Log("waited %u ms: %s%s%s; play-state UI at %p starts with %s", waited, outcome, what[0] ? ": " : "", what, g_fn,
        g_fn ? Name(final) : "(not found)");
    LogFile("waited %u ms: %s%s%s; play-state UI at %p starts with %s", waited, outcome, what[0] ? ": " : "", what,
            g_fn, g_fn ? Name(final) : "(not found)");
    if (g_fn) {
        if (HANDLE t = CreateThread(nullptr, 0, CheckLater, nullptr, 0, nullptr)) CloseHandle(t);
    }
    return true;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}
