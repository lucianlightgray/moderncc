#!/bin/sh
# Locate where two mcc configurations first diverge, by diffing their TRACE logs.
#
# The recorder's failure mode is always "this body was accepted under config A
# and rejected under config B" -- and the historical way of chasing that was to
# hand-patch a temporary fprintf into whichever guard looked suspicious, rebuild,
# measure, revert. That is slow and throws the instrumentation away every time.
#
# MCC_CONFIG_TRACE builds already carry ~11.5k trace sites (every function entry
# and every branch), so the information is present; what was missing was a way to
# read it. Two things make it usable:
#   * scoping -- MCC_TRACE_FILE/MCC_TRACE_FUNC/MCC_TRACE_SKIP (mcclog.h). Roughly
#     80% of an unscoped trace is mccpp.c preprocessor churn that never
#     contributes to a codegen divergence; scoping to mccast cuts a 128k-line
#     trace to ~2.5k.
#   * values -- the four verdict-producing recorder hooks (vpush/vstore/vdup/
#     cmp_invert) print r/type/vn/rel, and AST_SET_DESYNC prints the recorder
#     state, so the diff shows WHICH VALUE decided, not merely which line ran.
#
# Worked example (this is the bug that motivated the tool -- MCC_AST_OPASSIGN
# staged at -O3 while nbody's advance() needed it at -O2):
#
#   tools/tracediff.sh ./cmake-debug/mcc repro.c MCC_AST_OPASSIGN=0 MCC_AST_OPASSIGN=1
#
# reports the first divergence inside ast_hook_vdup -- the compound-assignment
# path -- which is the whole answer.
#
# Usage: tracediff.sh <mcc> <src> <envA> <envB> [extra mcc flags ...]
#   envA/envB each mix VAR=VAL assignments and mcc flags ("-" = unchanged), so
#   both of these work:
#     tracediff.sh ./mcc r.c MCC_AST_OPASSIGN=0 MCC_AST_OPASSIGN=1
#     tracediff.sh ./mcc r.c -O2 -O3
# Env:
#   TD_FILE   trace scope, default "mccast"  (empty string = no file scoping)
#   TD_SKIP   comma-list of leaf functions to drop, default the tree walkers
#   TD_OPT    optimization level, default -O2
#   TD_CTX    context lines around the first divergence, default 12
set -e

MCC=$1
SRC=$2
ENVA=$3
ENVB=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$ENVA" ] && [ -n "$ENVB" ] || {
	echo "usage: tracediff.sh <mcc> <src> <envA> <envB> [mcc flags ...]" >&2
	exit 2
}
shift 4

TD_FILE=${TD_FILE-mccast}
TD_SKIP=${TD_SKIP-ast_kind,ast_first_child,ast_next_sib,ast_child,ast_op,ast_type_t,ast_parent}
TD_OPT=${TD_OPT--O2}
TD_CTX=${TD_CTX-12}

[ "$ENVA" = "-" ] && ENVA=
[ "$ENVB" = "-" ] && ENVB=

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# The mcc binary lives next to its -B base in every in-tree build layout.
BASE=$(cd "$(dirname "$MCC")" && pwd)

# A side spec mixes VAR=VAL assignments and mcc flags, so the two sides can
# differ by configuration (MCC_AST_OPASSIGN=0 vs =1) or by flags (-O2 vs -O3).
# An explicit -O on a side suppresses TD_OPT for that side.
run() {
	_spec=$1
	shift
	_env=
	_flags=
	for _t in $_spec; do
		case $_t in
		-*) _flags="$_flags $_t" ;;
		*=*) _env="$_env $_t" ;;
		esac
	done
	case " $_flags " in
	*" -O"*) _lvl= ;;
	*) _lvl=$TD_OPT ;;
	esac
	env $_env MCC_TRACE_FILE="$TD_FILE" MCC_TRACE_SKIP="$TD_SKIP" \
		"$MCC" -B"$BASE" -v128 $_lvl $_flags "$@" -c "$SRC" -o /dev/null 2>&1 |
		sed 's/\x1b\[[0-9;]*[A-Za-z]//g' |
		grep '^\[TRACE\] ' |
		sed 's|^\[TRACE\] .*/src/|src/|'
}

run "$ENVA" "$@" >"$WORK/a" || true
run "$ENVB" "$@" >"$WORK/b" || true

na=$(wc -l <"$WORK/a")
nb=$(wc -l <"$WORK/b")
echo "== trace sizes: A=$na  B=$nb  (scope: file=${TD_FILE:-<all>})"

if cmp -s "$WORK/a" "$WORK/b"; then
	echo "== IDENTICAL: these two configurations traced the same path."
	echo "   Either the config does nothing here, or the difference is outside"
	echo "   the trace scope -- retry with TD_FILE= (no scoping)."
	exit 0
fi

echo
echo "== FIRST DIVERGENCE (this is the actionable line)"
diff -u "$WORK/a" "$WORK/b" | sed -n "1,$((TD_CTX * 2 + 4))p" | tail -n +4

echo
echo "== functions ranked by diverging lines"
diff "$WORK/a" "$WORK/b" |
	grep -oE '[a-z_0-9]+: ' | sed 's/: //' | sort | uniq -c | sort -rn | head -12

echo
echo "== desync verdicts"
for side in a b; do
	n=$(grep -c 'DESYNC' "$WORK/$side" || true)
	printf "   %s: %s desync(s)" "$side" "$n"
	grep 'DESYNC' "$WORK/$side" | head -1 | sed 's/^/  first: /' || echo
	echo
done
