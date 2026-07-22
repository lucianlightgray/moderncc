#!/bin/sh
# riscv64 64-bit constant-materialization differential, docker-gated.
#
# RISC-V has no load-64-bit-immediate instruction, so mcc materializes a 64-bit
# constant with a lui + shifted-addi chain (load_large_constant, riscv64-gen.c).
# The signed 12-bit addi pieces carry between each other and into the high word;
# a mishandled top-piece carry silently corrupts constants whose low-32 word sits
# near 2^31 (regression guard for that fix). This builds the riscv64 cross mcc
# in-container (constant loading is base codegen, no optimizer needed), compiles a
# wide sweep of 64-bit constants (dense around the low-32 boundary region + a
# full-range stride + a deterministic pseudo-random tail), runs it under
# qemu-riscv64, and diffs its printed values against the same program built by
# riscv64-linux-gnu-gcc. Any divergence is a constant-materialization miscompile.
#
# Usage:  tools/riscv64const-docker.sh <mcc-riscv64> [workdir]
#   <mcc-riscv64>  host mcc-riscv64 binary; only its presence gates the test
#                  (the actual compiler used is rebuilt in-container).
# Exit:   0 mcc==gcc for all constants · 1 a miscompile · 77 skipped
#         (no docker / no mcc-riscv64 / cannot run linux/amd64 / no qemu-riscv64).

set -eu

MCC="${1:-}"
WORK="${2:-./w-riscv64const}"
IMAGE_BUILD="${MCC_DIVMAGIC_BUILD_IMAGE:-debian:bookworm-slim}"

if [ -z "$MCC" ] || [ ! -x "$MCC" ]; then echo "SKIP: riscv64 mcc not found at '${MCC:-<unset>}'"; exit 77; fi
if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
     docker run --rm --platform linux/amd64 "$IMAGE_BUILD" true >/dev/null 2>&1; then
	echo "SKIP: cannot run linux/amd64 containers ($IMAGE_BUILD)"; exit 77
fi
if ! MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
     docker run --rm --platform linux/amd64 "$IMAGE_BUILD" sh -c '
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
WORK_ABS=$(cd "$WORK" && pwd)
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"

# constant-sweep generator (kept out of the docker -c string to avoid quote hell)
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

echo "== docker linux/amd64: build riscv64 cross mcc + constant sweep + qemu diff vs gcc =="
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm --platform linux/amd64 \
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
