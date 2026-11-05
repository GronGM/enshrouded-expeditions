#!/bin/bash
set -e
cd "$(dirname "$0")"
mkdir -p build
g++ -std=c++20 -O1 -g -Wall -Wextra -fsanitize=address,undefined -o build/tests tests/test_main.cpp src/expeditions.cpp src/json.cpp src/lang.cpp src/page_text.cpp src/platform.cpp src/regions.cpp src/supplies.cpp
./build/tests
