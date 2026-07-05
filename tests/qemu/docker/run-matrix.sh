#!/usr/bin/env bash
# Docker entrypoint (macOS-friendly wrapper): compile the ci tool, stage the
# bind-mounted repo into an internal tree (so no Linux build artifacts leak back
# into a read-only / macOS host mount), wire the shared /vendor (inputs) and
# /dist (outputs) mounts into it, then hand off to `ci qemu`, which owns the
# configure/build/fixup/test steps. Keeping that logic in tools/ci.c lets CI run
# the same matrix natively on a Linux runner without Docker (see
# .github/workflows/ci.yml). Mirrors tests/ci/docker/run-ci.sh.
set -euo pipefail

SRC_MOUNT=/work
SRC=/src

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work ...)" >&2
    exit 2
fi

CC="${CC:-cc}"
"$CC" -O2 -o /tmp/mcc-ci \
    "$SRC_MOUNT/tools/ci.c" "$SRC_MOUNT/tools/toolsupport.c" -ldl
/tmp/mcc-ci stage "$SRC_MOUNT" "$SRC"

# Download-once, share-everywhere: point the staged tree's vendor/ (inputs) and
# dist/ (outputs) at their bind mounts, so fetched toolchains and produced
# artifacts are shared with the host and every container rather than staged.
for shared in vendor dist; do
    if [ -d "/$shared" ]; then
        ln -sfn "/$shared" "$SRC/$shared"
        echo "==> sharing /$shared mount into the staged tree"
    fi
done

cd "$SRC"
# Gentoo stage3 rootfs trees are vendored under vendor/ (-> the /vendor mount),
# so they are downloaded once and shared with the host and every container.
export MCC_QEMU_DLDIR="$SRC/vendor"
exec /tmp/mcc-ci qemu "$@"
