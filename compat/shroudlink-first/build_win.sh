#!/bin/bash
# shroudlink-first.dll (Greeneye Loader mod) for Windows x64
set -e
cd "$(dirname "$0")"
mkdir -p build
python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++20 -O2 -Wall -Wextra \
  -shared -o build/shroudlink-first.dll src/shroudlink_first.cpp -s
