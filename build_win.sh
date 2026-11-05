#!/bin/bash
# expeditions.dll for Windows x64 (MinGW ABI, C interface to the shroudlink core)
set -e
cd "$(dirname "$0")"
mkdir -p build
python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter \
  -shared -o build/expeditions.dll src/dll.cpp src/expeditions.cpp src/json.cpp src/lang.cpp src/page_text.cpp src/platform.cpp src/regions.cpp src/supplies.cpp -s
