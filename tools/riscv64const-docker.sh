#!/bin/sh

set -eu
. "$(dirname "$0")/dockergate.sh"

MCC="${1:-}"
WORK="${2:-./w-riscv64const}"
IMAGE_BUILD="${MCC_DIVMAGIC_BUILD_IMAGE:-debian:bookworm-slim}"
HP_PLAT=$(dg_host_plat)

dg_need_bin "$MCC" "riscv64 mcc"
dg_need_docker
dg_need_platform "$HP_PLAT" "$IMAGE_BUILD"
if ! dg_docker run --rm --platform "$HP_PLAT" "$IMAGE_BUILD" sh -c '
       export DEBIAN_FRONTEND=noninteractive
       apt-get update -qq >/dev/null 2>&1
       apt-get install -y -qq gcc-riscv64-linux-gnu qemu-user-static >/dev/null 2>&1
       printf "int main(){return 42;}\n" > /t.c
       riscv64-linux-gnu-gcc -static /t.c -o /t.rv && qemu-riscv64-static /t.rv
       [ $? -eq 42 ]' >/dev/null 2>&1; then
	echo "SKIP: cannot build/run a riscv64 binary under qemu-riscv64 in the container"; exit 77
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$SCRIPT_DIR/.." && pwd)
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"

rm -rf "$WORK"; mkdir -p "$WORK"
dg_need_mount "$WORK"
WORK_ABS=$(cd "$WORK" && pwd)
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"

cat > "$WORK_ABS/gen.py" <<'GEN'
print("extern int printf(const char*,...);")
vals=[]
his=[0,0x1,0x7fff7fff,0x7fff8000,0x80000000,0xdeadbeef,0xffffffff,0x12345678]
los=list(range(0x7ff00000,0x80100000,0x1000))+list(range(0,0x100000000,0x8000001))
for hi in his:
    for lo in los: vals.append((hi<<32)|lo)
s=0x9e3779b97f4a7c15
for i in range(2000):
    s=(s*6364136223846793005+1442695040888963407)&0xffffffffffffffff
    vals.append(s)
print("static volatile unsigned long long C[]={")
for v in vals: print("0x%016xULL,"%v)
print("};")
print('int main(){int n=sizeof C/sizeof C[0];for(int i=0;i<n;i++)printf("%016llx\\n",C[i]);return 0;}')
GEN

echo "== docker $HP_PLAT: build riscv64 cross mcc + constant sweep + qemu diff vs gcc =="
dg_docker run --rm --platform "$HP_PLAT" \
	-v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE_BUILD" bash -c '
	set -e
	export DEBIAN_FRONTEND=noninteractive
	apt-get update >/dev/null 2>&1
	apt-get install -y gcc bash python3 gcc-riscv64-linux-gnu qemu-user-static >/dev/null 2>&1
	mkdir -p /b
	cp -a /repo/src /repo/include /repo/runtime /b/
	cd /b
	echo "-- building mcc-riscv64 (base codegen; constant loading is not optimizer-gated) --"
	gcc -O1 -w -DMCC_TARGET_RISCV64=1 \
		-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 \
		-I src/arch/riscv64 -I src/objfmt -I src/formats -I include \
		src/mcc.c -o /w/mcc-riscv64
	echo "-- generating constant sweep --"
	python3 /w/gen.py > /w/sweep.c
	echo "   constants: $(grep -c ULL /w/sweep.c)"
	echo "-- compile with mcc, link + run under qemu-riscv64 --"
	/w/mcc-riscv64 -O2 -I /b/runtime/include -c /w/sweep.c -o /w/s_mcc.o
	riscv64-linux-gnu-gcc -static /w/s_mcc.o -o /w/s_mcc
	qemu-riscv64-static /w/s_mcc > /w/o_mcc
	echo "-- reference: same program via riscv64 gcc --"
	riscv64-linux-gnu-gcc -O2 -static /w/sweep.c -o /w/s_gcc
	qemu-riscv64-static /w/s_gcc > /w/o_gcc
	if diff -q /w/o_mcc /w/o_gcc >/dev/null; then
		echo "PASS  mcc==gcc for all $(wc -l < /w/o_mcc) constants"
	else
		echo "FAIL  constant-materialization miscompile ($(diff /w/o_mcc /w/o_gcc | grep -c "^<") mismatches):"
		diff /w/o_mcc /w/o_gcc | head -20
		exit 1
	fi
'
