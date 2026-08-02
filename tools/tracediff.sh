#!/bin/sh
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

BASE=$(cd "$(dirname "$MCC")" && pwd)

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

na=$(wc -l <"$WORK/a" | tr -d ' \t')
nb=$(wc -l <"$WORK/b" | tr -d ' \t')
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
