#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/../../.."
ROOT=$(pwd)
MCC=${MCC:-$ROOT/cmake-def/mcc}
MB=${MB:-$ROOT/cmake-def}
D=$ROOT/tests/benchmarks/retype_superopt/jit
WIDE="MCC_JIT_PURITY_NOESCAPE=1 MCC_JIT_LAZY=1 MCC_JIT_SEARCH=1"

fails=0
for k in fits needs64; do
	src=$D/$k.c
	off=$(env $WIDE MCC_JIT=1 "$MCC" -B"$MB" -O2 -run "$src" 2>/dev/null)
	on=$(env $WIDE MCC_JIT=1 MCC_JIT_BLIND_RETYPE=1 "$MCC" -B"$MB" -O2 -run "$src" 2>/dev/null)
	if [ "$off" = "$on" ]; then safe=SAFE-IDENTICAL; else safe="MISMATCH($off/$on)"; fails=$((fails + 1)); fi
	echo "### $k   result=$off   blind-on-vs-off: $safe"
	env $WIDE MCC_STATS_FORCE=1 MCC_JIT=1 MCC_JIT_BLIND_RETYPE=1 "$MCC" -B"$MB" -O2 --stats=2 -run "$src" 2>&1 \
		| grep -iE 'blind retype:|blind promote:|separab|kgc hits|poison=' | sed 's/\x1b\[[0-9;]*[A-Za-z]//g; s/^\[2K//; s/^/  /'
done

if [ "$fails" -eq 0 ]; then
	echo "jit_blind: PASS (blind-retype on == off for all subjects)"
	exit 0
fi
echo "jit_blind: FAIL ($fails mismatch)"
exit 1
