// Files and clocks, Windows in the game, POSIX for the tests.
#pragma once
#include <cstdint>
#include <string>

namespace platform {

// The game's folder (next to enshrouded.exe / enshrouded_server.exe), with a trailing separator.
std::string game_dir();

bool read_file(const std::string& path, std::string& out);
// Writes `path.new`, then replaces `path` with it, so a crash never leaves half a file.
bool write_file_atomic(const std::string& path, const std::string& data);
bool file_exists(const std::string& path);

// True when Windows shows a Russian-reading language (Russian, Ukrainian, Belarusian,
// Kazakh); EXPED_LANG=ru/en overrides (tests, other systems).
bool system_reads_russian();

// A variable of this process's environment (as set from any module in it); empty: not set.
std::string env(const char* name);

int64_t unix_now();
// Seconds on a monotonic clock.
double steady_seconds();

}  // namespace platform
