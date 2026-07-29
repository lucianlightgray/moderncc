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

# -fdata-sections: the same idea for OBJECTS. Same class of flag -- it was also
# parsed into MCCState and read by nothing -- and it has a sharper failure mode:
# mcc already owns a section literally named `.data.ro`, so a global called `ro`
# can be silently MERGED into it instead of getting its own. That is checked at
# the SYMBOL level, because comparing section-name lists cannot tell "ro got its
# own section" from "ro was absorbed by the existing one".
cat >"$WORK/ds.c" <<'EOF'
extern int printf(const char *, ...);
int ro = 7;
int used_var = 1;
int unused_var = 2;
int get(void) { return used_var + ro; }
int main(void) { printf("%d\n", get()); return 0; }
EOF
"$MCC" -B"$BASE" -O2 -c "$WORK/ds.c" -o "$WORK/dsplain.o"
"$MCC" -B"$BASE" -O2 -fdata-sections -c "$WORK/ds.c" -o "$WORK/ds.o"

if cmp -s "$WORK/dsplain.o" "$WORK/ds.o"; then
	echo "FAIL: -fdata-sections produced a byte-identical object; accepted and discarded again"
	rc=1
elif readelf -SW "$WORK/ds.o" | grep -q '\.data\.used_var'; then
	echo "PASS: -fdata-sections gives each object its own section"
else
	echo "FAIL: no .data.used_var section"
	rc=1
fi

# The collision case, by symbol index rather than by name.
dsec=$(readelf -SW "$WORK/ds.o" | sed 's/[][]//g' | awk '$2==".data"{print $1}')
rosec=$(readelf -sW "$WORK/ds.o" | awk '$8=="ro"{print $7}')
if [ -n "$dsec" ] && [ "$rosec" = "$dsec" ]; then
	echo "PASS: a global named 'ro' falls back to .data instead of merging into .data.ro"
else
	echo "FAIL: 'ro' is in section '$rosec', expected the plain .data index '$dsec'"
	rc=1
fi

if "$MCC" -B"$BASE" -O2 -fdata-sections "$WORK/ds.c" -o "$WORK/dsown" 2>/dev/null &&
	 [ "$("$WORK/dsown")" = "8" ]; then
	echo "PASS: -fdata-sections links and runs under mcc's own linker"
else
	echo "FAIL: mcc-linked -fdata-sections build wrong or failed"
	rc=1
fi

if command -v cc >/dev/null 2>&1 && cc -Wl,--gc-sections "$WORK/ds.o" -o "$WORK/dsgc" 2>/dev/null; then
	if [ "$("$WORK/dsgc")" = "8" ] && ! nm "$WORK/dsgc" 2>/dev/null | grep -q unused_var; then
		echo "PASS: --gc-sections drops the unreferenced global"
	else
		echo "FAIL: --gc-sections build wrong, or kept unused_var"
		rc=1
	fi
fi

exit $rc
