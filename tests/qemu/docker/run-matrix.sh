#!/usr/bin/env bash
set -euo pipefail

SRC_MOUNT=/work
SRC=/src
# Gentoo stage3 rootfs trees are vendored: they land under vendor/, which the
# launcher bind-mounts at /vendor (staged tree's vendor/ -> /vendor below), so
# they are downloaded once and shared with the host and every container.
DLDIR="$SRC/vendor"

PRESET="${PRESET:-qemu}"
JOBS="${JOBS:-$(nproc)}"
BUILD="$SRC/cmake-$PRESET"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work ...)" >&2
    exit 2
fi

echo "==> staging source $SRC_MOUNT -> $SRC"
mkdir -p "$SRC"
rsync -a --delete \
    --exclude 'cmake-*' \
    --exclude 'cmake-windows-*' \
    --exclude 'vendor' \
    --exclude 'build-*' \
    --exclude '.git' \
    "$SRC_MOUNT"/ "$SRC"/

# Download-once, share-everywhere: if the launcher bind-mounted the host's
# vendor tree at /vendor, point the staged source's vendor/ at it so fetched
# toolchains (clang/mingw/musl) are reused across containers and persist back
# to the host. Absent the mount, the build falls back to an ephemeral vendor/.
if [ -d /vendor ]; then
    ln -sfn /vendor "$SRC/vendor"
    echo "==> sharing vendor toolchains from the /vendor mount"
fi

cd "$SRC"

overrides=( -DMCC_QEMU_DLDIR="$DLDIR" )
[ -n "${ARCHS:-}" ] && overrides+=( -DMCC_QEMU_ARCHS="$ARCHS" )
[ -n "${LIBCS:-}" ] && overrides+=( -DMCC_QEMU_LIBCS="$LIBCS" )

echo "==> configuring (preset=$PRESET)"
cmake --preset "$PRESET" "${overrides[@]}"

echo "==> building mcc + cross compilers (-j$JOBS)"
cmake --build --preset "$PRESET" -j"$JOBS"

fixup_multilib() {
    for root in "$DLDIR"/gentoo-stage3-x86_64-*; do
        [ -d "$root" ] || continue
        local lib="$root/usr/lib" lib64="$root/usr/lib64"
        [ -f "$lib64/crt1.o" ] || continue
        for o in crt1.o crti.o crtn.o Scrt1.o gcrt1.o Mcrt1.o; do
            [ -f "$lib64/$o" ] && cp -f "$lib64/$o" "$lib/$o"
        done
        echo "==> fixed multilib crt in $(basename "$root")"
    done
}

echo "==> pre-fetching x86_64 sysroots for multilib fixup (if applicable)"
ctest --test-dir "$BUILD" -R 'qemu-x86_64-.*-fetch' --output-on-failure || true
fixup_multilib

echo "==> running qemu matrix (preset=$PRESET)"
ctest --preset "$PRESET" "$@"
