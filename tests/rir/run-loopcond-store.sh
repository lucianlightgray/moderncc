#!/bin/sh
set -e

# T-lin-10018: a for-condition store `for (pp=list; (p=*pp); pp=nn)` was never
# emitted as a statement under the replay producer, so `p` stayed uninitialised
# and the loop body dereferenced garbage -- a SIGSEGV, not a byte difference,
# that took down the JIT self-host. The fix holds the loop-condition store in
# the condition prefix and drops the hold at body/incr (d76e5384 for `for`,
# 545ffdb0 for/while, 82f255d8 do-while). This cell is the reduced reproducer
# as a regression: the replay-producer path with the fallback off must produce
# the same answer as the parser, not crash.

MCC=$1
BASE=$2
[ -n "$MCC" ] && [ -n "$BASE" ] || {
	echo "usage: run-loopcond-store.sh <mcc> <build-dir>" >&2
	exit 2
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/pu.c" <<'EOF'
#include <stdio.h>
struct node { struct node *next; int val; };
static void ptr_unlink(void *list, void *e, unsigned next) {
	void **pp, **nn, *p;
	for (pp = list; !!(p = *pp); pp = nn) {
		nn = (void *)((char *)p + next);
		if (p == e) { *pp = *nn; break; }
	}
}
int main(void) {
	struct node c = {0, 3}, b = {&c, 2}, a = {&b, 1};
	struct node *head = &a;
	ptr_unlink(&head, &b, 0);          /* unlink b: head -> a -> c */
	printf("%d %d\n", head->val, head->next->val);
	return 0;
}
EOF

# Reference: the parser path prints the right answer.
if ! "$MCC" -B"$BASE" -w -O0 "$WORK/pu.c" -o "$WORK/ref" 2>"$WORK/ref.err"; then
	echo "FAIL loopcond-store: the reference (-O0) compile did not build" >&2
	cat "$WORK/ref.err" >&2
	exit 1
fi
ref=$("$WORK/ref" 2>&1) || {
	echo "FAIL loopcond-store: the reference binary did not run" >&2
	exit 1
}

# Subject: replay is a producer at -O1+, and -fno-replay-fallback removes the
# net that used to hide the miscompile. If the condition store is not held, the
# body dereferences an uninitialised `p` and this crashes.
if ! env MCC_REPLAY_IR=2 MCC_FORCE_REPLAY=1 \
		"$MCC" -B"$BASE" -w -O1 -fno-replay-fallback "$WORK/pu.c" \
		-o "$WORK/sub" 2>"$WORK/sub.err"; then
	echo "FAIL loopcond-store: the replay-producer compile did not build" >&2
	cat "$WORK/sub.err" >&2
	exit 1
fi
sub=$("$WORK/sub" 2>&1)
rc=$?

if [ "$rc" != 0 ]; then
	echo "FAIL loopcond-store: the replay-producer binary exited $rc (the" >&2
	echo "for-condition store was not held, so p is uninitialised)" >&2
	echo "  ref: [$ref]  sub: [$sub]" >&2
	exit 1
fi
if [ "$sub" != "$ref" ]; then
	echo "FAIL loopcond-store: replay-producer output [$sub] != parser [$ref]" >&2
	exit 1
fi

echo "loopcond-store: OK -- the held for-condition store replays faithfully ([$sub])"
