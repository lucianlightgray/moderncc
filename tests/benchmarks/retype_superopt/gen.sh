#!/usr/bin/env bash
set -u
WORK=${1:?usage: gen.sh <workdir>}
mkdir -p "$WORK"

TYPES=("int32_t" "uint32_t" "int64_t" "uint64_t" "float" "double" "long double")
declare -A FMT=(
	[int32_t]='"%lld\n",(long long)acc'
	[uint32_t]='"%llu\n",(unsigned long long)acc'
	[int64_t]='"%lld\n",(long long)acc'
	[uint64_t]='"%llu\n",(unsigned long long)acc'
	[float]='"%.0Lf\n",(long double)acc'
	[double]='"%.0Lf\n",(long double)acc'
	['long double']='"%.0Lf\n",(long double)acc'
)

emit() {
	local ty=$1 out=$2
	cat >"$out" <<EOF
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int main(int argc, char **argv) {
	long N = argc > 1 ? atol(argv[1]) : 1000;
	$ty acc = 0;
	for (long i = 1; i <= N; i++)
		acc += ($ty)i * ($ty)i;
	printf(${FMT[$ty]});
	return 0;
}
EOF
}

for ty in "${TYPES[@]}"; do
	slug=${ty// /_}
	emit "$ty" "$WORK/k_${slug}.c"
done
printf '%s\n' "${TYPES[@]}" >"$WORK/types.txt"
