#!/bin/sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
triple=${1:-x86_64}
bdir=${2:-$root/cmake-release}
xdir=${3:-$root/cmake-cross}

corpus="$root/tests/run"
[ -d "$corpus" ] || { echo "SKIP: no corpus directory at $corpus"; exit 77; }

skip() { echo "SKIP: $*"; exit 77; }
fail() { echo "FAIL: [$triple] $*"; exit 1; }

host_os=$(uname -s 2>/dev/null || echo unknown)
host_cpu=$(uname -m 2>/dev/null || echo unknown)

KIND=elf
QEMU=""
WINE=""
SR=""
BUILDDEFS=""
BUILDEXTRA=""
CORPUSFLAGS=""

case "$triple" in
x86_64)
	KIND=native
	IDENT="(x86_64 Linux)"
	;;
i386)
	KIND=elf; QEMU=qemu-i386
	BUILDDEFS="-DMCC_TARGET_I386"
	IDENT="(i386 Linux)"
	;;
arm)
	KIND=elf; QEMU=qemu-arm
	BUILDDEFS="-DMCC_TARGET_ARM -DMCC_ARM_VFP -DMCC_ARM_EABI -DMCC_ARM_HARDFLOAT"
	BUILDEXTRA="-mfloat-abi hard"
	IDENT="(ARM eabihf Linux)"
	;;
arm64)
	KIND=elf; QEMU=qemu-aarch64
	BUILDDEFS="-DMCC_TARGET_ARM64"
	IDENT="(AArch64 Linux)"
	;;
riscv64)
	KIND=elf; QEMU=qemu-riscv64
	BUILDDEFS="-DMCC_TARGET_RISCV64"
	IDENT="(riscv64 Linux)"
	;;
x86_64-win32)
	KIND=pe
	BUILDDEFS="-DMCC_TARGET_X86_64 -DMCC_TARGET_PE"
	IDENT="(x86_64 Windows)"
	;;
i386-win32)
	KIND=pe
	BUILDDEFS="-DMCC_TARGET_I386 -DMCC_TARGET_PE"
	IDENT="(i386 Windows)"
	;;
x86_64-osx)
	KIND=macho
	BUILDDEFS="-DMCC_TARGET_X86_64 -DMCC_TARGET_MACHO"
	IDENT="(x86_64 Darwin)"
	;;
arm64-osx)
	KIND=macho
	BUILDDEFS="-DMCC_TARGET_ARM64 -DMCC_TARGET_MACHO"
	IDENT="(AArch64 Darwin)"
	;;
arm64-win32|arm-win32|arm-wince)
	skip "no runner for $triple on any host available here: -run needs a $triple-hosted mcc, and there is no Windows-on-ARM emulator (wine is x86-only, qemu-user cannot load PE)"
	;;
*)
	skip "unknown triple '$triple'"
	;;
esac

CROSS="$xdir/mcc-$triple"
[ "$KIND" = native ] || [ -x "$CROSS" ] || skip "no $CROSS to bootstrap a $triple-hosted mcc with"

INC="-I$root/src -I$root/include -I$root/src/formats -I$root/src/objfmt"
for a in i386 x86_64 arm arm64 riscv64; do
	INC="$INC -I$root/src/arch/$a"
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
B="$work/B"
mkdir -p "$B/lib"
MCC="$work/mcc"
RUN=""
ENVPFX=""

case "$KIND" in
native)
	MCC="$bdir/mcc"
	[ -x "$MCC" ] || skip "no native mcc at $MCC"
	B="$bdir"
	CORPUSFLAGS="-I$root/runtime/include"
	;;
elf)
	command -v "$QEMU" >/dev/null 2>&1 || skip "$QEMU not available for $triple"
	SR="$root/vendor/gentoo-stage3-$triple-glibc"
	[ -d "$SR" ] || skip "no vendored sysroot at $SR"
	LIBS=""
	for d in usr/lib64 lib64 usr/lib lib; do
		[ -d "$SR/$d" ] && LIBS="$LIBS -L$SR/$d"
	done
	RTA="$xdir/$triple-libmccrt.a"
	RMO="$xdir/$triple-runmain.o"
	[ -f "$RTA" ] || skip "no $RTA (build the $triple runtime)"
	[ -f "$RMO" ] || skip "no $RMO (build the $triple runtime)"
	cp "$RTA" "$B/libmccrt.a"
	cp "$RTA" "$B/$triple-libmccrt.a"
	cp "$RMO" "$B/runmain.o"
	cp "$RMO" "$B/$triple-runmain.o"
	echo "[$triple] bootstrapping a $triple-hosted mcc with $CROSS against $SR"
	"$CROSS" -w $BUILDDEFS -DMCC_CONFIG_OPTIMIZER=1 $BUILDEXTRA -O1 \
		$INC --sysroot="$SR" "-I$SR/usr/include" $LIBS \
		-o "$MCC" "$root/src/mcc.c" -lm
	[ -s "$MCC" ] || fail "$triple mcc did not build"
	RUN="$QEMU -L $SR"
	CORPUSFLAGS="--sysroot=$SR -I$SR/usr/include $LIBS -I$root/runtime/include"
	;;
pe)
	case "$triple" in
	i386-win32) for w in wine wine32 wine-proton-10.0.4; do
			command -v "$w" >/dev/null 2>&1 && { WINE=$w; break; }
		done ;;
	*) for w in wine64 wine wine64-proton-10.0.4; do
			command -v "$w" >/dev/null 2>&1 && { WINE=$w; break; }
		done ;;
	esac
	[ -n "$WINE" ] || skip "no wine for $triple"
	OBJDIR="$xdir/lib-$triple"
	RTA="$xdir/$triple-libmccrt.a"
	RMO="$xdir/$triple-runmain.o"
	[ -d "$OBJDIR" ] || skip "no $OBJDIR (build the $triple runtime)"
	[ -f "$RTA" ] || skip "no $RTA (build the $triple runtime)"
	[ -f "$RMO" ] || skip "no $RMO (build the $triple runtime)"
	cp "$root"/runtime/win32/lib/*.def "$B/lib/"
	cp "$OBJDIR"/*.o "$B/lib/"
	cp "$OBJDIR"/*.o "$B/"
	cp "$RTA" "$B/lib/$triple-libmccrt.a"
	cp "$RTA" "$B/$triple-libmccrt.a"
	cp "$RMO" "$B/runmain.o"
	cp "$RMO" "$B/$triple-runmain.o"
	MCC="$work/mcc.exe"
	echo "[$triple] bootstrapping a $triple-hosted mcc with $CROSS (PE, run under $WINE)"
	"$CROSS" -w "-B$B" $BUILDDEFS -DMCC_CONFIG_OPTIMIZER=1 -O1 \
		"-I$root/runtime/win32/include" "-I$root/runtime/win32/include/winapi" \
		"-I$root/runtime/include" $INC \
		-o "$MCC" "$root/src/mcc.c"
	[ -s "$MCC" ] || fail "$triple mcc.exe did not build"
	RUN="$WINE"
	ENVPFX="WINEDEBUG=-all WINEPREFIX=$work/.wineprefix"
	CORPUSFLAGS="-I$root/runtime/win32/include -I$root/runtime/include"
	;;
macho)
	[ "$host_os" = Darwin ] || skip "$triple -run needs a Darwin host; this host is $host_os"
	case "$triple:$host_cpu" in
	arm64-osx:arm64) ;;
	x86_64-osx:x86_64) ;;
	x86_64-osx:arm64) ;;
	arm64-osx:*) skip "arm64-osx -run needs Apple silicon; this Darwin host is $host_cpu" ;;
	*) skip "no runner for $triple on Darwin/$host_cpu" ;;
	esac
	RTA="$xdir/$triple-libmccrt.a"
	RMO="$xdir/$triple-runmain.o"
	[ -f "$RTA" ] || skip "no $RTA (build the $triple runtime)"
	[ -f "$RMO" ] || skip "no $RMO (build the $triple runtime)"
	cp "$RTA" "$B/libmccrt.a"
	cp "$RTA" "$B/$triple-libmccrt.a"
	cp "$RMO" "$B/runmain.o"
	cp "$RMO" "$B/$triple-runmain.o"
	echo "[$triple] bootstrapping a $triple-hosted mcc with $CROSS (Mach-O)"
	"$CROSS" -w $BUILDDEFS -DMCC_CONFIG_OPTIMIZER=1 -O1 $INC \
		"-B$root/runtime" "-I$root/runtime/include" \
		-o "$MCC" "$root/src/mcc.c"
	[ -s "$MCC" ] || fail "$triple mcc did not build"
	CORPUSFLAGS="-I$root/runtime/include"
	;;
esac

RUNARGS="-w -B$B -L$B $CORPUSFLAGS"

echo "[$triple] $RUN $MCC $RUNARGS"
if ! env $ENVPFX $RUN "$MCC" -v >"$work/ver" 2>&1; then
	sed -e "s/^/[$triple] /" "$work/ver"
	fail "the $triple-hosted mcc does not start under its runner"
fi
sed -e "s/^/[$triple] /" "$work/ver"
grep -F "$IDENT" "$work/ver" >/dev/null ||
	fail "the mcc under test does not identify as '$IDENT' -- the runner is not executing a $triple-hosted mcc"

pass=0
bad=0
for src in "$corpus"/*.c; do
	case "$src" in *.aux.c) continue ;; esac
	name=$(basename "$src" .c)
	exp="$corpus/$name.expected"
	[ -f "$exp" ] || fail "$name has no .expected"
	aux=""
	[ -f "$corpus/$name.aux.c" ] && aux="$corpus/$name.aux.c"
	args=""
	[ -f "$corpus/$name.args" ] && args=$(cat "$corpus/$name.args")

	o0=""
	o1=""
	rc0=0
	rc1=0
	set +e
	env $ENVPFX MCC_JIT=0 $RUN "$MCC" $RUNARGS $aux -run "$src" $args \
		>"$work/o0" 2>"$work/e0"
	rc0=$?
	env $ENVPFX MCC_JIT=1 $RUN "$MCC" $RUNARGS $aux -run "$src" $args \
		>"$work/o1" 2>"$work/e1"
	rc1=$?
	set -e
	o0=$(tr -d '\r' <"$work/o0")
	o1=$(tr -d '\r' <"$work/o1")

	want=$(cat "$exp")
	if [ "$rc0" != 0 ]; then
		tail -20 "$work/e0" | sed -e "s/^/[$triple] $name JIT=0 stderr: /"
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name MCC_JIT=0 exited $rc0"
		continue
	fi
	if [ "$rc1" != 0 ]; then
		tail -20 "$work/e1" | sed -e "s/^/[$triple] $name JIT=1 stderr: /"
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name MCC_JIT=1 exited $rc1"
		continue
	fi
	if [ "$o0" != "$want" ]; then
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name MCC_JIT=0 output mismatch"
		printf '%s\n' "--- want ---" "$want" "--- got ---" "$o0" "------"
		continue
	fi
	if [ "$o1" != "$want" ]; then
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name MCC_JIT=1 output mismatch"
		printf '%s\n' "--- want ---" "$want" "--- got ---" "$o1" "------"
		continue
	fi
	if [ "$o0" != "$o1" ]; then
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name JIT parity broken"
		continue
	fi
	pass=$((pass + 1))
	echo "[$triple] $name: OK (MCC_JIT=0 == MCC_JIT=1 == expected)"
done

[ "$bad" = 0 ] || fail "$bad of $((pass + bad)) corpus programs failed"
[ "$pass" -gt 0 ] || fail "corpus is empty"
echo "[$triple] -run tier: $pass/$pass programs OK under both JIT tiers"
