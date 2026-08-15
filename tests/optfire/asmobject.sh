#!/bin/sh
set -u

MCC=${1:-}
BDIR=${2:-}
IDIR=${3:-}
WORK=${4:-}
SRC=${5:-}
MODE=${6:-}

[ -n "$MCC" ] && [ -n "$BDIR" ] && [ -n "$IDIR" ] && [ -n "$WORK" ] && [ -n "$SRC" ] || {
	echo "usage: asmobject.sh <mcc> <bdir> <idir> <work> <src.c> [known-positive]" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP asmobject: no mcc at $MCC"; exit 77; }
[ -f "$SRC" ] || { echo "SKIP asmobject: no subject at $SRC"; exit 77; }

# The invariant: MCC_REPLAY_IR must not change the object. optfire/asmreplay
# already runs asm bodies under replay, but it compares what the program
# PRINTS, and none of the ways replay corrupts an inline-asm body change that
# -- a duplicated .data emission, a leaked undefined symbol and a redefined
# label are all invisible to stdout. This cell compares the object instead,
# which is the artefact the A/B exists to defend.

rm -rf "$WORK"
mkdir -p "$WORK/plain" "$WORK/replay"

KPDEF=
if [ "$MODE" = known-positive ]; then
	KPDEF=-DASMOBJ_KP
fi

# Same output basename in two directories: an object can encode the name it was
# given, and a difference manufactured by the harness is not a finding.
if ! "$MCC" -B"$BDIR" -I"$IDIR" -w -O0 -c -o "$WORK/plain/a.o" "$SRC" \
		>"$WORK/plain.log" 2>&1; then
	echo "FAIL asmobject: the plain -O0 compile did not build"
	head -4 "$WORK/plain.log" | sed 's/^/  /'
	exit 1
fi

# An object with no inline asm in it cannot disagree about inline asm. Refuse a
# subject whose asm was compiled out -- the whole file is #if'd off on non-x86.
if command -v nm >/dev/null 2>&1; then
	if nm "$WORK/plain/a.o" 2>/dev/null | grep -q 'asmobj_not_x86'; then
		echo "SKIP asmobject: the subject's asm is x86-only"
		exit 77
	fi
	if ! nm "$WORK/plain/a.o" 2>/dev/null | grep -q 'asmobj_named'; then
		echo "FAIL asmobject: the plain object carries no asm-defined symbol, so"
		echo "  this comparison has no subject and cannot report agreement"
		exit 1
	fi
fi

ran=0
diffs=0

check() {
	lbl=$1
	shift
	ran=$((ran + 1))
	rm -rf "$WORK/replay"
	mkdir -p "$WORK/replay"
	if ! env "$@" "$MCC" -B"$BDIR" -I"$IDIR" -w -O0 $KPDEF -c \
			-o "$WORK/replay/a.o" "$SRC" >"$WORK/r$ran.log" 2>&1; then
		echo "FAIL asmobject [$lbl]: the subject did not build under replay"
		grep -v '^\[rir-' "$WORK/r$ran.log" | head -4 | sed 's/^/  /'
		diffs=$((diffs + 1))
		return
	fi
	if cmp -s "$WORK/plain/a.o" "$WORK/replay/a.o"; then
		return
	fi
	diffs=$((diffs + 1))
	echo "DIFF asmobject [$lbl]: replay changed the object"
	echo "  plain  $(wc -c < "$WORK/plain/a.o") bytes"
	echo "  replay $(wc -c < "$WORK/replay/a.o") bytes"
	if command -v nm >/dev/null 2>&1; then
		nm "$WORK/plain/a.o" 2>/dev/null | sort > "$WORK/plain.nm"
		nm "$WORK/replay/a.o" 2>/dev/null | sort > "$WORK/replay.nm"
		diff -u "$WORK/plain.nm" "$WORK/replay.nm" | head -12 | sed 's/^/  /'
	fi
}

check "replay+force" MCC_REPLAY_IR=1 MCC_RIR_FORCE=1
check "replay" MCC_REPLAY_IR=1

WANT=2
[ "$ran" -eq "$WANT" ] || {
	echo "FAIL asmobject: $ran of $WANT configurations ran"
	exit 1
}

if [ "$MODE" = known-positive ]; then
	[ "$diffs" -gt 0 ] || {
		echo "FAIL asmobject-known-positive: every configuration produced the same"
		echo "  object while the replay side carried $KPDEF, which adds a .data"
		echo "  emission the plain side does not have. A cell that cannot see that"
		echo "  cannot see the defect it exists for"
		exit 1
	}
	echo "asmobject-known-positive: OK -- the planted difference was caught"
	exit 0
fi

[ "$diffs" -eq 0 ] || exit 1
echo "asmobject: OK -- MCC_REPLAY_IR leaves the object of $ran configurations unchanged"
exit 0
