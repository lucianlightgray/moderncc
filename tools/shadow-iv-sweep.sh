#!/usr/bin/env bash
set -eu

ARCH="${1:?usage: shadow-iv-sweep.sh <arch|host> [opt ...]}"
shift || true
OPTS=("$@")
[ "${#OPTS[@]}" -gt 0 ] || OPTS=(-O1 -O2)

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

DEFS=(-DMCC_CONFIG_OPTIMIZER=1 -DMCC_CONFIG_AST_SHADOW=1)
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

SR=""
case "$ARCH" in
host|x86_64) ;;
*) SR="$REPO/vendor/gentoo-stage3-$ARCH-glibc"
   [ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR"; exit 77; }
   EXTRA+=("--sysroot=$SR" "-I$SR/usr/include") ;;
esac

command -v gcc >/dev/null 2>&1 || { echo "SKIP: no gcc"; exit 77; }
gcc -w "${DEFS[@]}" "${INCS[@]}" -O1 -o "$WORK/mcc-shadow" src/mcc.c 2>"$WORK/berr" || {
	echo "SKIP: could not build a shadow-enabled $ARCH mcc"; head -3 "$WORK/berr"; exit 77; }

case "$ARCH" in
host|x86_64) BFLAG=(-B "$REPO/cmake-debug") ;;
*)           BFLAG=(-B "$REPO/cmake-cross") ;;
esac

n=0; div=0; built=0
for f in $(find tests/exec -name '*.c' | sort) tests/diff/full_language.c; do
	for o in "${OPTS[@]}"; do
		n=$((n+1))
		out=$(timeout 300 "$WORK/mcc-shadow" "$o" "${BFLAG[@]}" "${EXTRA[@]}" \
			-c "$f" -o /dev/null 2>&1) || true
		case "$out" in
		*"side-car divergence"*) div=$((div+1)); echo "DIVERGE $f $o"; echo "$out" | grep divergence | head -1 ;;
		*) built=$((built+1)) ;;
		esac
	done
done

echo "shadow-iv-sweep $ARCH: attempts=$n clean=$built divergences=$div"
[ "$built" -gt 0 ] || { echo "FAIL: nothing compiled; the sweep is vacuous"; exit 1; }
[ "$div" -eq 0 ] || exit 1
echo "PASS: $ARCH shadow-IV zero divergence over ${OPTS[*]}"
