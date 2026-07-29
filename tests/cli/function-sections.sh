#!/bin/sh
# -ffunction-sections: one `.text.<name>` per function.
#
# The flag was parsed into MCCState.function_sections and read by NOTHING --
# accepted and silently discarded, so `mcc -ffunction-sections` produced
# byte-identical output and every user who passed it got nothing. This cell
# fails if it ever goes back to being a no-op.
#
# The assertions are about what the flag is FOR, not about the section names:
# a linker must be able to drop a function nobody references. So the cell
# checks that an unreferenced static survives a normal link and DISAPPEARS
# under --gc-sections, and that the program still produces the right answer
# both ways. A cell that only grepped for `.text.a` would pass on an object
# whose relocations made it unlinkable -- which is exactly the state the
# first cut of this feature was in, because every FDE still pointed at
# `.text` and GNU ld rejected the result with "overlapping FDEs".
#
# Usage: function-sections.sh <mcc> <mccbase> <workdir>
set -e

MCC=$1
BASE=$2
WORK=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] || {
	echo "usage: function-sections.sh <mcc> <mccbase> <workdir>" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
for t in readelf nm; do
	command -v $t >/dev/null 2>&1 || { echo "SKIP: $t not found"; exit 77; }
done

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/fs.c" <<'EOF'
extern int printf(const char *, ...);
static int unused_helper(int x) { return x * 3; }
int a(int x) { return x + 1; }
int b(int x) { return x + 2; }
int main(void) { printf("%d\n", a(1) + b(2)); return 0; }
EOF

"$MCC" -B"$BASE" -O2 -c "$WORK/fs.c" -o "$WORK/plain.o"
"$MCC" -B"$BASE" -O2 -ffunction-sections -c "$WORK/fs.c" -o "$WORK/fs.o"

rc=0

# 1. The flag must actually change the object.
if cmp -s "$WORK/plain.o" "$WORK/fs.o"; then
	echo "FAIL: -ffunction-sections produced a byte-identical object; the flag"
	echo "  is being accepted and discarded again"
	exit 1
fi

for fn in a b main unused_helper; do
	if readelf -SW "$WORK/fs.o" | grep -q "\.text\.$fn\b"; then
		:
	else
		echo "FAIL: no .text.$fn section"
		rc=1
	fi
done
[ $rc = 0 ] && echo "PASS: each function has its own .text.<name>"

# 2. It must still link and run, through mcc's own linker.
if "$MCC" -B"$BASE" -O2 -ffunction-sections "$WORK/fs.c" -o "$WORK/own" 2>"$WORK/e1"; then
	out=$("$WORK/own")
	if [ "$out" = "6" ]; then
		echo "PASS: links and runs under mcc's own linker"
	else
		echo "FAIL: mcc-linked output '$out', expected 6"
		rc=1
	fi
else
	echo "FAIL: mcc could not link its own -ffunction-sections object:"
	sed 's/^/  /' "$WORK/e1" | head -3
	rc=1
fi

# 3. An external linker must accept it too -- this is what caught the FDEs.
if command -v cc >/dev/null 2>&1 && cc -c -x c /dev/null -o "$WORK/probe.o" 2>/dev/null; then
	if cc "$WORK/fs.o" -o "$WORK/ext" 2>"$WORK/e2"; then
		out=$("$WORK/ext")
		if [ "$out" = "6" ]; then
			echo "PASS: links and runs under the external linker"
		else
			echo "FAIL: externally linked output '$out', expected 6"
			rc=1
		fi
	else
		echo "FAIL: the external linker rejected the object:"
		sed 's/^/  /' "$WORK/e2" | head -3
		rc=1
	fi

	# 4. The point of the flag: --gc-sections drops what nothing references.
	if cc -Wl,--gc-sections "$WORK/fs.o" -o "$WORK/gc" 2>"$WORK/e3"; then
		if [ "$("$WORK/gc")" != "6" ]; then
			echo "FAIL: --gc-sections build gives the wrong answer"
			rc=1
		elif nm "$WORK/gc" 2>/dev/null | grep -q unused_helper; then
			echo "FAIL: --gc-sections kept unused_helper; the sections are not"
			echo "  independently collectable"
			rc=1
		else
			echo "PASS: --gc-sections drops the unreferenced static"
		fi
		# And the control: without per-function sections it must NOT be dropped,
		# so the check above is measuring the flag rather than the linker.
		if cc -Wl,--gc-sections "$WORK/plain.o" -o "$WORK/gc0" 2>/dev/null; then
			if nm "$WORK/gc0" 2>/dev/null | grep -q unused_helper; then
				echo "PASS: control -- without the flag the static survives --gc-sections"
			else
				echo "FAIL: unused_helper was dropped WITHOUT -ffunction-sections, so"
				echo "  the assertion above proves nothing"
				rc=1
			fi
		fi
	else
		echo "FAIL: --gc-sections link failed:"
		sed 's/^/  /' "$WORK/e3" | head -3
		rc=1
	fi
else
	echo "SKIP-PART: no usable external cc; mcc-linker assertions still ran"
fi

exit $rc
