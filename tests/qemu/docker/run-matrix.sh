#!/usr/bin/env bash
#
# qemu-user cross matrix: stage the mounted repo, then configure/build/test via
# a named CMake preset (qemu, or a per-arch qemu-<arch>; see CMakePresets.json).
# This script owns only the container-side staging, the persistent rootfs cache
# wiring, and the x86_64 multilib crt fixup.
#
#   docker run --rm -e PRESET=qemu-arm64 \
#       -v "$PWD:/work" -v mcc-qemu-roots:/qemu-roots mcc-qemu
#
# Local subsetting without a per-arch preset: -e ARCHS=arm64 -e LIBCS=glibc.
#
set -euo pipefail

SRC_MOUNT=/work
SRC=/src
ROOTS=/qemu-roots

PRESET="${PRESET:-qemu}"
JOBS="${JOBS:-$(nproc)}"
BUILD="$SRC/cmake-build-$PRESET"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work ...)" >&2
    exit 2
fi

echo "==> staging source $SRC_MOUNT -> $SRC"
mkdir -p "$SRC" "$ROOTS"
rsync -a --delete \
    --exclude 'cmake-build*' \
    --exclude 'cmake-windows-*' \
    --exclude 'cmake-mingw-*' \
    --exclude 'cmake-clang' \
    --exclude 'build-*' \
    --exclude '.git' \
    "$SRC_MOUNT"/ "$SRC"/

cd "$SRC"

# The rootfs cache dir is a container-mounted volume, so it can't live in the
# preset; ARCHS/LIBCS are optional local overrides on top of the preset default.
overrides=( -DMCC_QEMU_DLDIR="$ROOTS" )
[ -n "${ARCHS:-}" ] && overrides+=( -DMCC_QEMU_ARCHS="$ARCHS" )
[ -n "${LIBCS:-}" ] && overrides+=( -DMCC_QEMU_LIBCS="$LIBCS" )

echo "==> configuring (preset=$PRESET)"
cmake --preset "$PRESET" "${overrides[@]}"

echo "==> building mcc + cross compilers (-j$JOBS)"
cmake --build --preset "$PRESET" -j"$JOBS"

# glibc x86_64 multilib rootfs ships crt objects under lib64/; mcc looks in lib/.
fixup_multilib() {
    for root in "$ROOTS"/x86_64-*; do
        [ -d "$root" ] || continue
        local lib="$root/usr/lib" lib64="$root/usr/lib64"
        [ -f "$lib64/crt1.o" ] || continue
        for o in crt1.o crti.o crtn.o Scrt1.o gcrt1.o Mcrt1.o; do
            [ -f "$lib64/$o" ] && cp -f "$lib64/$o" "$lib/$o"
        done
        echo "==> fixed multilib crt in $(basename "$root")"
    done
}

# Fetch x86_64 sysroots first (if this matrix includes them) so the fixup runs
# before the real conformance tests; no-op when no x86_64 roots are produced.
echo "==> pre-fetching x86_64 sysroots for multilib fixup (if applicable)"
ctest --test-dir "$BUILD" -R 'qemu-x86_64-.*-fetch' --output-on-failure || true
fixup_multilib

echo "==> running qemu matrix (preset=$PRESET)"
ctest --preset "$PRESET" "$@"
