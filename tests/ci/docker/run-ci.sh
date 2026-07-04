#!/usr/bin/env bash
set -euo pipefail

SRC_MOUNT=/work
SRC=/src

PRESET="${PRESET:?set PRESET to a CMake preset name (e.g. linux-gcc); see CMakePresets.json}"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work:ro ...)" >&2
    exit 2
fi

CC="${CC:-cc}"
"$CC" -O2 -o /tmp/mcc-ci \
    "$SRC_MOUNT/tools/ci.c" "$SRC_MOUNT/tools/toolsupport.c" -ldl

/tmp/mcc-ci stage "$SRC_MOUNT" "$SRC"
cd "$SRC"

if [ -d /out ] && [ -w /out ]; then
    exec /tmp/mcc-ci run-preset "$PRESET" --out /out -- "$@"
else
    exec /tmp/mcc-ci run-preset "$PRESET" -- "$@"
fi
