#!/usr/bin/env bash
# Regression guard for the arm ARM/Thumb interworking bug on EABI-helper calls.
#
# mcc emits A32 code and reaches runtime helpers (__aeabi_memmove/memcpy,
# __stack_chk_fail, bounds/asan/ubsan) through a `bl`. Historically that `bl`
# carried the deprecated R_ARM_PC24 reloc, which GNU ld does NOT treat as
# interworking-capable: linked against a THUMB helper it emits a plain ARM->Thumb
# veneer that SIGILLs when entered from mcc's ARM caller. The fix emits R_ARM_CALL
# (for `bl`) / R_ARM_JUMP24 (for `b`) so ld performs the standard BL<->BLX
# interworking substitution.
#
# This guard, run under qemu-arm, (1) asserts mcc's call relocs are R_ARM_CALL and
# zero R_ARM_PC24 remain, and (2) links an mcc-compiled struct-copy program (which
# emits __aeabi_memmove8) against a *Thumb* shim helper, static-links with GNU ld,
# and requires it to run correctly under qemu-arm -- a SIGILL here means the reloc
# regressed. NB: on Apple Silicon `--platform linux/amd64` breaks qemu-arm's 32-bit
# VA reservation, so we run on the host-native platform (amd64 on CI x86, arm64 on
# Apple Silicon) where qemu-arm works either way.
set -eu

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
WORK="${1:-./w-arminterwork}"
rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"
IMAGE="debian:bookworm-slim"

if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! docker info >/dev/null 2>&1; then echo "SKIP: docker daemon not available"; exit 77; fi

MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends \
  gcc libc6-dev gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
  libc6-dev-armhf-cross qemu-user-static ca-certificates >/dev/null 2>&1 \
  || { echo "SKIP: apt install of arm toolchain failed"; exit 77; }

mkdir -p /b; cp -a /repo/src /repo/include /repo/runtime /b/
INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
DEF="-DMCC_TARGET_ARM=1 -DMCC_ARM_VFP=1 -DMCC_ARM_EABI=1 -DMCC_ARM_HARDFLOAT=1"
cd /b
echo "== build arm (armv7 AAPCS) cross mcc =="
gcc -O1 -w $DEF $INC src/mcc.c -o /w/mcc-arm
RD=/b/runtime/include

cat > /w/t.c <<EOF
extern int printf(const char*,...);
struct Big { long long a,b,c,d,e,f,g,h; };
struct Big mk(long long s){ struct Big b; b.a=s; b.b=s+1; b.c=s+2; b.d=s+3; b.e=s+4; b.f=s+5; b.g=s+6; b.h=s+7; return b; }
long long consume(struct Big b){ return b.a+b.b+b.c+b.d+b.e+b.f+b.g+b.h; }
int main(void){
  struct Big x = mk(10);
  struct Big y; y = x;
  long long r = consume(y);
  printf("r=%lld (expect 108)\n", r);
  return r == 108 ? 0 : 3;
}
EOF

# Thumb shim: DEFINES the aeabi helpers mcc calls, as Thumb code. Its strong .o
# defs win over the (ARM) libc archive members, forcing the ARM-caller ->
# Thumb-callee interworking edge that R_ARM_PC24 mis-relocates.
cat > /w/shim.c <<EOF
extern void *memmove(void*, const void*, unsigned long);
extern void *memcpy(void*, const void*, unsigned long);
void __aeabi_memmove8(void *d, const void *s, unsigned long n){ memmove(d,s,n); }
void __aeabi_memmove4(void *d, const void *s, unsigned long n){ memmove(d,s,n); }
void __aeabi_memmove(void *d, const void *s, unsigned long n){ memmove(d,s,n); }
void __aeabi_memcpy8(void *d, const void *s, unsigned long n){ memcpy(d,s,n); }
void __aeabi_memcpy4(void *d, const void *s, unsigned long n){ memcpy(d,s,n); }
void __aeabi_memcpy(void *d, const void *s, unsigned long n){ memcpy(d,s,n); }
EOF
arm-linux-gnueabihf-gcc -mthumb -O2 -c /w/shim.c -o /w/shim.o

echo "== assert 1: mcc call relocs are R_ARM_CALL, zero R_ARM_PC24 =="
/w/mcc-arm -O0 -I $RD -c /w/t.c -o /w/t.o
if arm-linux-gnueabihf-readelf -r /w/t.o | grep -q "R_ARM_PC24"; then
  echo "FAIL: mcc still emits R_ARM_PC24 for calls"; arm-linux-gnueabihf-readelf -r /w/t.o | grep R_ARM_PC24; exit 1
fi
if ! arm-linux-gnueabihf-readelf -r /w/t.o | grep -q "R_ARM_CALL .* __aeabi_memmove8"; then
  echo "FAIL: expected R_ARM_CALL against __aeabi_memmove8"; arm-linux-gnueabihf-readelf -r /w/t.o | grep -iE "aeabi|R_ARM"; exit 1
fi
echo "   OK: call relocs are R_ARM_CALL"

echo "== assert 2: interworking run against a Thumb helper (SIGILL if regressed) =="
arm-linux-gnueabihf-gcc -marm -static /w/t.o /w/shim.o -o /w/t.elf
echo -n "   __aeabi_memmove8 provider: "; arm-linux-gnueabihf-nm /w/t.elf | grep -iE " T __aeabi_memmove8$" | head -1
set +e
OUT=$(qemu-arm-static /w/t.elf); RC=$?
set -e
echo "   qemu-arm output: $OUT (rc=$RC)"
if [ $RC -ne 0 ] || [ "$OUT" != "r=108 (expect 108)" ]; then
  echo "FAIL: interworking run did not produce the expected result (rc=$RC)"; exit 1
fi
echo "   OK: ARM->Thumb interworking call runs correctly"

echo "PASS: arm Thumb-interworking guard"
'
