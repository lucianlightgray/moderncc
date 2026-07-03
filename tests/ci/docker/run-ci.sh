#!/usr/bin/env bash
#
# CI runner: stage the mounted repo, then configure/build/test via a named
# CMake preset (see CMakePresets.json). The workflow selects the scenario by
# setting PRESET; this script only bootstraps the in-tree CI tool.
#
#   docker run --rm -e PRESET=linux-gcc -v "$PWD:/work:ro" mcc-ci
#
# The staging (shared exclusion list + CRLF->LF fixup) and the
# configure/build/test/install sequence now live in tools/ci.c (PLAN 0.9),
# so this script only bootstrap-compiles that tool with the host cc (no cmake
# has run yet, so no build target exists) and hands off to it.
#
# Optionally mount a writable dir at /out to export the build targets: the
# tree is configured with CMAKE_INSTALL_PREFIX=/out and `cmake --install` runs
# after the tests. The prefix must be set at configure time — the mcc runtime
# dir install destination is an absolute path baked into the install rules.
#
set -euo pipefail

SRC_MOUNT=/work
SRC=/src

PRESET="${PRESET:?set PRESET to a CMake preset name (e.g. linux-gcc); see CMakePresets.json}"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work:ro ...)" >&2
    exit 2
fi

# Bootstrap the in-tree CI tool from the mounted sources (no cmake yet).
CC="${CC:-cc}"
"$CC" -O2 -o /tmp/mcc-ci \
    "$SRC_MOUNT/tools/ci.c" "$SRC_MOUNT/tools/toolsupport.c" -ldl

# Stage /work -> /src (exclusions + line-ending normalization) then run the
# preset. tools/ci owns the parallelism probe and the whole sequence.
/tmp/mcc-ci stage "$SRC_MOUNT" "$SRC"
cd "$SRC"

if [ -d /out ] && [ -w /out ]; then
    exec /tmp/mcc-ci run-preset "$PRESET" --out /out -- "$@"
else
    exec /tmp/mcc-ci run-preset "$PRESET" -- "$@"
fi
