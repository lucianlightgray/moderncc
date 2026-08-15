#!/bin/sh
set -e

# T-lin-10032: mccjit_pool_start caps a worker request above MCCJIT_POOL_MAX (64).
# The cap used to be silent. This cell asks for more than the cap through the
# baked embed-JIT constructor (--jit-threads passes the literal to
# mccjit_boot_swap_async, which calls mccjit_pool_start) and requires the
# diagnostic to appear -- a constant no code reads cannot fail, so the cap is
# checked by a compile-and-run of one program.

MCC=$1
BASE=$2
[ -n "$MCC" ] && [ -n "$BASE" ] || {
	echo "usage: run-poolcap.sh <mcc> <build-dir>" >&2
	exit 2
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/pool.c" <<'EOF'
int add(int a, int b) { return a + b; }
int main(void) { volatile int x = add(2, 3); return x == 5 ? 0 : 1; }
EOF

if ! "$MCC" -B"$BASE" -w -O1 --embed-jit --jit-threads 100 \
		"$WORK/pool.c" -o "$WORK/pool.exe" 2>"$WORK/build.err"; then
	echo "FAIL poolcap: the --embed-jit compile did not build" >&2
	cat "$WORK/build.err" >&2
	exit 1
fi

rc=0
MCC_JIT=1 "$WORK/pool.exe" 2>"$WORK/run.err" || rc=$?
if [ "$rc" != 0 ]; then
	echo "FAIL poolcap: the capped run exited $rc (expected 0) -- clamping to the" >&2
	echo "cap must not break the program" >&2
	cat "$WORK/run.err" >&2
	exit 1
fi

if ! grep -q "JIT worker pool capped at 64 (requested 100)" "$WORK/run.err"; then
	echo "FAIL poolcap: asked for 100 workers over a cap of 64 and got no" >&2
	echo "diagnostic -- the over-cap request was clamped silently" >&2
	cat "$WORK/run.err" >&2
	exit 1
fi

echo "poolcap: OK -- an over-cap worker request (100) is diagnosed and clamped to 64, program still correct"
