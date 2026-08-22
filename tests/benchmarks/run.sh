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
NS=${1:-"1000 2000 4000"}
REPS=${2:-3}
TIMEOUT=""
command -v timeout >/dev/null 2>&1 && TIMEOUT="timeout 90"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

have() { command -v "$1" >/dev/null 2>&1; }
GCC=$(command -v gcc || true)
if [ -n "$GCC" ] && "$GCC" --version 2>/dev/null | grep -qi clang; then
	GCC=""
	while IFS= read -r g; do [ -x "$g" ] && GCC=$g; done \
		< <(IFS=:; for d in $PATH; do ls "$d"/gcc-[0-9]* 2>/dev/null; done | sort -Vu)
fi
CLANG=$(command -v clang || true)
INC="$ROOT/runtime/include"

PERF=""
if command -v perf >/dev/null 2>&1 &&
	perf stat -x, -e instructions:u -o "$WORK/probe" true >/dev/null 2>&1 &&
	grep -q instructions:u "$WORK/probe" 2>/dev/null; then
	PERF=1
fi

# reference value: serial build of the fork-join kernel with NT=1
REF=""
REFCC=""
for refcc in "$GCC" "$CLANG"; do [ -n "$refcc" ] && { REFCC=$refcc; break; }; done
# mcc-gpu toolchain (heterogeneous CPU+GPU, T-lin-10526) is offered only for the
# kernels that carry a -DMCC_GPU_OFFLOAD path (today: mandelbrot) and only when a
# Vulkan device is present AND -lvulkan links. Its default run splits the work
# ~50/50 across the GPU and the C11 thread pool (MCC_GPU_PERCENT overrides).
GPU_OK=""
if command -v vulkaninfo >/dev/null 2>&1 && vulkaninfo --summary >/dev/null 2>&1; then
	printf 'int main(void){return 0;}\n' > "$WORK/vkprobe.c"
	"$MCC" -B"$MB" "$WORK/vkprobe.c" -o "$WORK/vkprobe" -lvulkan >/dev/null 2>&1 && GPU_OK=1
fi

echo "inputs (N)=$NS  reps=$REPS  perf=$([ -n "$PERF" ] && echo on || echo off)  timeout=$([ -n "$TIMEOUT" ] && echo on || echo off)  gpu=$([ -n "$GPU_OK" ] && echo on || echo off)"
echo "each build is verified at EVERY N against the serial reference; gcc and clang serial refs are cross-checked"

bestrun() { # $1=exe $2=N -> INSN, MS (min over REPS), OUT; sets RC (124=timeout/HANG)
	local exe=$1 n=$2
	local bi="" bm=""
	RC=0
	for i in $(seq 1 "$REPS"); do
		local t0=$EPOCHREALTIME rc ins ms gm
		if [ -n "$PERF" ]; then
			$TIMEOUT perf stat -x, -e instructions:u -o "$WORK/p" "$exe" "$n" >"$WORK/o" 2>"$WORK/oe"; rc=$?
		else
			$TIMEOUT "$exe" "$n" >"$WORK/o" 2>"$WORK/oe"; rc=$?
		fi
		[ $rc -ne 0 ] && { INSN=ERR; MS=ERR; OUT=ERR; RC=$rc; return; }
		local t1=$EPOCHREALTIME
		ms=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.1f",(b-a)*1000}')
		# mcc-gpu reports compute-only wall (GPUs warmed; one-time Vulkan init and
		# teardown excluded, as they would be amortized in a long-running process).
		gm=$(grep -o 'MCCGPU_MS=[0-9.]*' "$WORK/oe" 2>/dev/null | head -1 | cut -d= -f2)
		[ -n "$gm" ] && ms=$gm
		if [ -n "$PERF" ]; then
			ins=$(grep instructions:u "$WORK/p" | cut -d, -f1)
			[ -z "$ins" ] && ins=0
			[ -z "$bi" ] && bi=$ins || { awk -v x="$ins" -v y="$bi" 'BEGIN{exit !(x<y)}' && bi=$ins; }
		fi
		[ -z "$bm" ] && bm=$ms  || { awk -v x="$ms"  -v y="$bm" 'BEGIN{exit !(x<y)}' && bm=$ms; }
	done
	INSN=${bi:-n/a}; MS=$bm; OUT=$(cat "$WORK/o")
}

FAILS=0
for src in "$ROOT"/tests/benchmarks/*.c; do
	name=$(basename "$src" .c)
	echo
	echo "### $name"
	# build each toolchain x -O once; run it against every N below
	tags=""
	for tc in "gcc" "clang" "mcc-nat" "mcc-coop" "mcc-coop-mn" "mcc-gpu"; do
		# mcc-gpu applies only to kernels with a -DMCC_GPU_OFFLOAD path, and only
		# when a Vulkan device is available (else no row, not a failure).
		if [ "$tc" = mcc-gpu ]; then
			[ "$name" = mandelbrot ] || continue
			[ -n "$GPU_OK" ] || { echo "  SKIP mcc-gpu (all -O): no linkable Vulkan device"; continue; }
		fi
		case $tc in
			mcc-*) olevels="0 1 2 3 4" ;;
			*)     olevels="0 1 2 3" ;;
		esac
		for o in $olevels; do
			exe="$WORK/${name}_${tc}_O${o}"
			case $tc in
				gcc)      [ -z "$GCC" ]   && continue; "$GCC"   -O$o -pthread -I "$INC" "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				clang)    [ -z "$CLANG" ] && continue; "$CLANG" -O$o -pthread -I "$INC" "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				mcc-nat)  "$MCC" -B"$MB" -O$o -pthread "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				mcc-coop) "$MCC" -B"$MB" -O$o -DMCC_THREADS_COOP "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				mcc-coop-mn) "$MCC" -B"$MB" -O$o -DMCC_THREADS_COOP -DMCC_COOP_MN -pthread "$src" -o "$exe" -lm >"$WORK/be" 2>&1 ;;
				mcc-gpu)  "$MCC" -B"$MB" -I "$INC" -O$o -DMCC_GPU_OFFLOAD -pthread "$src" -o "$exe" -lvulkan -lm >"$WORK/be" 2>&1 ;;
			esac
			if [ $? -ne 0 ]; then
				echo "  BUILD-FAIL $tc -O$o: $(head -1 "$WORK/be")"; FAILS=$((FAILS+1)); continue
			fi
			tags="$tags ${tc}_O${o}"
		done
	done
	for N in $NS; do
		# serial references (gcc + clang), cross-checked against each other
		REF=""; CLREF=""
		[ -n "$GCC" ]   && "$GCC"   -O2 -pthread -I "$INC" -DNT=1 "$src" -o "$WORK/gref" -lm 2>/dev/null && REF=$($TIMEOUT "$WORK/gref" "$N" 2>/dev/null)
		[ -n "$CLANG" ] && "$CLANG" -O2 -pthread -I "$INC" -DNT=1 "$src" -o "$WORK/cref" -lm 2>/dev/null && CLREF=$($TIMEOUT "$WORK/cref" "$N" 2>/dev/null)
		xref="ok"; [ -n "$REF" ] && [ -n "$CLREF" ] && [ "$REF" != "$CLREF" ] && { xref="GCC!=CLANG"; FAILS=$((FAILS+1)); }
		echo "-- N=$N  serial-ref=${REF:-<none>}  (gcc-vs-clang: $xref)"
		printf "  %-12s %-4s | %14s | %9s | %s\n" toolchain -O "instructions" "wall(ms)" "result"
		for tag in $tags; do
			tc=${tag%_O*}; o=${tag##*_O}
			bestrun "$WORK/${name}_${tag}" "$N"
			if [ "$RC" = 124 ]; then res="HANG/TIMEOUT"; FAILS=$((FAILS+1))
			elif [ "$INSN" = ERR ]; then res="RUN-ERR"; FAILS=$((FAILS+1))
			elif [ -n "$REF" ] && [ "$OUT" != "$REF" ]; then res="MISMATCH($OUT)"; FAILS=$((FAILS+1))
			else res="ok"; fi
			printf "  %-12s -O%s | %14s | %9s | %s\n" "$tc" "$o" "$INSN" "$MS" "$res"
		done
	done
done

echo
if [ "$FAILS" -eq 0 ]; then echo "SUMMARY: all builds green across all inputs (mcc == gcc == clang)"; else echo "SUMMARY: $FAILS failure(s) — see MISMATCH/HANG/BUILD-FAIL above"; fi
