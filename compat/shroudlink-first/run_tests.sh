#!/bin/bash
# Builds the harness and runs every scenario under Wine.
set -e
cd "$(dirname "$0")"
./build_win.sh
T=build/test
rm -rf $T && mkdir -p $T/greeneye/0-shroudlink-first
python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++20 -O1 -municode -o $T/harness.exe tests/harness.cpp
python3 -m ziglang c++ -target x86_64-windows-gnu -O1 -shared -o $T/dinput8.dll tests/fake_dinput8.cpp
cp build/shroudlink-first.dll $T/greeneye/0-shroudlink-first/
echo core > $T/shroudlink_core.dll
export WINEDEBUG=-all WINEDLLOVERRIDES="dinput8=n"
for mode in normal chain disabled child loaderfirst broken blind nolog logfirst nodinput; do
  echo "=== $mode"
  timeout 60 "${WINE:-wine64}" $T/harness.exe $mode 2>&1 | grep -v "^$" | tr -d '\r'
done

# ShroudForge: the game folder's dinput8.dll is not the addon; the addon is a native DLL mod
mkdir -p $T/shroudforge $T/mods/shroudlink $T/mods/other
echo runtime > $T/shroudforge/shroudforge-runtime.dll
echo "=== shroudforge without the addon"
timeout 60 "${WINE:-wine64}" $T/harness.exe normal 2>&1 | grep -v "^$" | tr -d '\r'
python3 -m ziglang c++ -target x86_64-windows-gnu -O1 -shared -o $T/mods/shroudlink/addon.dll tests/fake_sf_addon.cpp
printf '[Plugin]\r\nEnabled=1\r\nDll=missing.dll\r\n' > $T/mods/other/native-plugin.ini
printf '; comment\r\n[Plugin]\r\nEnabled=0\r\nDll=addon.dll\r\n' > $T/mods/shroudlink/native-plugin.ini
echo "=== shroudforge, the addon switched off"
timeout 60 "${WINE:-wine64}" $T/harness.exe normal 2>&1 | grep -v "^$" | tr -d '\r'
printf '; comment\r\n[Plugin]\r\nEnabled=1\r\nDll=addon.dll\r\n' > $T/mods/shroudlink/native-plugin.ini
echo "=== shroudforge with the addon"
timeout 60 "${WINE:-wine64}" $T/harness.exe normal 2>&1 | grep -v "^$" | tr -d '\r'
