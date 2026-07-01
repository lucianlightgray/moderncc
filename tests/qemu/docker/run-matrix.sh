#!/usr/bin/env bash













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
    --exclude 'cmake-windows-*' \
    --exclude 'cmake-mingw-*' \
    --exclude 'cmake-clang' \
    --exclude 'build-*' \
    --exclude '.git' \
    "$SRC_MOUNT"/ "$SRC"/

echo "==> configuring (archs=[$ARCHS] libcs=[$LIBCS])"






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




case ";$ARCHS;" in
    *";x86_64;"*)
        echo "==> pre-fetching x86_64 sysroots for multilib fixup"
        ctest --test-dir "$BUILD" -R 'qemu-x86_64-.*-fetch' --output-on-failure || true
        fixup_multilib
        ;;
esac

echo "==> running qemu matrix"

ctest --test-dir "$BUILD" -L qemu --output-on-failure "$@"
