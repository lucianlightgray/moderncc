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

# <triple>:<program> pairs whose -run failure is a documented defect, XFAILed
# instead of failing the cell. Empty: the PE -run TLS defect that kept the four
# {x86_64-win32,i386-win32} x {tls,tls_threads} cells red is fixed. A listed
# program that starts passing fails the cell so this cannot rot into lost coverage.
KNOWN_RED=""

is_known_red() {
	for kr in $KNOWN_RED; do
		[ "$kr" = "$triple:$1" ] && return 0
	done
	return 1
}

host_os=$(uname -s 2>/dev/null || echo unknown)
host_cpu=$(uname -m 2>/dev/null || echo unknown)

KIND=elf
QEMU=""
WINE=""
DOCKERPLAT=""
USEDOCKER=""
DOCKERIMG=${MCC_RUNTIER_IMAGE:-debian:stable-slim}
SR=""
BUILDDEFS=""
BUILDEXTRA=""
CORPUSFLAGS=""

case "$triple" in
x86_64)
	if [ "$host_os" = Linux ] && [ "$host_cpu" = x86_64 ]; then
		KIND=native
	else
		KIND=elf
		BUILDDEFS="-DMCC_TARGET_X86_64"
		DOCKERPLAT=linux/amd64
	fi
	IDENT="(x86_64 Linux)"
	;;
i386)
	KIND=elf; QEMU=qemu-i386; DOCKERPLAT=linux/386
	BUILDDEFS="-DMCC_TARGET_I386"
	IDENT="(i386 Linux)"
	;;
arm)
	KIND=elf; QEMU=qemu-arm; DOCKERPLAT=linux/arm/v7
	BUILDDEFS="-DMCC_TARGET_ARM -DMCC_ARM_VFP -DMCC_ARM_EABI -DMCC_ARM_HARDFLOAT"
	BUILDEXTRA="-mfloat-abi hard"
	IDENT="(ARM eabihf Linux)"
	;;
arm64)
	KIND=elf; QEMU=qemu-aarch64; DOCKERPLAT=linux/arm64
	BUILDDEFS="-DMCC_TARGET_ARM64"
	IDENT="(AArch64 Linux)"
	;;
riscv64)
	KIND=elf; QEMU=qemu-riscv64; DOCKERPLAT=linux/riscv64
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
arm64-win32|arm-win32)
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

WSRV=""

# The wineserver outlives its last client by a moment and flushes user.reg and
# userdef.reg back into the prefix as it goes. Tearing the work tree down on top
# of that writeback loses the race often enough to matter: rm empties
# .wineprefix, the server recreates a registry file in it, and the final rmdir
# fails with ENOTEMPTY. Under set -e that rm was the last command the shell ran,
# so its status became the cell's status and ctest called the cell red even
# though the corpus had already reported every program OK. Shut the server down
# and wait for it before removing anything, and never let the teardown decide
# the exit status.
cleanup() {
	rc=$?
	trap - EXIT
	if [ -n "$WSRV" ]; then
		env WINEDEBUG=-all WINEPREFIX="$work/.wineprefix" "$WSRV" -k >/dev/null 2>&1 || :
		env WINEDEBUG=-all WINEPREFIX="$work/.wineprefix" "$WSRV" -w >/dev/null 2>&1 || :
	fi
	rm -rf "$work" >/dev/null 2>&1 ||
		{ sleep 1; rm -rf "$work" >/dev/null 2>&1; } || :
	exit $rc
}

if [ -n "$DOCKERPLAT" ]; then
	work=$(mktemp -d "$bdir/run-tier.XXXXXX")
else
	work=$(mktemp -d)
fi
trap cleanup EXIT
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
	if ! command -v "$QEMU" >/dev/null 2>&1; then
		# No qemu-user here. If this triple names a docker platform and a daemon
		# is up, run there instead: same binary, real execution.
		# A daemon being up is not enough: it also needs a binfmt handler for this
		# platform, or the container dies with "exec format error". Probe it.
		if [ -n "${DOCKERPLAT:-}" ] && command -v docker >/dev/null 2>&1 &&
			 docker info >/dev/null 2>&1 &&
			 docker run --rm --platform "$DOCKERPLAT" "$DOCKERIMG" true >/dev/null 2>&1; then
			QEMU=""
			USEDOCKER=1
		else
			skip "$QEMU not available for $triple${DOCKERPLAT:+, and no docker for $DOCKERPLAT}"
		fi
	fi
	SR="$root/vendor/gentoo-stage3-$triple-glibc"
	[ -d "$SR" ] || skip "no vendored sysroot at $SR"
	LIBS=""
	CRTB=""
	for d in usr/lib64 lib64 usr/lib lib; do
		[ -d "$SR/$d" ] && LIBS="$LIBS -L$SR/$d"
	done
	# The x86_64 stage3 carries a 32-bit crt1.o in lib/ and the 64-bit one in
	# usr/lib64/. The sysroot's default crt search finds lib/ first and the link
	# dies on "invalid object file", so point -B at the directory whose crt1.o
	# actually matches the triple.
	CRTDIR=""
	for d in usr/lib64 lib64 usr/lib lib; do
		[ -f "$SR/$d/crt1.o" ] || continue
		CRTDIR="$SR/$d"
		CRTB="-B$CRTDIR"
		break
	done
	# --sysroot pins the crt search to $SR/lib and -B cannot override it, so when
	# the matching crt lives elsewhere the sysroot flag has to go; the explicit
	# -B/-L/-I below already cover what it was providing.
	SYSF="--sysroot=$SR"
	[ -n "$CRTDIR" ] && [ "$CRTDIR" != "$SR/lib" ] && SYSF=""
	RTA="$xdir/$triple-libmccrt.a"
	RMO="$xdir/$triple-runmain.o"
	[ -f "$RTA" ] || skip "no $RTA (build the $triple runtime)"
	[ -f "$RMO" ] || skip "no $RMO (build the $triple runtime)"
	cp "$RTA" "$B/libmccrt.a"
	cp "$RTA" "$B/$triple-libmccrt.a"
	cp "$RMO" "$B/runmain.o"
	cp "$RMO" "$B/$triple-runmain.o"
	echo "[$triple] bootstrapping a $triple-hosted mcc with $CROSS against $SR"
	"$CROSS" -w $BUILDDEFS $BUILDEXTRA -O1 \
		$INC $SYSF "-I$SR/usr/include" $CRTB $LIBS \
		-o "$MCC" "$root/src/mcc.c" -lm
	[ -s "$MCC" ] || fail "$triple mcc did not build"
	if [ -n "${USEDOCKER:-}" ]; then
		# The sysroot's libc.so is a GNU ld script naming /lib64 and /usr/lib64 by
		# absolute path. --sysroot would resolve those, but it also pins the crt
		# search to $SR/lib, which holds a 32-bit crt1.o here. Mounting the
		# sysroot at the paths the script already names satisfies both.
		RUN="docker run --rm --platform $DOCKERPLAT -v $root:$root"
		for d in lib64 usr/lib64 lib usr/lib; do
			[ -d "$SR/$d" ] && RUN="$RUN -v $SR/$d:/$d:ro"
		done
		RUN="$RUN -w $PWD $DOCKERIMG"
	else
		RUN="$QEMU -L $SR"
	fi
	CORPUSFLAGS="$SYSF -I$SR/usr/include $CRTB $LIBS -I$root/runtime/include"
	;;
pe)
	case "$triple" in
	i386-win32) _wl="wine wine32 wine-proton-10.0.4" ;;
	*) _wl="wine64 wine wine64-proton-10.0.4" ;;
	esac
	[ -n "$MCC_WINE" ] && _wl="$MCC_WINE $_wl"
	for w in $_wl; do
		command -v "$w" >/dev/null 2>&1 && { WINE=$w; break; }
	done
	if [ -z "$WINE" ]; then
		case "${MCC_WINE_REQUIRED:-}" in
		"" | 0 | OFF | off | FALSE | false | NO | no)
			skip "no wine for $triple" ;;
		*)
			fail "MCC_WINE_REQUIRED is set but no wine for $triple was found" ;;
		esac
	fi
	# The wineserver that goes with the wine picked above, so cleanup() can shut
	# down this prefix's server without touching any other one: a suffixed build
	# (wine64-proton-10.0.4) carries the same suffix on its server.
	case "$WINE" in
	wine64-* | wine32-*) WSRV="wineserver-${WINE#wine??-}" ;;
	wine-*) WSRV="wineserver-${WINE#wine-}" ;;
	*) WSRV=wineserver ;;
	esac
	if ! command -v "$WSRV" >/dev/null 2>&1; then
		WSRV=""
		if command -v wineserver >/dev/null 2>&1; then WSRV=wineserver; fi
	fi
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
	"$CROSS" -w "-B$B" $BUILDDEFS -O1 \
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
	# libSystem lives in the SDK, not on the filesystem root; without this the
	# bootstrap dies with "library 'c' not found" and an unresolved-libc flood.
	SDKL=""
	sdk=$(xcrun --show-sdk-path 2>/dev/null || true)
	[ -n "$sdk" ] && [ -d "$sdk/usr/lib" ] && SDKL="-L$sdk/usr/lib"
	[ -n "$SDKL" ] || skip "no macOS SDK (xcrun --show-sdk-path)"
	echo "[$triple] bootstrapping a $triple-hosted mcc with $CROSS (Mach-O)"
	"$CROSS" -w $BUILDDEFS -O1 $INC \
		"-B$root/runtime" "-I$root/runtime/include" $SDKL \
		-o "$MCC" "$root/src/mcc.c"
	[ -s "$MCC" ] || fail "$triple mcc did not build"
	CORPUSFLAGS="-I$root/runtime/include $SDKL"
	;;
esac

RUNARGS="-w -B$B -L$B $CORPUSFLAGS"

echo "[$triple] $RUN $MCC $RUNARGS"
if ! env $ENVPFX $RUN "$MCC" -v >"$work/ver" 2>&1; then
	sed -e "s/^/[$triple] /" "$work/ver"
	# Under docker the sysroot has to be mounted at the paths its ld scripts name,
	# which replaces the container's own /lib. An emulated container tolerates
	# that; a native-arch one does not -- its loader is the thing being replaced.
	# That is a property of this host, not of mcc, so it is a skip.
	[ -n "${USEDOCKER:-}" ] &&
		skip "$triple cannot be hosted under docker $DOCKERPLAT here: mounting the sysroot over the container runtime stops it executing"
	fail "the $triple-hosted mcc does not start under its runner"
fi
sed -e "s/^/[$triple] /" "$work/ver"
grep -F "$IDENT" "$work/ver" >/dev/null ||
	fail "the mcc under test does not identify as '$IDENT' -- the runner is not executing a $triple-hosted mcc"

pass=0
bad=0
xfail=0
stale=0
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
		if is_known_red "$name"; then
			xfail=$((xfail + 1))
			echo "XFAIL: [$triple] $name MCC_JIT=0 exited $rc0 (known -run TLS defect)"
			continue
		fi
		tail -20 "$work/e0" | sed -e "s/^/[$triple] $name JIT=0 stderr: /"
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name MCC_JIT=0 exited $rc0"
		continue
	fi
	if [ "$rc1" != 0 ]; then
		if is_known_red "$name"; then
			xfail=$((xfail + 1))
			echo "XFAIL: [$triple] $name MCC_JIT=1 exited $rc1 (known -run TLS defect)"
			continue
		fi
		tail -20 "$work/e1" | sed -e "s/^/[$triple] $name JIT=1 stderr: /"
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name MCC_JIT=1 exited $rc1"
		continue
	fi
	if [ "$o0" != "$want" ]; then
		if is_known_red "$name"; then
			xfail=$((xfail + 1))
			echo "XFAIL: [$triple] $name MCC_JIT=0 output mismatch (known -run TLS defect)"
			continue
		fi
		bad=$((bad + 1))
		echo "FAIL: [$triple] $name MCC_JIT=0 output mismatch"
		printf '%s\n' "--- want ---" "$want" "--- got ---" "$o0" "------"
		continue
	fi
	if [ "$o1" != "$want" ]; then
		if is_known_red "$name"; then
			xfail=$((xfail + 1))
			echo "XFAIL: [$triple] $name MCC_JIT=1 output mismatch (known -run TLS defect)"
			continue
		fi
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
	if is_known_red "$name"; then
		stale=$((stale + 1))
		echo "[$triple] $name: PASSES but is listed in KNOWN_RED"
	fi
	pass=$((pass + 1))
	echo "[$triple] $name: OK (MCC_JIT=0 == MCC_JIT=1 == expected)"
done

[ "$bad" = 0 ] || fail "$bad of $((pass + bad + xfail)) corpus programs failed"
[ "$stale" = 0 ] ||
	fail "$stale KNOWN_RED program(s) now pass -- drop them from KNOWN_RED in $0"
[ "$pass" -gt 0 ] || fail "corpus is empty"
if [ "$xfail" = 0 ]; then
	echo "[$triple] -run tier: $pass/$pass programs OK under both JIT tiers"
else
	echo "[$triple] -run tier: $pass/$((pass + xfail)) OK under both JIT tiers," \
		"$xfail known-red (see KNOWN_RED in $0)"
fi
