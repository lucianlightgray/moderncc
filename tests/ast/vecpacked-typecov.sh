#!/bin/sh
# ast/vecpacked-typecov -- 16-byte vector ops emit packed SSE, not a scalar loop.
#
# gen_vector_op scalarized GNU-vector arithmetic and comparisons into a per-lane
# loop. T-lin-10448 + T-lin-10425 added packed fast-paths (x86_64 && !PE,
# optimize>=1): x86_64_vec16_packed_op (float/double arith), _iop (integer
# arith), _fcmp (float/double compare), _icmp (integer compare). This cell is
# the regression guard: at -O1 the subject must emit addps/mulpd/paddd/pand/
# cmpltps/pcmpgtd (one each), the result must equal -O0 (== gcc), and -O0 must
# stay scalar (the fast-path is optimize>=1). See DETAILS.md#t-lin-10420-child-decomposition.
set -e

MCC=$1
SRCDIR=$2
WORK=$3

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: vecpacked-typecov.sh <mcc> <srcdir> <workdir>" >&2
	exit 2
fi

mkdir -p "$WORK"
src=$SRCDIR/tests/misc/vecpacked_subject.c
gold="10 31 96 20 1660 -2147483648 0 -1 0 0"

# x86_64-only: the packed fast-paths are MCC_TARGET_X86_64 && !MCC_TARGET_PE.
case $("$MCC" -dumpmachine 2>/dev/null) in
	x86_64-*|amd64-*) ;;
	*) echo "ast/vecpacked-typecov: skip (packed vector fast-path is x86_64 native only)"; exit 77 ;;
esac
case $("$MCC" -dumpmachine 2>/dev/null) in
	*-win32|*mingw*|*windows*) echo "ast/vecpacked-typecov: skip (PE falls back to scalar)"; exit 77 ;;
esac

"$MCC" -O0 "$src" -o "$WORK/r0" >/dev/null 2>&1
"$MCC" -O1 "$src" -o "$WORK/r1" >/dev/null 2>&1
r0=$("$WORK/r0")
r1=$("$WORK/r1")

if [ "$r0" != "$gold" ]; then
	echo "FAIL: -O0 output '$r0' != golden '$gold'"
	exit 1
fi
if [ "$r1" != "$r0" ]; then
	echo "FAIL: -O1 output '$r1' != -O0 '$r0' -- a packed vector op miscompiled"
	exit 1
fi

"$MCC" -O1 -c "$src" -o "$WORK/r1.o" >/dev/null 2>&1
missing=""
for insn in addps mulpd paddd pand pmuludq punpckldq cmpltps pcmpgtd; do
	if ! objdump -d "$WORK/r1.o" 2>/dev/null | grep -qw "$insn"; then
		missing="$missing $insn"
	fi
done
if [ -n "$missing" ]; then
	echo "FAIL: -O1 did not emit packed SSE (missing:$missing) -- vector op scalarized"
	exit 1
fi

"$MCC" -O0 -c "$src" -o "$WORK/r0.o" >/dev/null 2>&1
if objdump -d "$WORK/r0.o" 2>/dev/null | grep -qwE 'addps|mulpd|paddd|pcmpgtd'; then
	echo "FAIL: -O0 emitted packed SSE -- fast-path is not optimize>=1 gated (o0 drift)"
	exit 1
fi

echo "ast/vecpacked-typecov OK: packed SSE for float/double/int arith + compares (result '$r1' == -O0 == gcc), -O0 scalar"
