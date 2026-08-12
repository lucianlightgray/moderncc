#!/usr/bin/env bash
set -eu

ARCH="${1:?usage: shadow-iv-sweep.sh <arch|host> [opt ...]}"
shift || true
OPTS=("$@")
[ "${#OPTS[@]}" -gt 0 ] || OPTS=(-O1 -O2)

REPO="$(cd "$(dirname "$0")/.." && pwd)"
XDIR="${MCC_CROSS_DIR:-$REPO/cmake-cross}"
cd "$REPO"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

DEFS=(-DMCC_DEV=1)
INCS=(-Isrc -Iinclude -Isrc/formats -Isrc/objfmt -Isrc/arch/i386)
EXTRA=()
case "$ARCH" in
host|x86_64)
	DEFS+=(-DMCC_TARGET_X86_64); INCS+=(-Isrc/arch/x86_64) ;;
i386)
	DEFS+=(-DMCC_TARGET_I386) ;;
arm)
	DEFS+=(-DMCC_TARGET_ARM -DMCC_ARM_EABI -DMCC_ARM_VFP -DMCC_ARM_HARDFLOAT)
	INCS+=(-Isrc/arch/arm); EXTRA=(-mfloat-abi hard) ;;
arm64)
	DEFS+=(-DMCC_TARGET_ARM64); INCS+=(-Isrc/arch/arm64) ;;
riscv64)
	DEFS+=(-DMCC_TARGET_RISCV64); INCS+=(-Isrc/arch/riscv64) ;;
*) echo "unsupported arch '$ARCH'"; exit 2 ;;
esac

case "$(uname -m)" in
x86_64|amd64) NATIVE=x86_64 ;;
aarch64|arm64) NATIVE=arm64 ;;
i386|i486|i586|i686) NATIVE=i386 ;;
riscv64) NATIVE=riscv64 ;;
arm*) NATIVE=arm ;;
*) NATIVE=unknown ;;
esac
if [ "$ARCH" = host ]; then
	ARCH="$NATIVE"
fi

SR=""
if [ "$ARCH" = "$NATIVE" ]; then
	:
else
	SR="$REPO/vendor/gentoo-stage3-$ARCH-glibc"
	[ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR, and $ARCH is not this host's arch ($NATIVE) -- the shadow compiler would target $ARCH and be handed this host's headers, so every subject would fail to compile and the sweep would measure nothing"; exit 77; }
	EXTRA+=("--sysroot=$SR" "-I$SR/usr/include")
fi

if [ "$ARCH" = "$NATIVE" ]; then
	BDIR="${MCC_BUILD_DIR:-$REPO/cmake-debug}"
	[ -d "$BDIR" ] || { echo "SKIP: no build directory at $BDIR -- set MCC_BUILD_DIR; without one the shadow compiler cannot find mccdefs.h and every subject fails to compile, which reads as a vacuous sweep rather than as a missing path"; exit 77; }
	BFLAG=(-B "$BDIR")
	if [ -f "$BDIR/config-defines.txt" ]; then
		while IFS= read -r _d; do
			if [ -n "$_d" ]; then
				DEFS+=("-D$_d")
			fi
		done < "$BDIR/config-defines.txt"
	fi
else
	BFLAG=(-B "$XDIR")
fi

TIMEOUT=()
if command -v timeout >/dev/null 2>&1; then
	TIMEOUT=(timeout 300)
elif command -v gtimeout >/dev/null 2>&1; then
	TIMEOUT=(gtimeout 300)
fi

command -v gcc >/dev/null 2>&1 || { echo "SKIP: no gcc"; exit 77; }
gcc -w "${DEFS[@]}" "${INCS[@]}" -O1 -o "$WORK/mcc-shadow" src/mcc.c 2>"$WORK/berr" || {
	echo "SKIP: could not build a shadow-enabled $ARCH mcc"; head -3 "$WORK/berr"; exit 77; }

n=0; div=0; built=0; failed=0
for f in $(find tests/exec -name '*.c' | sort) tests/diff/full_language.c; do
	for o in "${OPTS[@]}"; do
		n=$((n+1))
		rc=0
		out=$("${TIMEOUT[@]}" "$WORK/mcc-shadow" "$o" "${BFLAG[@]}" "${EXTRA[@]}" \
			-c "$f" -o /dev/null 2>&1) || rc=$?
		case "$out" in
		*"side-car divergence"*) div=$((div+1)); echo "DIVERGE $f $o"; echo "$out" | grep divergence | head -1 ;;
		*) if [ "$rc" -eq 0 ]; then built=$((built+1)); else failed=$((failed+1)); fi ;;
		esac
	done
done

echo "shadow-iv-sweep $ARCH: attempts=$n clean=$built failed=$failed divergences=$div"
[ "$built" -gt 0 ] || { echo "FAIL: nothing compiled; the sweep is vacuous"; exit 1; }
MINPCT="${MCC_SHADOW_MIN_CLEAN_PCT:-80}"
if [ $((built * 100)) -lt $((n * MINPCT)) ]; then
	echo "FAIL: only $built of $n attempts compiled ($((built * 100 / n))%), under the ${MINPCT}% floor. divergences=$div is therefore a statement about $built subjects, not about the corpus. Raise MCC_SHADOW_MIN_CLEAN_PCT only with a reason"
	exit 1
fi
if [ "$failed" -eq 0 ]; then
	echo "shadow-iv-sweep $ARCH: every attempt compiled"
else
	echo "shadow-iv-sweep $ARCH: NOTE $failed of $n attempts exited nonzero and were exercised by the side-car zero times; $built of $n compiled, over the ${MINPCT}% floor"
fi
[ "$div" -eq 0 ] || exit 1
echo "PASS: $ARCH shadow-IV zero divergence over ${OPTS[*]}"
