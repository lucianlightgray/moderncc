#!/bin/sh
# --embed-jit through a CROSS osx compiler, and through the on-disk Mach-O
# archive reader rather than the bin2c'd blob.
#
# Two things had no coverage before this cell:
#   * no osx triple exercised --embed-jit at all. The native host mcc bakes
#     mccjit_blob.c and takes the in-memory path, so mcc_add_file() on a real
#     Mach-O archive -- the fallback a cross mcc uses, since no blob is baked
#     for a cross target -- was only ever tested against the synthetic fixture
#     in macho-archive.
#   * the produced image was never RUN with the JIT on. --embed-jit output that
#     is correct at MCC_JIT=0 and faults at MCC_JIT=1 is the exact shape the
#     arm64 BRANCH26 sibling-call defect produced, and nothing caught it.
#
# Skips when the engine archive next to the cross mcc is not this target's
# cputype -- a Linux host building mcc-arm64-osx has an x86_64 ELF engine and
# there is nothing to bake.
#
# Usage: embedjit.sh <cross-mcc> <mccbase> <workdir> <arch> [extra-flags...]
set -e

MCC=$1
BASE=$2
WORK=$3
ARCH=$4
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] && [ -n "$ARCH" ] || {
	echo "usage: embedjit.sh <cross-mcc> <mccbase> <workdir> <arch> [extra-flags...]" >&2
	exit 2
}
shift 4

[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: running a Mach-O image needs a Darwin host"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no cross mcc at $MCC"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/hot.c" <<'EOF'
extern int printf(const char *, ...);
int hot(int n){ int s = 0, i; for (i = 0; i < n; i++) s += i % 7; return s; }
int main(void){ int t = 0, k; for (k = 0; k < 200; k++) t += hot(500); printf("%d\n", t); return 0; }
EOF
EXPECT=298800

if ! "$MCC" -B"$BASE" --embed-jit --jit-functions=hot -O2 "$@" \
	"$WORK/hot.c" -o "$WORK/out" 2>"$WORK/err"; then
	if grep -q 'wrong cputype' "$WORK/err"; then
		echo "SKIP: the engine archive beside $MCC is not $ARCH ($(head -1 "$WORK/err"))"
		exit 77
	fi
	echo "FAIL: --embed-jit bake failed:"
	sed 's/^/  /' "$WORK/err" | head -3
	exit 1
fi

got=$(lipo -info "$WORK/out" 2>/dev/null | sed 's/.*: //')
case "$got" in
*"$ARCH"*) echo "PASS: baked a $ARCH Mach-O image" ;;
*) echo "FAIL: expected a $ARCH image, lipo says '$got'"; exit 1 ;;
esac

rc=0
for jit in 0 1; do
	out=$(MCC_JIT=$jit "$WORK/out" 2>&1) || {
		echo "FAIL: MCC_JIT=$jit exited $? -- the baked engine faults at boot"
		echo "  $(printf '%s' "$out" | head -2)"
		rc=1
		continue
	}
	if [ "$out" != "$EXPECT" ]; then
		echo "FAIL: MCC_JIT=$jit printed '$out', want '$EXPECT'"
		rc=1
	else
		echo "PASS: MCC_JIT=$jit runs and prints $EXPECT"
	fi
done

exit $rc
