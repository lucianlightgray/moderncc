#!/bin/sh
# ast/inline-divguard -- the AST inliner's divisor-feed profitability guard.
#
# The strategy inliner (ast_inline_run) grafts every single-return scalar
# callee unconditionally. That is a net WIN when the callee owns the loop's
# expensive op (spectral_norm_barrier eval_A returns 1.0/d), but a net LOSS
# when a cheap integer callee is spliced into a divisor-bound loop
# (spectral_norm_forkjoin: v[j]/A(i,j), A returns int) -- inline ON was 55%
# slower there at FEWER instructions (IPC halved). MCC_AST_INLINE_DIVGUARD
# (default on) declines exactly the loss case: an integer-returning callee
# whose result is an operand of a '/' division. See DETAILS.md#t-lin-10440.
#
# This cell pins BOTH directions and stays anti-vacuous:
#   decline subject -- a pure-int callee feeding v[]/callee: guard drops it to
#     inline=0, while the same source WITHOUT the guard inlines it (>=1). If the
#     guard ever stops firing, guardON rises and the cell fails; if the subject
#     stops reaching the inliner, guardOFF falls to 0 and the vacuity check fails.
#   keep subject -- a double callee that owns 1.0/d (feeds a '*') plus an int
#     callee feeding '+': the guard must leave both untouched (guardON==guardOFF).
# And declining an inline must never change the computed result.
set -e

MCC=$1
SRCDIR=$2
WORK=$3

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: inline-divguard.sh <mcc> <srcdir> <workdir>" >&2
	exit 2
fi

mkdir -p "$WORK"

decl=$SRCDIR/tests/misc/inline_divguard_subject.c
keep=$SRCDIR/tests/misc/inline_divkeep_subject.c

icount() {
	MCC_STATS=strategy MCC_AST_INLINE_DIVGUARD=$2 "$MCC" -O2 -c "$1" \
		-o "$WORK/g$2.o" 2>&1 |
		sed -n 's/.*[^a-z]inline=\([0-9][0-9]*\).*/\1/p' | tail -1
}

runval() {
	MCC_AST_INLINE_DIVGUARD=$2 "$MCC" -O2 "$1" -o "$WORK/run$2" >/dev/null 2>&1
	"$WORK/run$2"
}

d_on=$(icount "$decl" 1)
d_off=$(icount "$decl" 0)
k_on=$(icount "$keep" 1)
k_off=$(icount "$keep" 0)
echo "decline: guardON=$d_on guardOFF=$d_off   keep: guardON=$k_on guardOFF=$k_off"

if [ "$d_on" != 0 ]; then
	echo "FAIL: divisor-fed int callee still inlined under guard (inline=$d_on, want 0)"
	exit 1
fi
if [ -z "$d_off" ] || [ "$d_off" -lt 1 ]; then
	echo "FAIL: vacuous -- callee never inlines even without the guard (inline=$d_off)"
	exit 1
fi
if [ "$k_on" != "$k_off" ]; then
	echo "FAIL: guard perturbed owner/non-division inlining ($k_on vs $k_off)"
	exit 1
fi
if [ -z "$k_on" ] || [ "$k_on" -lt 1 ]; then
	echo "FAIL: keep subject inlines nothing (inline=$k_on)"
	exit 1
fi

for s in "$decl" "$keep"; do
	on=$(runval "$s" 1)
	off=$(runval "$s" 0)
	if [ "$on" != "$off" ]; then
		echo "FAIL: result differs guard on/off for $s ('$on' vs '$off')"
		exit 1
	fi
done

echo "ast/inline-divguard OK: guard declines divisor-fed int callee (${d_off}->0), preserves owner/non-div inlining (${k_on}), result-invariant"
