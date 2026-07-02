#!/usr/bin/env bash
#
# CI runner: stage the mounted repo, then configure/build/test via a named
# CMake preset (see CMakePresets.json). The workflow selects the scenario by
# setting PRESET; this script owns only the container-side staging + EOL fixup.
#
#   docker run --rm -e PRESET=linux-gcc -v "$PWD:/work:ro" mcc-ci
#
set -euo pipefail

SRC_MOUNT=/work
SRC=/src

PRESET="${PRESET:?set PRESET to a CMake preset name (e.g. linux-gcc); see CMakePresets.json}"
JOBS="${JOBS:-$(nproc)}"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work:ro ...)" >&2
    exit 2
fi

echo "==> staging $SRC_MOUNT -> $SRC"
mkdir -p "$SRC"
rsync -a --delete \
    --exclude 'cmake-build*' \
    --exclude 'cmake-windows-*' \
    --exclude 'cmake-mingw-*' \
    --exclude 'cmake-clang' \
    --exclude 'build-*' \
    --exclude '.git' \
    "$SRC_MOUNT"/ "$SRC"/

# A Windows (autocrlf) checkout otherwise feeds CRLF sources that break
# LF-expecting tests; normalize on the staged copy.
if [ "${NORMALIZE_EOL:-1}" = 1 ]; then
    echo "==> normalizing line endings (CRLF -> LF) in staged sources"
    find "$SRC" -type f \
        \( -name '*.c' -o -name '*.h' -o -name '*.cmake' -o -name '*.txt' \
           -o -name '*.S' -o -name '*.def' \) \
        -exec sed -i 's/\r$//' {} +
fi

cd "$SRC"
echo "==> configuring (preset=$PRESET)"
cmake --preset "$PRESET"

echo "==> building (-j$JOBS)"
cmake --build --preset "$PRESET" -j"$JOBS"

echo "==> testing (preset=$PRESET)"
ctest --preset "$PRESET" -j"$JOBS" "$@"
