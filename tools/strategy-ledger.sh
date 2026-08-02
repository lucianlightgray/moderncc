#!/usr/bin/env bash
set -eu

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"
BDIR="${1:-cmake-debug}"
shift || true
OPTS=("$@")
[ "${#OPTS[@]}" -gt 0 ] || OPTS=(-O2 -O3)

MCC="$REPO/$BDIR/mcc"
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

for o in "${OPTS[@]}"; do
	: > "$WORK/raw$o"
	for f in $(find tests/exec -name '*.c' | sort) tests/diff/full_language.c; do
		"$MCC" "$o" -B "$BDIR" --stats -c "$f" -o "$WORK/a.o" 2>>"$WORK/raw$o" >/dev/null || true
		echo "##TU $f" >> "$WORK/raw$o"
	done
done

for o in "${OPTS[@]}"; do
	echo "=== $o"
	awk '
	/STRATEGY/ { instrat = 1; next }
	/FOLD|GATES/ { instrat = 0 }
	/^##TU/ {
		for (k in cur) { if (cur[k] > 0) tus[k]++ }
		delete cur
		next
	}
	instrat {
		line = $0
		gsub(/\033\[[0-9;]*m/, "", line)
		n = split(line, w, /[ \t]+/)
		for (i = 1; i <= n; i++) {
			if (w[i] ~ /^[a-z]+$/ && (i + 1) <= n && w[i+1] ~ /^[0-9]+$/) {
				name[w[i]] = 1
				tot[w[i]] += w[i+1]
				cur[w[i]] += w[i+1]
			}
		}
	}
	END {
		nfired = 0; nnever = 0
		for (k in name) {
			if (tot[k] > 0) { nfired++; printf "  FIRES  %-10s total=%-8d tus=%d\n", k, tot[k], tus[k] }
		}
		for (k in name) {
			if (tot[k] == 0) { nnever++; printf "  NEVER  %-10s\n", k }
		}
		printf "  summary: %d technique(s) fire, %d never fire anywhere in the corpus\n",
		       nfired, nnever
		if (nfired + nnever == 0) { print "  FAIL: no STRATEGY panel parsed; the ledger is vacuous"; exit 1 }
	}' "$WORK/raw$o" | sort
done
