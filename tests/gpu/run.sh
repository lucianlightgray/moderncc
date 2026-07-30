#!/bin/sh
set -u

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)

MCC=${MCC:-$root/cmake-debug/mcc}
BDIR=${BDIR:-$root/cmake-debug}
IDIR=${IDIR:-$root/runtime/include}
WORK=${WORK:-$here/.work}
QUICK=${QUICK:-0}

fail=0
ran=0

say() { printf '%s\n' "$*"; }
hr() { say "------------------------------------------------------------"; }

need() {
    command -v "$1" >/dev/null 2>&1 || {
        say "SKIP: $1 not found ($2)"
        exit 77
    }
}

[ -x "$MCC" ] || { say "SKIP: no mcc at $MCC (set MCC=)"; exit 77; }
need glslc "install shaderc"
need vulkaninfo "install vulkan-tools"

vulkaninfo --summary >/dev/null 2>&1 || {
    say "SKIP: no usable Vulkan device"
    exit 77
}

gpu=$(vulkaninfo --summary 2>/dev/null | sed -n 's/.*deviceName *= *//p' | head -1)

mkdir -p "$WORK"
say "mcc:    $MCC"
say "gpu:    ${gpu:-unknown}"
say "work:   $WORK"
hr

cc_host() {
    "$MCC" -O0 -B"$BDIR" -I"$IDIR" "$1" -o "$2" ${3:-} || {
        say "FAIL: could not build $1"
        fail=$((fail + 1))
        return 1
    }
}

say "building"
cc_host "$here/cpu/sweep_int32.c" "$WORK/sweep_int32" || exit 1
cc_host "$here/cpu/sweep_int64.c" "$WORK/sweep_int64" || exit 1
cc_host "$here/cpu/sweep_float.c" "$WORK/sweep_float" || exit 1
cc_host "$here/cpu/rev64_mt.c" "$WORK/rev64_mt" "-pthread" || exit 1
cc_host "$here/rev64_vk.c" "$WORK/rev64_vk" "-lvulkan" || exit 1
cc_host "$here/halves_vk.c" "$WORK/halves_vk" "-lvulkan" || exit 1

for s in "$here/rev64.comp" "$here/halves.comp" \
         "$here/known_positive/crossterm.comp" \
         "$here/known_positive/bothhalves.comp"; do
    n=$(basename "$s" .comp)
    glslc -O -fshader-stage=compute "$s" -o "$WORK/$n.spv" || {
        say "FAIL: glslc $s"
        fail=$((fail + 1))
    }
done
hr

check() {
    ran=$((ran + 1))
    if [ "$2" = "$3" ]; then
        say "PASS  $1"
    else
        say "FAIL  $1"
        say "        expected: $3"
        say "        got:      $2"
        fail=$((fail + 1))
    fi
}

say "cpu exhaustive sweeps"
got=$("$WORK/sweep_int32" | tr '\n' ' ')
check "int32/uint32 full range" \
    "$(printf '%s' "$got" | sed -n 's/.*unsigned: n=\([0-9]*\).*/\1/p')" \
    "4294967296"

got=$("$WORK/sweep_float")
check "float classification sums to 2^32" \
    "$(printf '%s\n' "$got" | sed -n 's/^sum=//p')" "4294967296"
check "float NaN self-compare count" \
    "$(printf '%s\n' "$got" | sed -n 's/^self_ne=//p')" "16777214"
check "float sNaN quieted on double roundtrip" \
    "$(printf '%s\n' "$got" | sed -n 's/^roundtrip_mismatch=\([0-9]*\).*/\1/p')" "8388606"

got=$("$WORK/sweep_int64" | sed -n 's/^lattice: n=128 bits_one=\([0-9]*\) bits_zero=\([0-9]*\)/\1 \2/p')
check "int64 lattice covers every bit as 0 and 1" "$got" "64 64"
hr

if [ "$QUICK" = "1" ]; then
    N=$((1 << 24))
else
    N=$((1 << 32))
fi

say "cpu/gpu differential (N=$N)"
cpu_x=$("$WORK/rev64_mt" 16 $N | sed -n 's/^xsum=\([0-9a-f]*\).*/\1/p')
gpu_x=$(cd "$WORK" && ./rev64_vk $N rev64.spv | sed -n 's/^xsum=\([0-9a-f]*\).*/\1/p')
check "cpu xsum == gpu xsum" "$gpu_x" "$cpu_x"
hr

say "gpu half-split cross-product"
out=$(cd "$WORK" && ./halves_vk $N halves.spv)
say "$out" | sed -n 's/^swept=/  swept=/p'
check "separable+coupled checks clean" \
    "$(printf '%s\n' "$out" | sed -n 's/^bad_rev=\([0-9]*\) bad_pop=\([0-9]*\) bad_mul=\([0-9]*\)/\1 \2 \3/p')" \
    "0 0 0"
hr

say "known positives (these MUST fail; a pass here means the instrument is blind)"
for kp in crossterm bothhalves; do
    ran=$((ran + 1))
    out=$(cd "$WORK" && ./halves_vk $N "$kp.spv" 2>/dev/null)
    bad=$(printf '%s\n' "$out" | sed -n 's/^bad_rev=[0-9]* bad_pop=[0-9]* bad_mul=\([0-9]*\)/\1/p')
    if [ "${bad:-0}" != "0" ]; then
        say "PASS  $kp detected (bad_mul nonzero)"
    else
        say "FAIL  $kp NOT detected -- instrument is blind"
        fail=$((fail + 1))
    fi
done
hr

say "checks=$ran failures=$fail"
[ "$fail" -eq 0 ] || exit 1
say "OK"
exit 0
