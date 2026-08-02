#!/usr/bin/env bash
set -eu
. "$(dirname "$0")/dockergate.sh"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
IMAGE_386="${MCC_I386_DOCKER_IMAGE:-i386/debian:bookworm-slim}"

dg_need_docker
dg_need_platform linux/386 "$IMAGE_386"

dg_docker run --rm --platform linux/386 -v "$HP":/src:ro "$IMAGE_386" sh -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y -qq gcc cmake ninja-build python3 >/dev/null 2>&1 \
  || { echo "SKIP: apt install failed"; exit 77; }

mkdir -p /work && cd /work
cp -r /src/src /src/include /src/runtime /src/tools /src/tests /src/cmake \
      /src/CMakeLists.txt /src/CMakePresets.json /work/
cmake -G Ninja -S /work -B /work/b -DCMAKE_BUILD_TYPE=Release \
      -DMCC_SUPERBUILD_TEST=OFF -DMCC_TARGET_ARCH=i386 >/dev/null
cmake --build /work/b --target mcc -j"$(nproc)" >/dev/null

ver=$(/work/b/mcc -v 2>&1 | head -1)
echo "built: $ver"
case "$ver" in
  *"(i386 Linux)"*) ;;
  *) echo "FAIL: built mcc is not i386 -- MCC_TARGET_ARCH did not take"; exit 1;;
esac

INC="-I/usr/include/i386-linux-gnu -I/work/src -I/work/include -I/work/src/formats
     -I/work/src/objfmt -I/work/src/arch/i386 -L/usr/lib/i386-linux-gnu -L/lib/i386-linux-gnu"

MCC_JIT=1 /work/b/mcc -B/work/b $INC -O1 --stats -run src/mcc.c \
    -B/work/b $INC -c src/mcc.c -o /work/jit1.o >/work/o1 2>/work/e1 || true
MCC_JIT=0 /work/b/mcc -B/work/b $INC -O1 --stats -run src/mcc.c \
    -B/work/b $INC -c src/mcc.c -o /work/jit0.o >/work/o0 2>/work/e0 || true

grep -q "error:" /work/e1 && { echo "FAIL: MCC_JIT=1 run errored:"; grep "error:" /work/e1 | head -3; exit 1; }
grep -q "error:" /work/e0 && { echo "FAIL: MCC_JIT=0 run errored:"; grep "error:" /work/e0 | head -3; exit 1; }
[ -s /work/jit1.o ] && [ -s /work/jit0.o ] || { echo "FAIL: no object produced"; exit 1; }

n1=$(cat /work/o1 /work/e1 | sed -n "s/.*recompiles=\([0-9]*\).*/\1/p" | head -1)
n0=$(cat /work/o0 /work/e0 | sed -n "s/.*recompiles=\([0-9]*\).*/\1/p" | head -1)
echo "MCC_JIT=1 recompiles=$n1   MCC_JIT=0 recompiles=$n0   object=$(wc -c </work/jit1.o) bytes"
[ "${n1:-0}" -ge 1 ] || { echo "FAIL: the JIT never fired -- this run proves nothing"; exit 1; }
[ "${n0:-0}" -eq 0 ] || { echo "FAIL: MCC_JIT=0 recompiled; the control is not a control"; exit 1; }
cmp /work/jit1.o /work/jit0.o || { echo "FAIL: MCC_JIT=1 and MCC_JIT=0 objects differ"; exit 1; }
echo "PASS: i386 mcc self-hosts under its own JIT and MCC_JIT=1 == MCC_JIT=0"
'
