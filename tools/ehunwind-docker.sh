#!/usr/bin/env bash
# Runtime .eh_frame CFI guard: a C++ exception must unwind THROUGH an
# mcc-compiled C frame. extlink only checks that mcc's FDEs LINK (no overlapping
# FDEs); nothing exercises the .eh_frame at RUNTIME. Here a g++ main throws an
# exception from a callback invoked by an mcc-compiled C bridge, and the catch in
# main only fires if the C++ unwinder (libgcc _Unwind) can walk mcc's CFI for the
# bridge frame. This is the mcc-C-in-a-C++-codebase interop scenario.
#
# CFI is per-arch, so this validates each arch's unwind opcodes. arm64 runs
# host-native; riscv64/armv7 use a cross g++ + qemu-user (static so no target
# sysroot needed at run time).
#
# Usage:  tools/ehunwind-docker.sh <arch> [workdir]
#           arch: arm64 | amd64 | riscv64 | arm
# Exit:   0 pass · 1 an unwind failure · 77 skipped (no docker / toolchain / plat)
set -eu
. "$(dirname "$0")/dockergate.sh"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
ARCH="${1:-arm64}"
WORK="${2:-./w-ehunwind-$ARCH}"
rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"

HOSTM="$(uname -m)"
case "$HOSTM" in aarch64|arm64) NPLAT="linux/arm64"; NIMG="arm64v8/debian:bookworm-slim" ;; *) NPLAT="linux/amd64"; NIMG="debian:bookworm-slim" ;; esac
CROSS=""; RUNNER=""; LINKFLAGS=""; PKG="gcc g++ libc6-dev ca-certificates"
case "$ARCH" in
	arm64) IMAGE="arm64v8/debian:bookworm-slim"; PLAT="linux/arm64"; MDEF="-DMCC_TARGET_ARM64=1" ;;
	amd64) IMAGE="debian:bookworm-slim";         PLAT="linux/amd64"; MDEF="-DMCC_TARGET_X86_64=1" ;;
	riscv64) IMAGE="$NIMG"; PLAT="$NPLAT"; MDEF="-DMCC_TARGET_RISCV64=1"; CROSS="riscv64-linux-gnu-"; RUNNER="qemu-riscv64-static"; LINKFLAGS="-static"
	         PKG="$PKG g++-riscv64-linux-gnu gcc-riscv64-linux-gnu libc6-dev-riscv64-cross qemu-user-static" ;;
	arm) IMAGE="$NIMG"; PLAT="$NPLAT"; MDEF="-DMCC_TARGET_ARM=1 -DMCC_ARM_VFP=1 -DMCC_ARM_EABI=1 -DMCC_ARM_HARDFLOAT=1"; CROSS="arm-linux-gnueabihf-"; RUNNER="qemu-arm-static"; LINKFLAGS="-static"
	     PKG="$PKG g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf libc6-dev-armhf-cross qemu-user-static" ;;
	arm) echo "SKIP: armv7 uses ARM EHABI (.ARM.exidx), not DWARF .eh_frame, and C code by default has no unwind tables in BOTH mcc and gcc -- a C++ exception cannot unwind through a C frame on either, so there is no mcc-vs-gcc differential signal here (verified: both terminate identically)"; exit 77 ;;
	*) echo "SKIP: unsupported arch '$ARCH' (arm64|amd64|riscv64)"; exit 77 ;;
esac

dg_need_docker
dg_need_platform "$PLAT" "$IMAGE"

dg_docker run --rm --platform "$PLAT" -e MDEF="$MDEF" -e ARCH="$ARCH" \
  -e CROSS="$CROSS" -e RUNNER="$RUNNER" -e LINKFLAGS="$LINKFLAGS" -e PKG="$PKG" \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends $PKG >/dev/null 2>&1 \
  || { echo "SKIP: apt install of toolchain failed"; exit 77; }

GXX="${CROSS}g++"
mkdir -p /b; cp -a /repo/src /repo/include /repo/runtime /b/
INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
cd /b
echo "== build cross mcc ($MDEF) =="
gcc -O1 -w -DMCC_CONFIG_OPTIMIZER=1 $MDEF $INC src/mcc.c -o /w/mcc

# mcc-compiled C bridge frames the C++ exception must unwind through.
cat > /w/bridge.c <<EOF
void run_cb(void (*f)(void)){ f(); f(); }
long deep(void (*f)(void), long a, long b, long c){ long s = a + b + c; f(); return s; }
EOF
cat > /w/main.cpp <<EOF
#include <cstdio>
extern "C" void run_cb(void (*f)(void));
extern "C" long deep(void (*f)(void), long a, long b, long c);
static int count = 0;
static void thrower(void){ if(++count==2) throw 42; }
static void throw1(void){ throw 7; }
int main(){
  int rc = 3;
  try { run_cb(thrower); } catch(int e){ rc = (e==42)?0:10; }
  int rc2 = 3;
  try { deep(throw1, 100, 200, 300); } catch(int e){ rc2 = (e==7)?0:20; }
  printf("run_cb-unwind=%d deep-unwind=%d\n", rc, rc2);
  return rc | rc2;
}
EOF

echo "== mcc bridge.o has .eh_frame? =="
/w/mcc -O0 -I /b/runtime/include -c /w/bridge.c -o /w/bridge_mcc.o
if ! "${CROSS}readelf" -S /w/bridge_mcc.o 2>/dev/null | grep -q "eh_frame"; then
  echo "FAIL: mcc emitted no .eh_frame for the C bridge"; exit 1
fi

"$GXX" -O0 -c /w/main.cpp -o /w/main.o
echo "== g++ main + mcc bridge: exception unwinds through the mcc frame =="
"$GXX" $LINKFLAGS /w/main.o /w/bridge_mcc.o -o /w/prog
rc=0; OUT=$($RUNNER /w/prog) || rc=$?
echo "   mcc-bridge: $OUT (rc=$rc)"

# all-g++ reference: same program with a g++-compiled bridge.
cat > /w/bridge2.cpp <<EOF
extern "C" void run_cb(void (*f)(void)){ f(); f(); }
extern "C" long deep(void (*f)(void), long a, long b, long c){ long s=a+b+c; f(); return s; }
EOF
"$GXX" -O0 -c /w/bridge2.cpp -o /w/bridge2.o
"$GXX" $LINKFLAGS /w/main.o /w/bridge2.o -o /w/prog2
rr=0; REF=$($RUNNER /w/prog2) || rr=$?
echo "   all-g++  : $REF (rc=$rr)"

if [ "$rc" = 0 ] && [ "$OUT" = "$REF" ]; then
  echo "EHUNWIND PASS ($ARCH): C++ exception unwinds through mcc .eh_frame like all-g++"
else
  echo "EHUNWIND FAIL ($ARCH): unwind through the mcc frame diverged (rc=$rc vs ref rc=$rr)"
  exit 1
fi
'
