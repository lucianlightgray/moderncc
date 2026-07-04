#!/usr/bin/env bash
set -euo pipefail

SRC_MOUNT=/work
SRC=/src
ROOTS=/qemu-roots

PRESET="${PRESET:-qemu}"
JOBS="${JOBS:-$(nproc)}"
BUILD="$SRC/cmake-$PRESET"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work ...)" >&2
    exit 2
fi

echo "==> staging source $SRC_MOUNT -> $SRC"
mkdir -p "$SRC" "$ROOTS"
rsync -a --delete \
    --exclude 'cmake-*' \
    --exclude 'cmake-windows-*' \
    --exclude 'cmake-mingw-*' \
    --exclude 'cmake-clang' \
    --exclude 'build-*' \
    --exclude '.git' \
    "$SRC_MOUNT"/ "$SRC"/

cd "$SRC"

overrides=( -DMCC_QEMU_DLDIR="$ROOTS" )
[ -n "${ARCHS:-}" ] && overrides+=( -DMCC_QEMU_ARCHS="$ARCHS" )
[ -n "${LIBCS:-}" ] && overrides+=( -DMCC_QEMU_LIBCS="$LIBCS" )

echo "==> configuring (preset=$PRESET)"
cmake --preset "$PRESET" "${overrides[@]}"

echo "==> building mcc + cross compilers (-j$JOBS)"
cmake --build --preset "$PRESET" -j"$JOBS"

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

echo "==> pre-fetching x86_64 sysroots for multilib fixup (if applicable)"
ctest --test-dir "$BUILD" -R 'qemu-x86_64-.*-fetch' --output-on-failure || true
fixup_multilib

echo "==> running qemu matrix (preset=$PRESET)"
ctest --preset "$PRESET" "$@"
