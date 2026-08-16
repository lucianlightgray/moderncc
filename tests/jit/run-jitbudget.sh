#!/bin/sh
set -e

# T-lin-10393 slice 1: the JIT/GPU resource-budget args resolve to the right values.
#   --jit-cpu-budget=N%  -> JIT worker threads = round(nproc * N%)   [percent-of-hardware]
#   --jit-conservative   -> 50% on both the CPU pool and the GPU VRAM budget
#   an explicit --jit-cpu-budget overrides the --jit-conservative preset for its axis
#   --jit-gpu-budget=N%  -> GPU VRAM cap %      --jit-gpu-devices=N -> held-device cap
#   =auto is accepted but errors (live-load adaptation is slice 2, not built)
# The resolved budget is observable via MCC_JIT_BUDGET_DEBUG (no device needed, so
# this checks the arg->budget math on every host; the GPU-side capping BEHAVIOR that
# reads mcc_gpu_vram_budget_pct / mcc_gpu_max_devices is a device cell, slice 1b).

MCC=$1
BASE=$2
[ -n "$MCC" ] && [ -n "$BASE" ] || {
	echo "usage: run-jitbudget.sh <mcc> <build-dir>" >&2
	exit 2
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
echo 'int main(void){return 0;}' > "$WORK/hb.c"

# host CPU count + rounded fractions the tool should compute
NP=$(nproc 2>/dev/null || echo 1)
half=$(( (NP * 50 + 50) / 100 )); [ "$half" -lt 1 ] && half=1; [ "$half" -gt 64 ] && half=64
quarter=$(( (NP * 25 + 50) / 100 )); [ "$quarter" -lt 1 ] && quarter=1; [ "$quarter" -gt 64 ] && quarter=64

budget() {   # echo the "jit-budget:" line for the given args
	MCC_JIT_BUDGET_DEBUG=1 "$MCC" -B"$BASE" -w -c "$WORK/hb.c" -o "$WORK/hb.o" "$@" 2>&1 |
		grep "jit-budget:" || true
}

want() {   # want <label> <expected-substring> <actual>
	case "$3" in
		*"$2"*) ;;
		*) echo "FAIL jitbudget: $1 -- expected '$2' in: $3" >&2; exit 1 ;;
	esac
}

got=$(budget --jit-cpu-budget=50%)
want "cpu 50%% => $half workers" "cpu-workers=$half " "$got"

got=$(budget --jit-conservative)
want "conservative cpu"  "cpu-workers=$half "  "$got"
want "conservative gpu"  "gpu-vram-pct=50 "    "$got"

got=$(budget --jit-conservative --jit-cpu-budget=25%)
want "override cpu"  "cpu-workers=$quarter "  "$got"
want "override keeps gpu preset"  "gpu-vram-pct=50 "  "$got"

got=$(budget --jit-gpu-devices=2 --jit-gpu-budget=75%)
want "gpu-budget"   "gpu-vram-pct=75 "   "$got"
want "gpu-devices"  "gpu-devices=2"      "$got"

# =value and space-separated both accepted (modelled on --jit-threads)
got=$(budget --jit-cpu-budget 50%)
want "space-separated arg"  "cpu-workers=$half "  "$got"

# CPU auto: sizes the pool to idle capacity (nproc - loadavg), a value in [1, nproc]
got=$(budget --jit-cpu-budget=auto)
want "cpu auto emits a budget" "cpu-workers=" "$got"
aw=$(printf '%s' "$got" | sed -n 's/.*cpu-workers=\([0-9][0-9]*\).*/\1/p')
if [ -z "$aw" ] || [ "$aw" -lt 1 ] || [ "$aw" -gt "$NP" ]; then
	echo "FAIL jitbudget: --jit-cpu-budget=auto workers '$aw' not in [1,$NP] (nproc-loadavg)" >&2
	exit 1
fi
# GPU auto is not yet implemented (live VRAM-budget detection is a later slice)
err=$(MCC_JIT_BUDGET_DEBUG=1 "$MCC" -B"$BASE" -w -c "$WORK/hb.c" -o "$WORK/hb.o" --jit-gpu-budget=auto 2>&1 || true)
want "gpu auto errors" "not yet implemented" "$err"

# a bad budget is rejected, not silently ignored
err=$("$MCC" -B"$BASE" -w -c "$WORK/hb.c" -o "$WORK/hb.o" --jit-gpu-budget=frob 2>&1 || true)
want "bad budget rejected" "wants a percentage" "$err"

# a bad device count is rejected
err=$("$MCC" -B"$BASE" -w -c "$WORK/hb.c" -o "$WORK/hb.o" --jit-gpu-devices=0 2>&1 || true)
want "zero devices rejected" "device count >= 1" "$err"

echo "jitbudget: OK -- cpu-budget => round(nproc*%) workers (nproc=$NP, 50%=$half, 25%=$quarter), conservative=50%% both, explicit override wins, gpu-budget/devices set, auto/bad rejected"
