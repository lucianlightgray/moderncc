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

# Download-once, share-everywhere: reuse the host's vendor toolchains when the
# launcher bind-mounted them at /vendor (staging excludes vendor/ by design).
if [ -d /vendor ]; then
    ln -sfn /vendor "$SRC/vendor"
    echo "==> sharing vendor toolchains from the /vendor mount"
fi

cd "$SRC"

# Install/stage into the shared /dist mount (per-artifact subdir when ART is set),
# so build outputs land in the host's dist/ tree — the output counterpart of the
# /vendor input mount.
DEST="/dist${ART:+/$ART}"
if [ -d /dist ] && [ -w /dist ]; then
    mkdir -p "$DEST"
    exec /tmp/mcc-ci run-preset "$PRESET" --out "$DEST" -- "$@"
else
    exec /tmp/mcc-ci run-preset "$PRESET" -- "$@"
fi
