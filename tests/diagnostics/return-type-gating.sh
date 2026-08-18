#!/bin/sh
# T-mac-30052 slice-1: the reachable-end-of-non-void-function diagnostic
# ("function might return no value") was emitted via a plain mcc_warning(),
# not mcc_warning_c(warn_return_type) like its return-statement siblings
# (mccgen.c:16064/16070), so it ignored the return-type flag entirely:
# -Wno-return-type could not silence it and -Wno-error=return-type could not
# downgrade it. warn_return_type is in mcc's default-error set (libmcc.c:848),
# so the whole family (incl. "void function returns a value", "'return' with no
# value") is a hard ERROR by default; this fix makes fall-off-end consistent —
# error by default, silenced by -Wno-return-type, downgraded by
# -Wno-error=return-type. Shell cell (not tests/exec) so it does not grow the
# walked coverage corpora.
set -u

MCC="$1"
WORK="$2"

mkdir -p "$WORK" 2>/dev/null || exit 2
cd "$WORK" || exit 2
[ -x "$MCC" ] || exit 77

cat > f.c <<'EOF'
int falls_off(int x) {
	if (x)
		return 1;
	/* no return here: reachable end of a non-void function */
}
int main(void) { return 0; }
EOF

fail() { echo "FAIL: $1" >&2; exit 1; }

# 1. default: a hard error, consistent with the return-type family (was broken:
#    an ungated warning, exit 0)
out=$("$MCC" -c f.c -o f.o 2>&1); rc=$?
[ "$rc" -ne 0 ] || fail "default should ERROR (return-type family is error-by-default); rc=$rc: $out"
echo "$out" | grep -q "might return no value" || fail "default error text should mention the diagnostic; got: $out"

# 2. -Wno-return-type must SILENCE it entirely (was broken: fired anyway)
out=$("$MCC" -Wno-return-type -c f.c -o f.o 2>&1); rc=$?
[ "$rc" -eq 0 ] || fail "-Wno-return-type compile should succeed; rc=$rc: $out"
echo "$out" | grep -q "might return no value" && fail "-Wno-return-type should suppress it; got: $out"

# 3. -Wno-error=return-type must DOWNGRADE it to a warning (was broken: stayed an
#    ungated warning regardless, i.e. the flag was ignored)
out=$("$MCC" -Wno-error=return-type -c f.c -o f.o 2>&1); rc=$?
[ "$rc" -eq 0 ] || fail "-Wno-error=return-type should downgrade to a warning (rc 0); rc=$rc: $out"
echo "$out" | grep -q "might return no value" || fail "-Wno-error=return-type should still warn; got: $out"

echo "PASS: fall-off-end diagnostic honours -Wno-return-type and -Wno-error=return-type, consistent with the return-type family"
exit 0
