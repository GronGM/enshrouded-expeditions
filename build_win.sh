#!/bin/bash
# Windows x64 DLLs (MinGW ABI, C interfaces only):
#   build/expeditions.dll      the shroudlink mod
#   build/emf/expeditions.dll  the EMF mod
set -e
cd "$(dirname "$0")"
mkdir -p build/emf
COMMON="src/expeditions.cpp src/json.cpp src/lang.cpp src/page_text.cpp src/platform.cpp src/regions.cpp src/supplies.cpp"
FLAGS="-target x86_64-windows-gnu -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -shared -s"
python3 -m ziglang c++ $FLAGS -o build/expeditions.dll src/dll.cpp $COMMON
python3 -m ziglang c++ $FLAGS -o build/emf/expeditions.dll src/emf_main.cpp src/emf_host.cpp src/emf_ui.cpp src/item_names.cpp $COMMON
