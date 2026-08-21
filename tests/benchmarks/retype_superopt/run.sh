#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/../../.."
ROOT=$(pwd)
MCC=${MCC:-$ROOT/cmake-def/mcc}
MB=${MB:-$ROOT/cmake-def}
REF_TYPE=int64_t
BENCH_N=${1:-30000000}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

"$ROOT/tests/benchmarks/retype_superopt/gen.sh" "$WORK" >/dev/null
mapfile -t TYPES <"$WORK/types.txt"

SAMPLES=(1 10 100 1000 46341 3037000500 4294967296)

jitrun() {
	"$MCC" -B"$MB" -run "$1" "$2" 2>/dev/null
}

echo "=== JIT equivalence proof (reference type = $REF_TYPE) ==="
printf "%-12s |" "sample N"
for ty in "${TYPES[@]}"; do printf " %-11s" "${ty:0:11}"; done
echo

declare -A OK
for ty in "${TYPES[@]}"; do OK[$ty]=1; done

for n in "${SAMPLES[@]}"; do
	ref=$(jitrun "$WORK/k_${REF_TYPE}.c" "$n")
	printf "%-12s |" "$n"
	for ty in "${TYPES[@]}"; do
		slug=${ty// /_}
		got=$(jitrun "$WORK/k_${slug}.c" "$n")
		if [ "$got" = "$ref" ]; then
			printf " %-11s" "match"
		else
			printf " %-11s" "DIFFER"
			OK[$ty]=0
		fi
	done
	echo
done

echo
echo "proven-equivalent (all samples match ref): "
PROVEN=()
for ty in "${TYPES[@]}"; do
	[ "${OK[$ty]}" = 1 ] && PROVEN+=("$ty") && printf "  - %s\n" "$ty"
done

echo
echo "=== benchmark of proven-equivalent variants at -O4 (N=$BENCH_N) ==="
printf "%-12s | %10s | %s\n" "type" "wall(ms)" "note"
best_ms=""; best_ty=""
for ty in "${PROVEN[@]}"; do
	slug=${ty// /_}
	exe="$WORK/b_${slug}"
	"$MCC" -B"$MB" -O4 "$WORK/k_${slug}.c" -o "$exe" 2>/dev/null || { printf "%-12s | %10s |\n" "$ty" "BUILDFAIL"; continue; }
	bm=""
	for r in 1 2 3; do
		t0=$EPOCHREALTIME; "$exe" "$BENCH_N" >/dev/null 2>&1; t1=$EPOCHREALTIME
		ms=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.1f",(b-a)*1000}')
		[ -z "$bm" ] && bm=$ms || { awk -v x="$ms" -v y="$bm" 'BEGIN{exit !(x<y)}' && bm=$ms; }
	done
	note=""
	[ "$ty" = "$REF_TYPE" ] && note="(reference / hotpatch baseline)"
	printf "%-12s | %10s | %s\n" "$ty" "$bm" "$note"
	if [ -z "$best_ms" ] || awk -v x="$bm" -v y="$best_ms" 'BEGIN{exit !(x<y)}'; then best_ms=$bm; best_ty=$ty; fi
done

echo
ref_ms=""
for ty in "${PROVEN[@]}"; do :; done
echo "fastest proven-equivalent type: $best_ty  (${best_ms} ms)"
