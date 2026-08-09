#!/usr/bin/env bash
set -eu

ARCH="${1:?usage: qemu-selfhost.sh <arch> [workdir] [opt]}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${2:-$REPO/w-selfhost-$ARCH}"
OPT="${3:--O1}"
SR="$REPO/vendor/gentoo-stage3-$ARCH-glibc"
CROSS="${MCC_CROSS_DIR:-$REPO/cmake-cross}"
MCC0="$CROSS/mcc-$ARCH"

case "$ARCH" in
riscv64) QEMU=qemu-riscv64; LIBD="usr/lib64 lib64"; ABI="" ;;
arm64)   QEMU=qemu-aarch64; LIBD="usr/lib64 lib64 usr/lib lib"; ABI="" ;;
arm)     QEMU=qemu-arm;     LIBD="usr/lib lib"; ABI="-mfloat-abi hard" ;;
i386)    QEMU=qemu-i386;    LIBD="usr/lib lib"; ABI="" ;;
*) echo "unsupported arch '$ARCH'"; exit 2 ;;
esac

command -v "$QEMU" >/dev/null 2>&1 || { echo "SKIP: $QEMU not found"; exit 77; }
[ -x "$MCC0" ] || { echo "SKIP: no $MCC0 (cmake --build cmake-cross --target mcc-$ARCH)"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR"; exit 77; }
[ -f "$CROSS/$ARCH-libmccrt.a" ] || { echo "SKIP: no $ARCH-libmccrt.a in $CROSS"; exit 77; }

rm -rf "$WORK"; mkdir -p "$WORK/bd"
cp -r "$CROSS/include" "$WORK/bd/"
cp "$CROSS/$ARCH-libmccrt.a" "$WORK/bd/libmccrt.a"

cd "$REPO"
INC="-Isrc -Iinclude -Isrc/formats -Isrc/objfmt -Isrc/arch/$ARCH"
SYS="--sysroot=$SR -I$SR/usr/include"
for d in $LIBD; do [ -d "$SR/$d" ] && SYS="$SYS -L$SR/$d"; done
Q="$QEMU -L $SR"

echo "stage1: cross-build (host -> $ARCH)"
"$MCC0" $OPT $ABI -B "$CROSS" $INC $SYS src/mcc.c -o "$WORK/s1" || { echo "FAIL: stage1"; exit 1; }
echo "stage2: s1 compiles mcc under $QEMU"
$Q "$WORK/s1" $OPT $ABI -B "$WORK/bd" $INC $SYS src/mcc.c -o "$WORK/s2" || { echo "FAIL: stage2"; exit 1; }
echo "stage3: s2 compiles mcc under $QEMU"
$Q "$WORK/s2" $OPT $ABI -B "$WORK/bd" $INC $SYS src/mcc.c -o "$WORK/s3" || { echo "FAIL: stage3"; exit 1; }

if ! cmp "$WORK/s2" "$WORK/s3"; then
	echo "FAIL: stage2 != stage3 -- no fixpoint for $ARCH at $OPT"
	exit 1
fi

cat >"$WORK/t.c" <<'EOF'
extern int printf(const char *, ...);
int main(void) { int s = 0, i; for (i = 0; i < 10; i++) s += i * i;
	printf("%d\n", s); return s == 285 ? 0 : 1; }
EOF
$Q "$WORK/s3" $OPT $ABI -B "$WORK/bd" $SYS "$WORK/t.c" -o "$WORK/t.out" || { echo "FAIL: stage3 cannot compile"; exit 1; }
out=$($Q "$WORK/t.out") || { echo "FAIL: stage3's output does not run"; exit 1; }
[ "$out" = "285" ] || { echo "FAIL: stage3's output printed '$out', wanted 285"; exit 1; }

echo "PASS: $ARCH $OPT self-host fixpoint s2 == s3 ($(wc -c <"$WORK/s2") bytes) and s3 executes"
