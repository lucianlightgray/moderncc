#!/bin/sh
# Gate tools/xsuite-report.py against tests/xsuite/report-fixture.jsonl.
#
# xsuite-report.py summarizes a tools/xsuite.py results.jsonl. It publishes
# figures onto docs/TODO.md and was registered nowhere. It also cannot be run
# for real in this tree: xsuite.py needs external gcc and llvm checkouts, and no
# results.jsonl is committed. So what is gated here is the thing that IS in the
# tree -- the formatter -- against a fixture built to contain exactly the shapes
# that were being reported wrongly.
#
# The fixture carries, on purpose:
#   * a suite that ran at -O0 and never at -O3     (the phantom-row case)
#   * a suite that ran and passed NOTHING          (a genuine 0.0%, must survive)
#   * SKIP and REFSKIP rows                        (excluded from the denominator)
#   * FAILEXE / TIMEOUT / ICE / XPASS rows         (the tail sections)
#
# The invariants below are asserted by name rather than only by diffing the
# golden, so that a future reader can see what the cell is actually protecting
# and so a golden re-take cannot quietly discard them.
set -eu

TOOL=$1
FIXTURE=$2
GOLDEN=$3
WORK=$4

if [ ! -f "$TOOL" ]; then
	echo "SKIP: no $TOOL"
	exit 77
fi
command -v python3 >/dev/null 2>&1 || { echo "SKIP: no python3"; exit 77; }
if [ ! -f "$FIXTURE" ] || [ ! -f "$GOLDEN" ]; then
	echo "FAIL xsuite-report: missing fixture or golden"
	exit 1
fi

rm -rf "$WORK"
mkdir -p "$WORK"
cp "$FIXTURE" "$WORK/results.jsonl"

out=$WORK/report.txt
if ! python3 "$TOOL" "$WORK" >"$out" 2>&1; then
	echo "FAIL xsuite-report: the tool did not run"
	sed -e 's/^/  /' "$out" | head -30
	exit 1
fi

rc=0
fail() {
	echo "FAIL xsuite-report: $1"
	rc=1
}

# Anti-vacuity: an empty report would satisfy every "must not contain" check
# below by saying nothing at all.
if [ "$(wc -l <"$out" | tr -d ' ')" -lt 15 ]; then
	echo "FAIL xsuite-report: the report is too short to have summarized anything:"
	sed -e 's/^/  /' "$out"
	exit 1
fi
grep -q '^gcc.dg  *-O0 ' "$out" || fail "no gcc.dg -O0 row at all, so nothing was tallied"

# 1. The phantom row. only-O0.dg never ran at -O3; it must not be reported as a
#    suite that failed everything.
line=$(grep '^only-O0.dg  *-O3 ' "$out" || true)
[ -n "$line" ] || fail "the only-O0.dg -O3 pair vanished from the table entirely"
case $line in
*"not run"*) ;;
*) fail "only-O0.dg never ran at -O3 but is not marked 'not run': $line" ;;
esac
case $line in
*"0.0%"*) fail "only-O0.dg -O3 never ran and is still reported as 0.0%: $line" ;;
esac

# 2. A genuine zero must NOT be swallowed by that fix. torture -O0 ran one test
#    and it ICEd, so 0.0% is the truth and must still be printed.
line=$(grep '^torture  *-O0 ' "$out" || true)
case $line in
*"0.0%"*) ;;
*) fail "torture -O0 ran and passed nothing; a real 0.0% must still print: $line" ;;
esac

# 3. The rate is pass-of-admitted, and the report must say so rather than
#    labelling the column 'rate' and leaving it to be misread.
# Look at the header line specifically. Grepping the whole report would be
# satisfied by the legend below it, which mentions adm% either way.
head -1 "$out" | grep -q 'adm%' ||
	fail "the pass-rate COLUMN is not labelled adm%: $(head -1 "$out")"
grep -q 'SKIP/REFSKIP' "$out" ||
	fail "the report does not state that skips are excluded from the denominator"
grep -q 'RAISES adm%' "$out" ||
	fail "the report does not warn that skipping more tests raises the rate"

# 4. The -O3-only regression list is the tool's most-quoted output.
grep -q -- '-O3-only failures (pass at -O0): 1' "$out" ||
	fail "the -O3-only regression count is wrong or missing"
[ -f "$WORK/o3-only-failures.txt" ] || fail "o3-only-failures.txt was not written"
for f in failexe.txt timeout.txt ice.txt xpass.txt; do
	[ -f "$WORK/$f" ] || fail "$f was not written"
done

if [ "$rc" -ne 0 ]; then
	echo "  --- report was ---"
	sed -e 's/^/  /' "$out"
	exit 1
fi

if ! diff -u "$GOLDEN" "$out" >"$WORK/diff.txt"; then
	echo "FAIL xsuite-report: output differs from tests/xsuite/report-golden.txt"
	sed -e 's/^/  /' "$WORK/diff.txt" | head -40
	echo "  If the change is intended, re-take the golden with:"
	echo "    cp <workdir>/report.txt tests/xsuite/report-golden.txt"
	exit 1
fi

echo "xsuite-report: OK (fixture summarized, phantom-row and adm% invariants hold)"
exit 0
