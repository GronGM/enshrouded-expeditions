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
