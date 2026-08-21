#!/bin/sh
# Keeps tests/optfire/coverage.txt honest (T-lin-10466). For every ledger row it
# checks that the evidence still resolves: covered/partial/dev cells must name a
# live optfire row or a tests/cli/cases.h case, gap rows must name a real task,
# base/oos rows must carry a note. A renamed or deleted cell fails the cell it
# was cited in, so the completeness map cannot silently rot.
set -e

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
ledger=$here/coverage.txt
[ -f "$ledger" ] || { echo "FAIL: no coverage.txt at $ledger"; exit 1; }

cells=$(cat "$here"/counters*.txt "$here"/differs*.txt "$here"/levels*.txt \
	"$here"/cdelta*.txt "$here"/defstate*.txt 2>/dev/null |
	grep -v '^#' | grep -v '^[[:space:]]*$' | cut -d'|' -f1 | sort -u)
cases=$(grep -oE '\{"[a-z0-9_]+"' "$root/tests/cli/cases.h" | sed 's/{"//;s/"//' | sort -u)
tasks=$(cat "$root/docs/TODO.md" "$root/docs/ARCHIVED.md" "$root/docs/DETAILS.md" 2>/dev/null |
	grep -oE 'T-lin-[0-9]+' | sort -u)

known_cell() { printf '%s\n' "$cells" "$cases" | grep -qx "$1"; }
known_task() { printf '%s\n' "$tasks" | grep -qx "$1"; }

fails=0
covered=0 gap=0 base=0 oos=0
while IFS='|' read -r id name status evidence; do
	case "$id" in ''|\#*) continue ;; esac
	[ -n "$status" ] || { echo "FAIL $id ($name): no status"; fails=$((fails+1)); continue; }
	lead=${evidence%% — *}
	case "$status" in
	covered|partial|dev)
		covered=$((covered+1))
		toks=$(printf '%s' "$lead" | tr ',' '\n' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
		for t in $toks; do
			[ -n "$t" ] || continue
			known_cell "$t" || { echo "FAIL $id ($name): cell '$t' resolves to no optfire row or cli case"; fails=$((fails+1)); }
		done
		;;
	gap)
		gap=$((gap+1))
		first=$(printf '%s' "$lead" | tr ',' '\n' | head -1 | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
		case "$first" in
		T-lin-*) known_task "$first" || { echo "FAIL $id ($name): gap task '$first' not found in TODO/ARCHIVED/DETAILS"; fails=$((fails+1)); } ;;
		*) echo "FAIL $id ($name): gap evidence must name a T-lin task, got '$first'"; fails=$((fails+1)) ;;
		esac
		;;
	base) base=$((base+1)); [ -n "$evidence" ] || { echo "FAIL $id ($name): base row needs a note"; fails=$((fails+1)); } ;;
	oos)  oos=$((oos+1));  [ -n "$evidence" ] || { echo "FAIL $id ($name): oos row needs a note"; fails=$((fails+1)); } ;;
	*) echo "FAIL $id ($name): unknown status '$status'"; fails=$((fails+1)) ;;
	esac
done < "$ledger"

echo "coverage: $covered covered/partial/dev, $gap gap, $base base, $oos oos"
[ "$fails" -eq 0 ] || { echo "FAIL: $fails ledger row(s) did not resolve"; exit 1; }
echo "PASS: optimizer coverage ledger resolves"
