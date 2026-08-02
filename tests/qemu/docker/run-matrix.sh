#!/usr/bin/env bash
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

for shared in vendor dist; do
    if [ -d "/$shared" ]; then
        ln -sfn "/$shared" "$SRC/$shared"
        echo "==> sharing /$shared mount into the staged tree"
    fi
done

cd "$SRC"
export MCC_QEMU_DLDIR="$SRC/vendor"
exec /tmp/mcc-ci qemu "$@"
