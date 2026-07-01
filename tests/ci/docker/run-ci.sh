#!/usr/bin/env bash















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








if [ "${NORMALIZE_EOL:-1}" = 1 ]; then
    echo "==> normalizing line endings (CRLF -> LF) in staged sources"
    find "$SRC" -type f \
        \( -name '*.c' -o -name '*.h' -o -name '*.cmake' -o -name '*.txt' \
           -o -name '*.S' -o -name '*.def' \) \
        -exec sed -i 's/\r$//' {} +
fi

cross_flag=""
[ "$TARGET" = cross ] && cross_flag="-DMCC_ENABLE_CROSS=ON"

MUSL="${MUSL:-OFF}"
musl_flag=""
[ "$MUSL" = ON ] && musl_flag="-DMCC_BUILD_MUSL=ON"

release_flags=""
if [ "$BUILD_TYPE" = Release ]; then
    release_flags="-DMCC_BUILD_STRIP=ON -DMCC_CONFIG_BCHECK=OFF -DMCC_CONFIG_BACKTRACE=OFF"
fi

echo "==> configuring (cc=$CC target=$TARGET type=$BUILD_TYPE musl=$MUSL)"
cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER="$CC" \
    -DMCC_BUILD_STATIC_EXE=OFF \
    $cross_flag $musl_flag $release_flags

echo "==> building (-j$JOBS)"
cmake --build "$BUILD" -j"$JOBS"




echo "==> testing (ctest -LE \"$LABEL_EXCLUDE\")"
ctest --test-dir "$BUILD" -j"$JOBS" --output-on-failure -LE "$LABEL_EXCLUDE" "$@"
