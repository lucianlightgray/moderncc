#!/usr/bin/env bash
#
# tests/benchmarks runner.
#
# Builds every *.c benchmark in this directory with four threading toolchains
#   gcc       native <threads.h> (glibc/pthread)
#   clang     native <threads.h> (glibc/pthread)
#   mcc-nat   mcc, native <threads.h> (pthread backend)
#   mcc-coop  mcc, cooperative fibers (-DMCC_THREADS_COOP)
# at -O0 -O1 -O3 (mcc-coop and mcc-nat share mcc's ladder), runs each, checks the
# output against a serial reference, and reports perf instructions:u (stable) and
# best-of-N wall-clock (advisory — the box may be loaded).
#
# Usage: tests/benchmarks/run.sh [N] [REPS]
set -u
cd "$(dirname "$0")/../.."
ROOT=$(pwd)
MCC=${MCC:-$ROOT/cmake-def/mcc}
MB=${MB:-$ROOT/cmake-def}
N=${1:-2000}
REPS=${2:-3}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

have() { command -v "$1" >/dev/null 2>&1; }
GCC=$(command -v gcc || true)
CLANG=$(command -v clang || true)

# reference value: serial build of the fork-join kernel with NT=1
REF=""
ref_src=$ROOT/tests/benchmarks/spectral_norm_forkjoin.c
if [ -n "$GCC" ]; then
	"$GCC" -O2 -pthread -DNT=1 "$ref_src" -o "$WORK/ref" -lm 2>/dev/null && REF=$("$WORK/ref" "$N")
fi
echo "reference (serial) = ${REF:-<none>}    N=$N  reps=$REPS"

bestrun() { # $1=exe args... -> INSN, MS (min over REPS)
	local exe=$1; shift
	local bi="" bm=""
	for i in $(seq 1 "$REPS"); do
		local t0=$EPOCHREALTIME
		perf stat -x, -e instructions:u -o "$WORK/p" "$exe" "$@" >"$WORK/o" 2>/dev/null
		[ $? -ne 0 ] && { INSN=ERR; MS=ERR; OUT=ERR; return; }
		local t1=$EPOCHREALTIME
		local ins ms
		ins=$(grep instructions:u "$WORK/p" | cut -d, -f1)
		ms=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.1f",(b-a)*1000}')
		[ -z "$ins" ] && ins=0
		[ -z "$bi" ] && bi=$ins || { awk -v x="$ins" -v y="$bi" 'BEGIN{exit !(x<y)}' && bi=$ins; }
		[ -z "$bm" ] && bm=$ms  || { awk -v x="$ms"  -v y="$bm" 'BEGIN{exit !(x<y)}' && bm=$ms; }
	done
	INSN=$bi; MS=$bm; OUT=$(cat "$WORK/o")
}

for src in "$ROOT"/tests/benchmarks/*.c; do
	name=$(basename "$src" .c)
	echo
	echo "### $name"
	printf "%-10s %-4s | %14s | %9s | %s\n" toolchain -O "instructions" "wall(ms)" "result"
	# toolchain|compiler-invocation-tag|olevels
	for tc in "gcc" "clang" "mcc-nat" "mcc-coop"; do
		for o in 0 1 3; do
			exe="$WORK/${name}_${tc}_O${o}"
			case $tc in
				gcc)      [ -z "$GCC" ]   && continue; "$GCC"   -O$o -pthread "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				clang)    [ -z "$CLANG" ] && continue; "$CLANG" -O$o -pthread "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				mcc-nat)  "$MCC" -B"$MB" -O$o -pthread "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				mcc-coop) "$MCC" -B"$MB" -O$o -DMCC_THREADS_COOP "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
			esac
			if [ $? -ne 0 ]; then
				printf "%-10s -O%s | %14s | %9s | %s\n" "$tc" "$o" "BUILD-FAIL" "-" "$(head -1 "$WORK/be")"
				continue
			fi
			bestrun "$exe" "$N"
			ok="ok"; [ -n "$REF" ] && [ "$OUT" != "$REF" ] && ok="MISMATCH($OUT)"
			printf "%-10s -O%s | %14s | %9s | %s\n" "$tc" "$o" "$INSN" "$MS" "$ok"
		done
	done
done
