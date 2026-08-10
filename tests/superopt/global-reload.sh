#!/bin/sh
set -e

MCC=$1
BASE=$2
WORK=$3
SRC=$4
AWKF=$5
EXPECT=$6
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] && [ -n "$SRC" ] && [ -n "$AWKF" ] || {
	echo "usage: global-reload.sh <mcc> <mccbase> <work> <src.c> <facts.awk> [known-positive]" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
[ -f "$SRC" ] || { echo "SKIP: no subject at $SRC"; exit 77; }
[ -f "$AWKF" ] || { echo "SKIP: no fact extractor at $AWKF"; exit 77; }
command -v objdump >/dev/null 2>&1 || { echo "SKIP: objdump not found"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"
F=$WORK/facts
TAGS=$WORK/tags
: >"$TAGS"

rc=0
lv=?

fail() {
	echo "FAIL -O$lv [$1]: $2"
	echo "$1" >>"$TAGS"
	rc=1
}
note() { echo "  $*"; }

nloop() {
	awk -v f="$1" '$1=="LOOP" && $2==f {n++} END{print n+0}' "$F"
}
nrel() {
	awk -v f="$1" -v s="$2" '$1=="REL" && $2==f && $4==s {n++} END{print n+0}' "$F"
}
inloop() {
	awk -v f="$1" -v s="$2" '
		$1=="LOOP" && $2==f { k++; lo[k]=$3; hi[k]=$4 }
		$1=="REL" && $2==f && $4==s { m++; r[m]=$3 }
		END {
			for (i = 1; i <= m; i++)
				for (j = 1; j <= k; j++)
					if (("" r[i]) >= ("" lo[j]) && ("" r[i]) <= ("" hi[j])) { print 1; exit }
			print 0
		}' "$F"
}
firstoff() {
	awk -v f="$1" -v s="$2" '$1=="REL" && $2==f && $4==s {print $3; exit}' "$F"
}

for lv in 0 1 2 3 4 5 6 7 8 9 10 11 12; do
	obj=$WORK/g$lv.o
	if ! "$MCC" -B"$BASE" "-O$lv" -c "$SRC" -o "$obj" >"$WORK/e$lv" 2>&1; then
		fail compile "the subject did not compile"
		sed 's/^/  /' "$WORK/e$lv" | head -3
		continue
	fi
	objdump -dr "$obj" | awk -f "$AWKF" >"$F"
	if [ ! -s "$F" ]; then
		fail no-facts "objdump -dr yielded no relocation or branch facts"
		note "every check below would then pass by measuring nothing"
		continue
	fi

	for fn in spin_wait spin spin_cached; do
		if [ "$(nloop $fn)" -lt 1 ]; then
			fail loop-deleted "$fn has no backward branch"
			note "its loop was deleted, so 'the load stayed in the loop' is no"
			note "longer a question this cell asks"
		fi
	done

	if [ "$(inloop spin_wait g_flag)" = 1 ]; then :; else
		fail spin-wait-hoisted "the g_flag load left the 'while (!g_flag) {}' loop"
		note "A plain global must be re-read every iteration. That is the whole of"
		note "this compiler's thread-visibility story, and it holds only because"
		note "ast_cprop_is_local, ast_cse_regpure_compute and ast_plan_promotion"
		note "all refuse anything carrying VT_SYM -- not because anything decided"
		note "it should. Promoting globals into the innermost dominating frame is"
		note "precisely that filter widening. If that pass just landed, this cell"
		note "is the decision it was owed, not a regression to silence."
	fi
	if [ "$(inloop spin g_flag)" = 1 ]; then :; else
		fail spin-hoisted "the loop-invariant g_flag load was hoisted out of the loop"
	fi
	if [ "$(inloop spin_cached g_flag)" = 0 ]; then :; else
		fail cached-control "spin_cached reports its g_flag load inside the loop"
		note "but the source reads it once before the loop. The in-loop test is"
		note "answering yes to everything, so the two checks above prove nothing"
	fi

	n=$(nrel across_call g_data)
	if [ "$n" = 2 ]; then :; else
		fail reload-count "across_call references g_data $n times, expected 2"
		note "A global must be reloaded after an opaque call; folding the second"
		note "read into the first is the VT_SYM filter giving way"
	fi
	n=$(nrel once_call g_data)
	if [ "$n" = 1 ]; then :; else
		fail once-count "once_call references g_data $n times, expected 1"
		note "the reference count is not tracking the source, so the count of 2"
		note "above is not evidence of a reload"
	fi

	a=$(firstoff publish g_data)
	b=$(firstoff publish g_ready)
	if [ -n "$a" ] && [ -n "$b" ] && [ "$a" \< "$b" ]; then :; else
		fail store-order "publish put the g_ready store ($b) before g_data ($a)"
		note "'data = 42; ready = 1;' must keep its program order"
	fi
	a=$(firstoff publish_rev g_ready)
	b=$(firstoff publish_rev g_data)
	if [ -n "$a" ] && [ -n "$b" ] && [ "$a" \< "$b" ]; then :; else
		fail store-order-control "publish_rev did not keep its own reversed order"
		note "so the order check above is not reading the emitted sequence"
	fi
done

WANT="spin-wait-hoisted spin-hoisted cached-control reload-count once-count
store-order store-order-control"

if [ -z "$EXPECT" ]; then
	if [ $rc = 0 ]; then
		echo "PASS: over -O0..-O12 a plain global is re-read inside the loop,"
		echo "  reloaded after an opaque call, and its stores keep their program"
		echo "  order; the cached, single-read and reversed twins answer the other"
		echo "  way, so the three checks read the emitted code and not a constant"
	fi
	exit $rc
fi

if [ $rc = 0 ]; then
	echo "FAIL known-positive: the deliberately-broken subject $SRC passed every"
	echo "  check. A cell that cannot fail is worse than no cell."
	exit 1
fi
missing=
for t in $WANT; do
	grep -qx "$t" "$TAGS" || missing="$missing $t"
done
if [ -n "$missing" ]; then
	echo "FAIL known-positive: these checks never fired on the broken subject:$missing"
	echo "  each one is therefore unproven and may be inert in the real cell"
	exit 1
fi
echo "PASS known-positive: all 7 checks fired on $SRC"
exit 0
