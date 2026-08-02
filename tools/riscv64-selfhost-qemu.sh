#!/usr/bin/env bash
# riscv64 3-stage self-host fixpoint under qemu-user.
#
# Stage 1 is the host cross compiler (an x86_64 binary emitting riscv64); stages
# 2 and 3 are riscv64 binaries running under qemu-riscv64 against the vendored
# sysroot. The assertion is stage2 == stage3 BYTE-IDENTICAL, plus that stage 3
# produces a working executable -- byte-identity passes a stable miscompile, so
# the run is not optional (instruction 3).
#
# Scope, stated rather than implied: this is qemu-user, NOT native riscv64
# silicon. Self-compile byte-identity is a compile-time property so the emulator
# is a legitimate vehicle for it, but a defect that only appears on real hardware
# is out of this script's reach.
#
# Usage: riscv64-selfhost-qemu.sh [workdir] [opt-level]
set -eu

REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$REPO/w-riscv64-selfhost}"
OPT="${2:--O1}"
SR="$REPO/vendor/gentoo-stage3-riscv64-glibc"
CROSS="$REPO/cmake-cross"
MCC0="$CROSS/mcc-riscv64"

command -v qemu-riscv64 >/dev/null 2>&1 || { echo "SKIP: qemu-riscv64 not found"; exit 77; }
[ -x "$MCC0" ] || { echo "SKIP: no $MCC0 (cmake --build cmake-cross --target mcc-riscv64)"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no riscv64 sysroot at $SR"; exit 77; }
[ -f "$CROSS/riscv64-libmccrt.a" ] || { echo "SKIP: no riscv64-libmccrt.a in $CROSS"; exit 77; }

rm -rf "$WORK"; mkdir -p "$WORK/bd"
cp -r "$CROSS/include" "$WORK/bd/"
cp "$CROSS/riscv64-libmccrt.a" "$WORK/bd/libmccrt.a"

cd "$REPO"
INC="-Isrc -Iinclude -Isrc/formats -Isrc/objfmt -Isrc/arch/riscv64"
# --sysroot alone is not enough (journal_sweep.cmake:167 records the same trap):
# the explicit usr/include is what makes the system headers resolve, and the
# lib64 -L pair is what makes crt1.o and libc resolvable at link time.
SYS="--sysroot=$SR -I$SR/usr/include -L$SR/usr/lib64 -L$SR/lib64"
Q="qemu-riscv64 -L $SR"

echo "stage1: cross-build (host x86_64 -> riscv64)"
"$MCC0" $OPT -B "$CROSS" $INC $SYS src/mcc.c -o "$WORK/s1" || { echo "FAIL: stage1"; exit 1; }
echo "stage2: s1 compiles mcc under qemu"
$Q "$WORK/s1" $OPT -B "$WORK/bd" $INC $SYS src/mcc.c -o "$WORK/s2" || { echo "FAIL: stage2"; exit 1; }
echo "stage3: s2 compiles mcc under qemu"
$Q "$WORK/s2" $OPT -B "$WORK/bd" $INC $SYS src/mcc.c -o "$WORK/s3" || { echo "FAIL: stage3"; exit 1; }

# s1 != s2 is EXPECTED -- s1 was built by a different compiler. s2 == s3 is the
# fixpoint (instruction 3's s3 == s4 shape, one stage earlier because there is
# no MCC_EMBED_* define to drop here).
if ! cmp "$WORK/s2" "$WORK/s3"; then
	echo "FAIL: stage2 != stage3 -- no fixpoint at $OPT"
	exit 1
fi

cat >"$WORK/t.c" <<'EOF'
extern int printf(const char *, ...);
int main(void) { int s = 0, i; for (i = 0; i < 10; i++) s += i * i;
	printf("%d\n", s); return s == 285 ? 0 : 1; }
EOF
$Q "$WORK/s3" $OPT -B "$WORK/bd" $SYS "$WORK/t.c" -o "$WORK/t.out" || { echo "FAIL: stage3 cannot compile"; exit 1; }
out=$($Q "$WORK/t.out") || { echo "FAIL: stage3's output does not run"; exit 1; }
[ "$out" = "285" ] || { echo "FAIL: stage3's output printed '$out', wanted 285"; exit 1; }

echo "PASS: riscv64 $OPT self-host fixpoint s2 == s3 ($(wc -c <"$WORK/s2") bytes) and s3 executes"
