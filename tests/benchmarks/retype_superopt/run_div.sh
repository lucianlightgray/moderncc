#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/../../.."
ROOT=$(pwd)
MCC=${MCC:-$ROOT/cmake-def/mcc}
MB=${MB:-$ROOT/cmake-def}
REF_TYPE=int64_t
BENCH_N=${1:-40000000}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

TYPES=("int32_t" "uint32_t" "int64_t" "uint64_t" "float" "double")
declare -A FMT=(
	[int32_t]='"%lld\n",(long long)r'
	[uint32_t]='"%llu\n",(unsigned long long)r'
	[int64_t]='"%lld\n",(long long)r'
	[uint64_t]='"%llu\n",(unsigned long long)r'
	[float]='"%.0f\n",(double)r'
	[double]='"%.0f\n",(double)r'
)

emit() {
	local ty=$1 out=$2
	cat >"$out" <<EOF
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int main(int argc, char **argv) {
	long N = argc > 1 ? atol(argv[1]) : 1000;
	$ty r = 0;
	for (long i = 1; i <= N; i++)
		r ^= ($ty)2000000000 / ($ty)((i & 0xffff) + 1);
	printf(${FMT[$ty]});
	return 0;
}
EOF
}

for ty in "${TYPES[@]}"; do emit "$ty" "$WORK/d_${ty// /_}.c"; done

SAMPLES=(1 7 100 65535 65536 1000000 40000000)

jitrun() { "$MCC" -B"$MB" -run "$1" "$2" 2>/dev/null; }

echo "=== kernel: r ^= 2000000000 / ((i & 0xffff)+1)   (all operands provably < 2^31) ==="
echo "=== JIT equivalence proof (reference type = $REF_TYPE) ==="
printf "%-12s |" "sample N"
for ty in "${TYPES[@]}"; do printf " %-9s" "${ty:0:9}"; done
echo
declare -A OK
for ty in "${TYPES[@]}"; do OK[$ty]=1; done
for n in "${SAMPLES[@]}"; do
	ref=$(jitrun "$WORK/d_${REF_TYPE}.c" "$n")
	printf "%-12s |" "$n"
	for ty in "${TYPES[@]}"; do
		got=$(jitrun "$WORK/d_${ty// /_}.c" "$n")
		if [ "$got" = "$ref" ]; then printf " %-9s" "match"; else printf " %-9s" "DIFFER"; OK[$ty]=0; fi
	done
	echo
done

echo
PROVEN=()
for ty in "${TYPES[@]}"; do [ "${OK[$ty]}" = 1 ] && PROVEN+=("$ty"); done
echo "proven-equivalent: ${PROVEN[*]}"

echo
echo "=== benchmark of proven-equivalent variants at -O4 (N=$BENCH_N) ==="
printf "%-12s | %10s | %s\n" "type" "wall(ms)" "note"
best_ms=""; best_ty=""; ref_ms=""
for ty in "${PROVEN[@]}"; do
	exe="$WORK/b_${ty// /_}"
	"$MCC" -B"$MB" -O4 "$WORK/d_${ty// /_}.c" -o "$exe" 2>/dev/null || { printf "%-12s | %10s |\n" "$ty" "BUILDFAIL"; continue; }
	bm=""
	for rp in 1 2 3; do
		t0=$EPOCHREALTIME; "$exe" "$BENCH_N" >/dev/null 2>&1; t1=$EPOCHREALTIME
		ms=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.1f",(b-a)*1000}')
		[ -z "$bm" ] && bm=$ms || { awk -v x="$ms" -v y="$bm" 'BEGIN{exit !(x<y)}' && bm=$ms; }
	done
	note=""; [ "$ty" = "$REF_TYPE" ] && { note="(reference / hotpatch baseline)"; ref_ms=$bm; }
	printf "%-12s | %10s | %s\n" "$ty" "$bm" "$note"
	if [ -z "$best_ms" ] || awk -v x="$bm" -v y="$best_ms" 'BEGIN{exit !(x<y)}'; then best_ms=$bm; best_ty=$ty; fi
done

echo
if [ -n "$ref_ms" ] && [ -n "$best_ms" ]; then
	spd=$(awk -v r="$ref_ms" -v b="$best_ms" 'BEGIN{printf "%.2f", r/b}')
	echo "winner: $best_ty @ ${best_ms} ms  vs  $REF_TYPE baseline @ ${ref_ms} ms  =>  ${spd}x"
fi
