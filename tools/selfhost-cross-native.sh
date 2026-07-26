#!/bin/sh
# 3-stage CROSS self-host fixpoint WITHOUT docker or a cross toolchain.
#
#   tools/selfhost-cross-native.sh <arch> [KNOB=VAL ...]      arch: riscv64|arm64
#
# Same gate as tools/selfhost-riscv64-docker.sh -- o1 == o2 == o3 with stages 2
# and 3 executed by actual riscv64 code -- but it links with mcc's OWN linker
# against the in-tree gentoo stage3 sysroot and runs under the host qemu, so it
# needs only: a host cc, qemu-riscv64, vendor/gentoo-stage3-riscv64-glibc, and
# cmake-cross/riscv64-libmccrt.a. The docker script delegates here when those
# are present, because the only cross-arch self-host gate in the tree was
# skipping (77) on any box without a running docker daemon.
#
# Three things must line up, and each fails loudly rather than subtly:
#   - the runtime archive is staged as plain `libmccrt.a` in a scratch -B dir.
#     mcc looks for MCC_CONFIG_CROSSPREFIX "libmccrt.a", and a stage-built mcc
#     has an empty prefix, so pointing -B straight at cmake-cross makes it pick
#     the HOST x86_64 libmccrt.a and fail with "invalid object file" plus
#     unresolved __clear_cache / __floatunsitf.
#   - --sysroot is required, or mcc picks up the host /usr/lib/crt1.o and again
#     reports "invalid object file".
#   - project includes must precede the sysroot includes, or the system elf.h
#     shadows src/formats/elf.h and the build dies on R_RISCV_SET_ULEB128.
#
# Usage: tools/selfhost-cross-native.sh <arch> [KNOB=VAL ...]
#   Extra args are passed as environment to every stage, so gate sets can be
#   soaked: tools/selfhost-cross-native.sh arm64 MCC_AST_IVSR_PTR=1
#
# Exit 77 (ctest SKIP) when the prerequisites are missing.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
arch=${1:-riscv64}
[ $# -gt 0 ] && shift
case "$arch" in
  riscv64) QEMU=qemu-riscv64;  TDEF=MCC_TARGET_RISCV64; ADIR=riscv64 ;;
  arm64)   QEMU=qemu-aarch64;  TDEF=MCC_TARGET_ARM64;   ADIR=arm64 ;;
  *) echo "unsupported arch '$arch' (riscv64|arm64)"; exit 77 ;;
esac
SR="$root/vendor/gentoo-stage3-$arch-glibc"
RTA="$root/cmake-cross/$arch-libmccrt.a"
CC=${CC:-cc}

command -v "$QEMU" >/dev/null 2>&1 || { echo "$QEMU not available"; exit 77; }
command -v "$CC" >/dev/null 2>&1 || { echo "no host cc"; exit 77; }
[ -d "$SR" ] || { echo "no $arch sysroot at $SR"; exit 77; }
[ -f "$RTA" ] || { echo "no $RTA (build the cross preset)"; exit 77; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/stage"
cp "$RTA" "$work/stage/libmccrt.a"

KNOBS="$*"
Q="$QEMU -L $SR"
DEF="-DMCC_CONFIG_OPTIMIZER=1 -D$TDEF"
# project includes FIRST (see header)
INC="-I$root/src -I$root/src/formats -I$root/src/objfmt -I$root/src/arch/$ADIR -I$root/include"
ARGS="-B$work/stage --sysroot=$SR -I$root/runtime/include $INC -I$SR/usr/include $DEF"

need() { [ -s "$1" ] || { echo "FAIL: $2 produced no output"; exit 1; }; }

echo "stage0: host tool emitting $arch"
$CC -w $DEF $INC -O0 -o "$work/mcc0" "$root/src/mcc.c"
need "$work/mcc0" "stage0"

echo "stage1: mcc0 compiles src/mcc.c, mcc links it"
env $KNOBS "$work/mcc0" $ARGS -c -O2 "$root/src/mcc.c" -o "$work/o1.o" 2>/dev/null
need "$work/o1.o" "stage1"
env $KNOBS "$work/mcc0" $ARGS "$work/o1.o" -o "$work/mcc1" 2>/dev/null
need "$work/mcc1" "stage1 link"

echo "stage2: $arch mcc1 compiles src/mcc.c under qemu"
env $KNOBS $Q "$work/mcc1" $ARGS -c -O2 "$root/src/mcc.c" -o "$work/o2.o" 2>/dev/null
need "$work/o2.o" "stage2"
env $KNOBS $Q "$work/mcc1" $ARGS "$work/o2.o" -o "$work/mcc2" 2>/dev/null
need "$work/mcc2" "stage2 link"

echo "stage3: $arch mcc2 compiles src/mcc.c under qemu"
env $KNOBS $Q "$work/mcc2" $ARGS -c -O2 "$root/src/mcc.c" -o "$work/o3.o" 2>/dev/null
need "$work/o3.o" "stage3"

echo "sizes o1=$(stat -c%s "$work/o1.o") o2=$(stat -c%s "$work/o2.o") o3=$(stat -c%s "$work/o3.o")"
cmp "$work/o1.o" "$work/o2.o" || { echo "FAIL: o1 != o2"; exit 1; }
cmp "$work/o2.o" "$work/o3.o" || { echo "FAIL: o2 != o3"; exit 1; }
echo "$arch selfhost: OK (o1 == o2 == o3, byte-identical, no docker)"

# a fixpoint proves stability, not correctness, so also require the
# mcc-built $arch mcc to compile a running program matching the host cc
cat > "$work/sanity.c" <<'EOF'
#include <stdio.h>
#include <string.h>
struct P { double x, y; int n; };
static double norm(struct P *p) { return -(p->x * p->x + p->y * p->y); }
int main(void) {
  struct P p; char buf[64]; int i; long acc = 0;
  p.x = 3.0; p.y = 4.0; p.n = 7;
  for (i = 0; i < 10; i++) acc += i * p.n;
  snprintf(buf, sizeof buf, "%.3f/%ld/%d", norm(&p), acc, (int)strlen("abcd"));
  printf("%s\n", buf);
  return 0;
}
EOF
$CC -w -O2 "$work/sanity.c" -o "$work/ref"
ref=$("$work/ref")
for O in -O0 -O1 -O2; do
  rm -f "$work/s"
  env $KNOBS $Q "$work/mcc1" $ARGS $O "$work/sanity.c" -o "$work/s" 2>/dev/null
  need "$work/s" "sanity $O"
  got=$($Q "$work/s")
  [ "$got" = "$ref" ] || { echo "FAIL: sanity $O got [$got] want [$ref]"; exit 1; }
done
echo "$arch sanity: OK (matches the host cc at -O0/-O1/-O2)"
