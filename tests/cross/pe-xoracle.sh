#!/bin/sh
# pe/x-oracle (W1): native cross-vendor conformance oracle for x86_64-win32.
# For each run-mode tests/exec golden, compile+run with mcc (PE) and with the
# vendored mingw-w64 gcc (PE) on this native Windows host, and require the two
# to agree on exit status and stdout. Known implementation-defined divergences
# are listed in pe-xoracle-exclude.txt and counted but not failed. A floor on the
# agreement count keeps the corpus from silently shrinking.
#
# usage: pe-xoracle.sh <mcc> <gcc> <root> <work> <floor> [--mutate]
set -u
MCC="$1"; GCC="$2"; ROOT="$3"; WORK="$4"; FLOOR="$5"
MUTATE=0
[ "${6:-}" = "--mutate" ] && MUTATE=1

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then echo "pe-xoracle: no mcc: $MCC" >&2; exit 77; fi
if [ ! -f "$GCC" ]; then echo "pe-xoracle: no mingw gcc: $GCC (skip)" >&2; exit 77; fi

EXCL="$ROOT/tests/cross/pe-xoracle-exclude.txt"
excluded() {
	[ -f "$EXCL" ] || return 1
	grep -v '^#' "$EXCL" | grep -v '^[[:space:]]*$' | grep -qx "$1"
}

rm -rf "$WORK"; mkdir -p "$WORK"
inc="-I$ROOT/tests -I$ROOT/tests/exec -I$ROOT/runtime/include"
agree=0; newdiff=0; excl=0; nocompile=0
: > "$WORK/newdiff.txt"
first_agree=""

rows=$(grep -E '^[[:space:]]*\{"' "$ROOT/tests/exec/goldens.h")
IFS='
'
for line in $rows; do
	mode=$(printf '%s' "$line" | sed -E 's/^[[:space:]]*\{"[^"]*",[[:space:]]*"([^"]*)".*/\1/')
	src=$(printf '%s' "$line" | sed -E 's/^[[:space:]]*\{"[^"]*",[[:space:]]*"[^"]*",[[:space:]]*"([^"]*)".*/\1/')
	[ "$mode" = "run" ] || continue
	f="$ROOT/tests/$src"
	[ -f "$f" ] || continue
	base=$(basename "$src" .c)
	mexe="$WORK/m_$base.exe"; gexe="$WORK/g_$base.exe"
	# mingw is the reference; if it cannot build the program natively it is not a
	# usable oracle input, so skip silently (not counted against mcc).
	"$GCC" -w $inc "$f" -o "$gexe" >/dev/null 2>&1 || continue
	# now mcc: a failure here is an mcc-specific gap on this target.
	"$MCC" $inc "$f" -o "$mexe" >/dev/null 2>&1 || { if excluded "$src"; then excl=$((excl+1)); else nocompile=$((nocompile+1)); echo "MCC_NOCOMPILE $src" >> "$WORK/newdiff.txt"; fi; continue; }
	mo=$("$mexe" 2>/dev/null); mr=$?
	go=$("$gexe" 2>/dev/null); gr=$?
	if [ "$MUTATE" = "1" ] && [ -z "$first_agree" ] && [ "$mo" = "$go" ] && [ "$mr" = "$gr" ]; then
		# negative control: corrupt the mcc result of the first agreeing program and
		# require it to fall through the real compare -> new-divergence path.
		first_agree="$src"
		mo="${mo}__MUTATED__"
	fi
	if [ "$mo" = "$go" ] && [ "$mr" = "$gr" ]; then
		agree=$((agree+1))
	else
		if excluded "$src"; then excl=$((excl+1)); else newdiff=$((newdiff+1)); echo "DIFF $src (mrc=$mr grc=$gr)" >> "$WORK/newdiff.txt"; fi
	fi
done
unset IFS

echo "pe-xoracle: agree=$agree new_divergence=$newdiff excluded=$excl mcc_nocompile_new=$nocompile floor=$FLOOR"

if [ "$MUTATE" = "1" ]; then
	# expect the injected mismatch to have driven exactly the real compare->diff path
	if [ "$newdiff" -ge 1 ]; then
		echo "pe-xoracle: MUTATE arm drove the oracle red as expected (compare path is live)"
		exit 0
	fi
	echo "pe-xoracle: MUTATE arm did NOT go red; the oracle adjudicates nothing" >&2
	exit 1
fi

rc=0
if [ "$newdiff" != "0" ] || [ "$nocompile" != "0" ]; then
	echo "pe-xoracle: $newdiff new divergence(s) + $nocompile new mcc-nocompile outside the exclusion list:" >&2
	cat "$WORK/newdiff.txt" >&2
	rc=1
fi
if [ "$agree" -lt "$FLOOR" ]; then
	echo "pe-xoracle: agreement $agree fell below floor $FLOOR (corpus shrank)" >&2
	rc=1
fi
exit $rc
