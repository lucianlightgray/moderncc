#!/bin/sh
set -e

triple=$1 xdir=$2 SR=$3 out=$4 root=$5
[ -n "$triple" ] && [ -n "$xdir" ] && [ -n "$SR" ] && [ -n "$out" ] && [ -n "$root" ] || {
	echo "usage: hosted-boot.sh <triple> <xdir> <sysroot> <outdir> <root>" >&2
	exit 2
}
skip() { echo "SKIP: $*"; exit 77; }

case "$triple" in
arm64)   QEMU=qemu-aarch64; DEFS="-DMCC_TARGET_ARM64";   IDENT="(AArch64 Linux)" ;;
riscv64) QEMU=qemu-riscv64; DEFS="-DMCC_TARGET_RISCV64"; IDENT="(riscv64 Linux)" ;;
i386)    QEMU=qemu-i386;    DEFS="-DMCC_TARGET_I386";    IDENT="(i386 Linux)" ;;
arm)     QEMU=qemu-arm;     DEFS="-DMCC_TARGET_ARM -DMCC_ARM_VFP -DMCC_ARM_EABI -DMCC_ARM_HARDFLOAT"; EXTRA="-mfloat-abi hard"; IDENT="(ARM eabihf Linux)" ;;
*) skip "hosted-boot: unknown triple '$triple'" ;;
esac

command -v "$QEMU" >/dev/null 2>&1 || skip "$QEMU not on PATH"
[ -d "$SR" ] || skip "no sysroot at $SR"
CROSS="$xdir/mcc-$triple"
[ -x "$CROSS" ] || skip "no $CROSS to bootstrap a $triple-hosted mcc"
RTA="$xdir/$triple-libmccrt.a"
RMO="$xdir/$triple-runmain.o"
[ -f "$RTA" ] || skip "no $RTA (build the $triple runtime)"
[ -f "$RMO" ] || skip "no $RMO (build the $triple runtime)"

rm -rf "$out"
B="$out/B"
mkdir -p "$B"
cp "$RTA" "$B/libmccrt.a"
cp "$RTA" "$B/$triple-libmccrt.a"
cp "$RMO" "$B/runmain.o"
cp "$RMO" "$B/$triple-runmain.o"

INC="-I$root/src -I$root/include -I$root/src/formats -I$root/src/objfmt"
for a in i386 x86_64 arm arm64 riscv64; do
	INC="$INC -I$root/src/arch/$a"
done

LIBS=""
for d in usr/lib64 lib64 usr/lib lib; do
	[ -d "$SR/$d" ] && LIBS="$LIBS -L$SR/$d"
done
CRTB=""
CRTDIR=""
for d in usr/lib64 lib64 usr/lib lib; do
	[ -f "$SR/$d/crt1.o" ] || continue
	CRTDIR="$SR/$d"
	CRTB="-B$CRTDIR"
	break
done
SYSF="--sysroot=$SR"
[ -n "$CRTDIR" ] && [ "$CRTDIR" != "$SR/lib" ] && SYSF=""

MCC="$out/mcc"
"$CROSS" -w $DEFS $EXTRA -O1 \
	$INC $SYSF "-I$SR/usr/include" $CRTB $LIBS \
	-o "$MCC" "$root/src/mcc.c" -lm
[ -s "$MCC" ] || { echo "FAIL: $triple mcc did not build"; exit 1; }

if ! "$QEMU" -L "$SR" "$MCC" -v >"$out/ver" 2>&1; then
	sed -e "s/^/[$triple] /" "$out/ver"
	echo "FAIL: the $triple-hosted mcc does not start under $QEMU"
	exit 1
fi
grep -F "$IDENT" "$out/ver" >/dev/null ||
	{ sed -e "s/^/[$triple] /" "$out/ver"; echo "FAIL: hosted mcc does not identify as '$IDENT'"; exit 1; }

CORPUSFLAGS="$SYSF -I$SR/usr/include $CRTB $LIBS -I$root/runtime/include"
W="$out/mcc-hosted"
{
	echo "#!/bin/sh"
	echo "exec $QEMU -L \"$SR\" \"$MCC\" -w -B\"$B\" -L\"$B\" $CORPUSFLAGS \"\$@\""
} >"$W"
chmod +x "$W"
echo "[$triple] hosted mcc ready: $W"
