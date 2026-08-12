#!/bin/sh
# --jit-always-gpu must not change a single verdict.
#
# The ladder is an equivalence oracle: it decides whether two ASTs agree over a
# swept input space. --jit-always-gpu moves that sweep onto the device. The
# device is therefore only ever allowed to be FASTER or to REFUSE -- never to
# answer differently, and never to change the object the compiler emits.
#
# Both arms run the same census over the same subject. The CPU arm arms the
# ladder by env; the GPU arm arms it with the flag. Every semantic field of the
# [ladder-self] and [ladder-cross] panels must match, and the emitted objects
# must be byte-identical. Only the timing fields may move, so they are stripped.
#
# Exits 77 when no device is present: a parity claim needs two arms, and with
# `available=0` the GPU arm silently is the CPU arm and the comparison is
# vacuous.
set -e
MCC="$1"
BD="$2"
SUBJ="$3"
WORK="$4"
OPT="${5:--O4}"
shift 5 2>/dev/null || true
EXTRA="$*"

# __DATE__/__TIME__ are in the corpus (tests/diff/parts/s6_10_4.h), so two
# compiles that straddle a second boundary differ in .data with identical code.
# The census is slow enough to make that routine, which is what used to read as
# "the census is non-deterministic". Pin the clock and the comparison is real.
SOURCE_DATE_EPOCH=1000000000
export SOURCE_DATE_EPOCH

mkdir -p "$WORK"
CPUO="$WORK/agp_cpu.o"
GPUO="$WORK/agp_gpu.o"
CPUE="$WORK/agp_cpu.err"
GPUE="$WORK/agp_gpu.err"

MCC_AST_EVAL_LADDER=1 MCC_AST_EVAL_LADDER_CENSUS=1 \
	"$MCC" -B"$BD" $EXTRA "$OPT" -w -c "$SUBJ" -o "$CPUO" 2> "$CPUE" || {
	echo "FAIL: the CPU arm did not compile"; sed -n '1,20p' "$CPUE"; exit 1; }

"$MCC" -B"$BD" $EXTRA "$OPT" -w --jit-always-gpu -c "$SUBJ" -o "$GPUO" 2> "$GPUE" || {
	echo "FAIL: the --jit-always-gpu arm did not compile"; sed -n '1,20p' "$GPUE"; exit 1; }

if ! grep -q 'available=1' "$GPUE"; then
	echo "SKIP: no GPU device available; the two arms would be the same arm"
	exit 77
fi

RUNGS=$(sed -n 's/.*rungs=\([0-9]*\).*/\1/p' "$GPUE" | head -1)
[ -n "$RUNGS" ] || RUNGS=0
if [ "$RUNGS" -eq 0 ]; then
	echo "FAIL: the device was armed but ran no rungs, so nothing was compared"
	grep 'ladder-gpu' "$GPUE" || true
	exit 1
fi

# Semantic panel lines only: secs=/us-per-pair= are wall-clock and must move.
panel() { grep -hE '^\[ladder-(self|cross)\]' "$1" | grep -v 'secs='; }
panel "$CPUE" > "$WORK/agp_cpu.panel"
panel "$GPUE" > "$WORK/agp_gpu.panel"

if [ ! -s "$WORK/agp_cpu.panel" ]; then
	echo "FAIL: the CPU arm produced no ladder panel; the census did not run"
	exit 1
fi

if ! diff -u "$WORK/agp_cpu.panel" "$WORK/agp_gpu.panel" > "$WORK/agp.diff"; then
	echo "FAIL: the device answered differently from the CPU"
	cat "$WORK/agp.diff"
	exit 1
fi

# The object check discriminates only if the CPU arm is reproducible, so prove
# it here rather than assuming it: compile the CPU arm a second time and require
# the two to agree before comparing against the device.
CPUO2="$WORK/agp_cpu2.o"
MCC_AST_EVAL_LADDER=1 MCC_AST_EVAL_LADDER_CENSUS=1 \
	"$MCC" -B"$BD" $EXTRA "$OPT" -w -c "$SUBJ" -o "$CPUO2" 2>/dev/null || true
if ! cmp -s "$CPUO" "$CPUO2"; then
	echo "FAIL: two CPU-arm compiles of the same subject differ, with"
	echo "      SOURCE_DATE_EPOCH pinned. That is a reproducibility defect in its"
	echo "      own right and it makes the device object comparison unattributable."
	exit 1
fi
if ! cmp -s "$CPUO" "$GPUO"; then
	echo "FAIL: --jit-always-gpu changed the emitted object"
	exit 1
fi

PAIRS=$(sed -n 's/^\[ladder-self\] pairs=\([0-9]*\).*/\1/p' "$GPUE" | head -1)
PTS=$(sed -n 's/.*points=\([0-9]*\).*/\1/p' "$GPUE" | head -1)
echo "PASS: device and CPU agree on every verdict"
echo "  pairs=$PAIRS points=$PTS rungs=$RUNGS, objects byte-identical"
grep 'ladder-gpu' "$GPUE" | sed 's/^/  /'
exit 0
