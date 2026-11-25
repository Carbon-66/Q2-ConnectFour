#!/bin/sh
# host build of the whole game (x86-64 Linux), for the tests
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
python3 tools/font.py build/font_data.c
python3 tools/logo.py build/logo_data.c
clang -O2 -g -fno-math-errno -Wall -Wextra -Wno-unused-parameter -Werror -Isrc src/*.c build/font_data.c build/logo_data.c -o build/host/q2-connect4
clang -shared -fPIC -O1 -w test/fbshim.c -o build/host/fbshim.so -ldl
clang -O2 -Wall -Wextra -Werror -Isrc test/logic_test.c src/logic.c -o build/host/logic_test
