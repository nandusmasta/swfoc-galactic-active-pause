#!/bin/sh
# Builds build/winmm.dll with MinGW-w64 (x86_64). Regenerate the proxy stubs with
# tools/gen_proxy.py if needed.
set -e
cd "$(dirname "$0")"
mkdir -p build
gcc -O2 -s -shared -Wall -Wextra \
    -o build/winmm.dll \
    src/spacebar_pause.c src/winmm_proxy.c src/winmm.def \
    -static-libgcc -Wl,--kill-at
echo "built build/winmm.dll"
