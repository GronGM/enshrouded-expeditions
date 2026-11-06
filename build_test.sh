#!/bin/bash
set -e
cd "$(dirname "$0")"
mkdir -p build
COMMON="src/expeditions.cpp src/json.cpp src/lang.cpp src/page_text.cpp src/platform.cpp src/regions.cpp src/supplies.cpp"
g++ -std=c++20 -O1 -g -Wall -Wextra -fsanitize=address,undefined -o build/tests tests/test_main.cpp $COMMON
./build/tests
# the EMF build against a fake EMF core
g++ -std=c++20 -O1 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -o build/tests_emf tests/test_emf.cpp \
  src/emf_host.cpp src/emf_ui.cpp src/item_names.cpp $COMMON
EXPED_LANG=en ./build/tests_emf
