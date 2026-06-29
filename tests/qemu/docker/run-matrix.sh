#!/usr/bin/env bash
# Entrypoint for the mcc-qemu image: configure mcc with the cross compilers +
# qemu matrix enabled, build, and run `ctest -L qemu`.
#
# The repo is expected to be bind-mounted read-only at /work. We stage a clean
# copy at /src (excluding host build dirs and .git) so nothing is written back
# to the macOS tree and stale host artifacts never leak in. Sysroots land in
# /qemu-roots — mount a named volume there to cache the Gentoo downloads.
#
# Env knobs:
#   ARCHS   default "x86_64;i386;arm;arm64;riscv64"
#   LIBCS   default "glibc;musl"
#   JOBS    default $(nproc)
# Any extra args are passed through to ctest (e.g. -R qemu-x86_64-glibc).
set -euo pipefail

SRC_MOUNT=/work
SRC=/src
BUILD=/build
ROOTS=/qemu-roots

ARCHS="${ARCHS:-x86_64;i386;arm;arm64;riscv64}"
LIBCS="${LIBCS:-glibc;musl}"
JOBS="${JOBS:-$(nproc)}"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work ...)" >&2
    exit 2
fi

echo "==> staging source $SRC_MOUNT -> $SRC"
mkdir -p "$SRC" "$ROOTS"
rsync -a --delete \
    --exclude 'cmake-build*' \
    --exclude '.git' \
    "$SRC_MOUNT"/ "$SRC"/

echo "==> configuring (archs=[$ARCHS] libcs=[$LIBCS])"
# Mirrors the `cross` preset (MCC_ENABLE_CROSS=ON, Debug, Unix Makefiles) but
# with an out-of-source build dir so the preset's in-tree binaryDir never lands
# on the host mount. MCC_QEMU_DLDIR redirects sysroots to the cache volume.
# MCC_CROSS_DIR points the darwin/macho row (qemu-arm64-osx) at this build's
# cross compilers (arm64-osx-mcc + lib-arm64-osx live in $BUILD, not the default
# in-source cmake-build-cross), so it runs here instead of self-skipping.
cmake -S "$SRC" -B "$BUILD" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMCC_ENABLE_CROSS=ON \
    -DMCC_QEMU_TESTS=ON \
    -DMCC_QEMU_DLDIR="$ROOTS" \
    -DMCC_CROSS_DIR="$BUILD" \
    -DMCC_QEMU_ARCHS="$ARCHS" \
    -DMCC_QEMU_LIBCS="$LIBCS"

echo "==> building mcc + cross compilers (-j$JOBS)"
cmake --build "$BUILD" -j"$JOBS"

# --- multilib amd64 sysroot fixup -------------------------------------------
# The Gentoo amd64 stage3 is multilib: usr/lib holds the 32-bit (i386) startup
# objects, usr/lib64 the 64-bit ones. But x86_64-mcc is built with
# CRTPREFIX="{R}/lib", so it would pick the 32-bit crt1.o/crti.o and reject
# them ("invalid object file"). On an x86_64 host the x86_64 row uses the
# native mcc and never hits this; on a non-x86_64 host it goes through the
# cross compiler, so we overlay the 64-bit startup objects into {R}/lib. This
# sysroot is used only by the x86_64 target, so the overwrite is safe; library
# resolution already prefers lib64 (harness passes -L lib64 first). Single-arch
# stage3s (i386/arm/arm64/riscv64) need no fixup.
fixup_multilib() {
    for root in "$ROOTS"/x86_64-*; do
        [ -d "$root" ] || continue
        local lib="$root/usr/lib" lib64="$root/usr/lib64"
        [ -f "$lib64/crt1.o" ] || continue   # not multilib (e.g. musl) -> skip
        for o in crt1.o crti.o crtn.o Scrt1.o gcrt1.o Mcrt1.o; do
            [ -f "$lib64/$o" ] && cp -f "$lib64/$o" "$lib/$o"
        done
        echo "==> fixed multilib crt in $(basename "$root")"
    done
}

# Pre-fetch the x86_64 sysroots so the fixup can run before the matrix; the
# fetch fixtures are idempotent (a marker short-circuits repeats), so the full
# run below reuses them. Other arches fetch normally during the run.
case ";$ARCHS;" in
    *";x86_64;"*)
        echo "==> pre-fetching x86_64 sysroots for multilib fixup"
        ctest --test-dir "$BUILD" -R 'qemu-x86_64-.*-fetch' --output-on-failure || true
        fixup_multilib
        ;;
esac

echo "==> running qemu matrix"
# -L qemu selects the matrix (fetch fixtures + per-(arch,libc) run tests).
ctest --test-dir "$BUILD" -L qemu --output-on-failure "$@"
