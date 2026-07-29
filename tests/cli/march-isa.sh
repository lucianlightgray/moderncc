#!/bin/sh
# -march= and -print-isa.
#
# -march= used to be matched and thrown away, so `mcc -march=x86-64-v3` was a
# silent no-op and an unknown value was accepted just as quietly. Two things
# are pinned here: the flag reports what it resolved to, and it CHANGES CODEGEN
# rather than only bookkeeping.
#
# The codegen assertion is the one that matters. roundsd is SSE4.1, above
# mcc's SSE2 baseline, so it must appear only when the user has said the target
# has SSE4.1 -- and in particular -O4 must NOT introduce it. "-O4 means run
# every optimizer, not raise the required ISA" is the rule this cell exists to
# hold; breaking it makes -O4 output demand a CPU nothing recorded.
#
# Usage: march-isa.sh <mcc> <mccbase> <workdir>
set -e

MCC=$1
BASE=$2
WORK=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] || {
	echo "usage: march-isa.sh <mcc> <mccbase> <workdir>" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
command -v objdump >/dev/null 2>&1 || { echo "SKIP: objdump not found"; exit 77; }

# Probe for an x86-64 mcc by asking whether it accepts an x86-64 level, NOT by
# pattern-matching the DEFAULT level: the default is `native`, so matching on
# "level: x86-64" made this whole cell SKIP (=pass) the moment the default
# flipped, which is exactly how a test stops testing anything without saying so.
case $("$MCC" -march=x86-64 -print-isa 2>/dev/null | head -1) in
	"level: x86-64") ;;
	*) echo "SKIP: not an x86-64 mcc"; exit 77 ;;
esac

rm -rf "$WORK"
mkdir -p "$WORK"
rc=0

cat >"$WORK/r.c" <<'EOF'
extern double floor(double), ceil(double), trunc(double);
double f(double x) { return floor(x) + ceil(x) + trunc(x); }
EOF

isa() { "$MCC" -B"$BASE" $1 -print-isa 2>&1; }

# 1. The default is NATIVE -- mcc targets the machine it runs on. Asserted as
#    "the default equals an explicit -march=native", not against a fixed feature
#    list, because the correct answer differs per host and hardcoding this
#    machine's would make the cell a lie on any other one.
if [ "$(isa '' | head -1)" = "level: native" ] &&
	 [ "$(isa '')" = "$(isa -march=native)" ]; then
	echo "PASS: the default is native and matches an explicit -march=native"
else
	echo "FAIL: default -print-isa is '$(isa '' | tr '\n' ' ')'"
	rc=1
fi

# 2. Named levels resolve, and each is a superset of the one below.
for lv in x86-64-v2 x86-64-v3 x86-64-v4; do
	if [ "$(isa "-march=$lv" | head -1)" = "level: $lv" ]; then :; else
		echo "FAIL: -march=$lv did not resolve"
		rc=1
	fi
done
isa -march=x86-64-v2 | grep -q 'sse4\.1' || { echo "FAIL: v2 lacks sse4.1"; rc=1; }
isa -march=x86-64-v3 | grep -q 'avx2'    || { echo "FAIL: v3 lacks avx2"; rc=1; }
isa -march=x86-64-v4 | grep -q 'avx512f' || { echo "FAIL: v4 lacks avx512f"; rc=1; }
[ $rc = 0 ] && echo "PASS: named levels resolve with the expected features"

# 3. An unknown value must be REJECTED. Silently accepting it is the bug.
if "$MCC" -B"$BASE" -march=definitely-not-a-cpu -print-isa >"$WORK/e" 2>&1; then
	echo "FAIL: -march=definitely-not-a-cpu was accepted silently"
	rc=1
elif grep -q "unknown -march=" "$WORK/e"; then
	echo "PASS: an unknown -march= value is diagnosed and fails"
else
	echo "FAIL: unknown -march= failed for the wrong reason:"
	sed 's/^/  /' "$WORK/e" | head -2
	rc=1
fi

# 4. -mtune/-mcpu pick a scheduling model mcc does not have; they must stay
#    accepted so existing command lines keep working.
if "$MCC" -B"$BASE" -mtune=haswell -mcpu=generic -O2 -c "$WORK/r.c" \
		-o "$WORK/t.o" 2>"$WORK/e2"; then
	echo "PASS: -mtune/-mcpu still accepted"
else
	echo "FAIL: -mtune/-mcpu rejected:"
	sed 's/^/  /' "$WORK/e2" | head -2
	rc=1
fi

# 5. Codegen follows the ISA, in both directions.
rounds() {
	"$MCC" -B"$BASE" -O2 -fno-math-errno $1 -c "$WORK/r.c" -o "$WORK/r.o" 2>/dev/null
	objdump -d "$WORK/r.o" | grep -cE 'roundsd|roundss' || true
}
# The baseline must be named EXPLICITLY here. The default is `native`, so on an
# SSE4.1 host the no-flag build correctly DOES emit roundsd; asserting against
# the implicit default would be asserting the host, not the flag.
if [ "$(rounds '-march=x86-64')" = "0" ]; then
	echo "PASS: an explicit baseline -march emits no roundsd"
else
	echo "FAIL: -march=x86-64 emitted roundsd ('$(rounds '-march=x86-64')')"
	rc=1
fi
if [ "$(rounds '-march=x86-64-v2')" -ge 1 ]; then
	echo "PASS: -march=x86-64-v2 enables roundsd"
else
	echo "FAIL: -march=x86-64-v2 emitted no roundsd, so the mask is not reaching codegen"
	rc=1
fi

# 6. The rule this whole axis exists for: optimization effort must not raise
#    the ISA floor.
"$MCC" -B"$BASE" -O4 -fno-math-errno -march=x86-64 -c "$WORK/r.c" -o "$WORK/r4.o" 2>/dev/null
if [ "$(objdump -d "$WORK/r4.o" | grep -cE 'roundsd|roundss' || true)" = "0" ]; then
	echo "PASS: -O4 does not raise the ISA above the -march it was given"
else
	echo "FAIL: -O4 emitted roundsd at the baseline -march; optimization effort"
	echo "  must not change which CPUs the output runs on"
	rc=1
fi

# 7. -march= must REJECT an unknown value on EVERY target, not just this one.
#    x86 diagnosing `-march=nonsense` while arm64 shrugged was worse than either
#    alone, and a silent accept is the exact bug -march= had before it meant
#    anything.
for lv in armv8-a rv64gc armv7-a i686; do
	if "$MCC" -B"$BASE" -march="$lv" -print-isa >/dev/null 2>&1; then
		echo "NOTE: this target also accepts -march=$lv"
	fi
done
if "$MCC" -B"$BASE" -march=definitely-not-a-level -print-isa >/dev/null 2>&1; then
	echo "FAIL: an unknown -march= was accepted silently"
	rc=1
else
	echo "PASS: an unknown -march= is rejected"
fi

exit $rc
