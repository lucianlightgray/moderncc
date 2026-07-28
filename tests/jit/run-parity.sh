#!/bin/sh
# Host `-run` JIT parity: MCC_JIT=1 must produce byte-identical output to
# MCC_JIT=0. That is a P0 bar item, and the existing run-parity-<arch> cells
# only cover the CROSS triples (riscv64/arm/arm64) with two programs, and skip
# entirely when cross tooling is absent -- so on an ordinary host machine
# nothing gates it at all.
#
# Each program is run under three configurations:
#   * MCC_JIT=0                      -- the reference
#   * MCC_JIT=1                      -- default admission
#   * MCC_JIT=1 + widened admission  -- MCC_JIT_PURITY_NOESCAPE/LAZY/SEARCH
#
# The widened leg matters because default admission REFUSES almost everything
# (see TODO "KGC verification refuses almost everything"), so a parity check
# that only runs the default gate is nearly vacuous -- it compares a JIT that
# installed no variant against no JIT. MCC_JIT_NEARMATCH is default-ON and by
# design keeps a variant that mismatches the baseline on a small input set, so
# the widened leg is exactly where a parity violation would show up.
#
# NON-VACUITY IS ASSERTED, not assumed: across the corpus the widened leg must
# INSTALL at least one variant (`mccjit-lazy[install]`), or this cell would be
# comparing a JIT that swapped nothing against no JIT at all. Measured when this
# landed: the default gate installs 0 for every program, the widened gate
# installs 1 each for int_mod and fp_div_accum, and 0 for float_narrow (a
# `float` signature is in neither the GP nor the FP verified set). So the
# requirement is corpus-wide rather than per-program.
#
# Do NOT count the token `verified`: the refusal message "signature not in the
# verified GP-int set" contains it, so grepping for it counts REFUSALS and the
# guard passes while nothing is verified. That mistake was made and caught here.
#
# Usage: run-parity.sh <mcc> <mccbase> <srcdir>
set -e

MCC=$1
BASE=$2
SRC=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$SRC" ] || {
	echo "usage: run-parity.sh <mcc> <mccbase> <srcdir>" >&2
	exit 2
}

WIDE="MCC_JIT_PURITY_NOESCAPE=1 MCC_JIT_LAZY=1 MCC_JIT_SEARCH=1"
rc=0
n=0
installs=0
for src in "$SRC"/*.c; do
	[ -f "$src" ] || continue
	name=$(basename "$src" .c)
	n=$((n + 1))

	ref=$(MCC_JIT=0 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: reference run (MCC_JIT=0) failed"
		rc=1
		continue
	}
	got=$(MCC_JIT=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: MCC_JIT=1 run failed"
		rc=1
		continue
	}
	if [ "$ref" != "$got" ]; then
		echo "FAIL $name: MCC_JIT=1 output differs from MCC_JIT=0"
		echo "  jit0: $ref"
		echo "  jit1: $got"
		rc=1
		continue
	fi

	wide=$(env $WIDE MCC_JIT=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: widened-admission run failed"
		rc=1
		continue
	}
	if [ "$ref" != "$wide" ]; then
		echo "FAIL $name: widened admission changed the output"
		echo "  jit0: $ref"
		echo "  wide: $wide"
		echo "  this is the MCC_JIT_NEARMATCH parity hazard; see TODO KGC section"
		rc=1
		continue
	fi

	nv=$(env $WIDE MCC_JIT=1 MCC_JIT_VERBOSE=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>&1 |
		grep -c 'mccjit-lazy\[install\]' || true)
	installs=$((installs + nv))
	echo "PASS $name: jit0 == jit1 == widened ($nv variant(s) installed)"
done

[ "$n" -gt 0 ] || { echo "FAIL: no programs found in $SRC"; exit 1; }
if [ "$installs" -lt 1 ]; then
	echo "FAIL: the widened leg installed NO variants across $n programs -- this"
	echo "  cell is comparing a JIT that swapped nothing against no JIT at all."
	echo "  Admission narrowed, or MCC_JIT_LAZY stopped installing; fix before"
	echo "  trusting a green result here."
	rc=1
fi
exit $rc
