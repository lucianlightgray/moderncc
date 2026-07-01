#!/usr/bin/env bash
# Entrypoint for the mcc-ci image: a container-equivalent of one GitHub Actions
# `linux` matrix cell (configure -> build -> `ctest -LE qemu`).
#
# The repo is expected bind-mounted read-only at /work. We stage a clean copy at
# /src (excluding host build dirs and .git) so nothing is written back to the
# host tree and stale host artifacts never leak in; the build is out-of-source
# in /build. Mirrors the staging in tests/qemu/docker/run-matrix.sh.
#
# Env knobs (defaults reproduce the `linux` job's gcc/native cell):
#   CC             gcc | clang       (default gcc)    -> -DCMAKE_C_COMPILER
#   TARGET         native | cross    (default native) -> cross adds -DMCC_ENABLE_CROSS=ON
#   BUILD_TYPE                        (default Debug)
#   JOBS                             (default nproc)
#   LABEL_EXCLUDE                    (default qemu)    -> ctest -LE
# Any extra args are passed through to ctest (e.g. -R diff3-suite).
set -euo pipefail

SRC_MOUNT=/work
SRC=/src
BUILD=/build

CC="${CC:-gcc}"
TARGET="${TARGET:-native}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
JOBS="${JOBS:-$(nproc)}"
LABEL_EXCLUDE="${LABEL_EXCLUDE:-qemu}"

if [ ! -e "$SRC_MOUNT/CMakeLists.txt" ]; then
    echo "error: mount the mcc repo at $SRC_MOUNT (docker run -v \"\$PWD\":/work:ro ...)" >&2
    exit 2
fi
case "$TARGET" in
    native|cross) ;;
    *) echo "error: TARGET must be native|cross (got '$TARGET')" >&2; exit 2 ;;
esac

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

# A Windows checkout (git core.autocrlf) stages CRLF sources, which break
# LF-expecting tools and tests -- e.g. an exec golden whose program greps its own
# source for a `$`-anchored line finds no match under CRLF. GitHub's Linux
# actions/checkout yields LF, so this is a no-op on CI; on a Windows working tree
# it makes the staged copy behave like a Linux checkout. No repo file is
# intentionally CRLF (.gitattributes only ever forces LF), so this is safe. Opt
# out with NORMALIZE_EOL=0.
if [ "${NORMALIZE_EOL:-1}" = 1 ]; then
    echo "==> normalizing line endings (CRLF -> LF) in staged sources"
    find "$SRC" -type f \
        \( -name '*.c' -o -name '*.h' -o -name '*.cmake' -o -name '*.txt' \
           -o -name '*.S' -o -name '*.def' \) \
        -exec sed -i 's/\r$//' {} +
fi

cross_flag=""
[ "$TARGET" = cross ] && cross_flag="-DMCC_ENABLE_CROSS=ON"

echo "==> configuring (cc=$CC target=$TARGET type=$BUILD_TYPE)"
cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER="$CC" \
    $cross_flag

echo "==> building (-j$JOBS)"
cmake --build "$BUILD" -j"$JOBS"

# Everything except the qemu grid (that runs under the tests/qemu/docker image).
# On native rows wine/macho self-skip where their cross compilers are absent; on
# cross rows they exercise the win32 (wine) and macho drivers.
echo "==> testing (ctest -LE \"$LABEL_EXCLUDE\")"
ctest --test-dir "$BUILD" -j"$JOBS" --output-on-failure -LE "$LABEL_EXCLUDE" "$@"
