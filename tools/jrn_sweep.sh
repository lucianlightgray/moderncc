#!/bin/sh
set -e
MCC=$1
CORPUS=$2
EXTRA=$3
OUT=$4
OPT=${5:--O1}
TMPL=${6:-0}
mkdir -p "$OUT"
: >"$OUT/ast.tsv"
: >"$OUT/jrn.tsv"
: >"$OUT/tot.txt"
for f in $(find "$CORPUS" -name '*.c' | sort) "$EXTRA"; do
	rel=$(printf '%s' "$f" | sed "s|^$CORPUS/||")
	MCC_AST_VERIFY=1 MCC_AST_TEMPLATES=$TMPL MCC_JOURNAL=1 \
		"$MCC" -w "$OPT" -c -o "$OUT/sweep.o" "$f" 2>"$OUT/err.txt" || true
	sed -n 's/^\[ast-verify\] //p' "$OUT/err.txt" |
		awk -v r="$rel" -F'\t' '{print r"\t"$3"\t"$1}' >>"$OUT/ast.tsv"
	sed -n 's/^\[jrn-verify\] //p' "$OUT/err.txt" |
		awk -v r="$rel" -F'\t' '{print r"\t"$3"\t"$1"\t"$4"\t"$5"\t"$6"\t"$7"\t"$8}' >>"$OUT/jrn.tsv"
	grep -E '^\[jrn-(total|fix|fixat|op|regdiff)\]' "$OUT/err.txt" >>"$OUT/tot.txt" || true
done
