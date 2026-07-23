#!/bin/sh
# arm64 divmagic soak, docker-gated and self-contained.
#
# The AST-reemit divmagic pass replaces constant integer divides/mods with
# reciprocal-multiply sequences. On arm64 the 64-bit high multiply is a native
# instruction (smulh/umulh) and the 32-bit path uses smull/umull, so — unlike
# i386 — no runtime helper archive is needed; the linked soak is self-contained.
# The normal build's mcc-arm64 is compiled WITHOUT the optimizer, so it cannot
# exercise this pass. This test therefore builds its own optimizer-enabled arm64
# cross mcc from the repo sources inside a linux/amd64 container (an amd64 host
# binary that EMITS arm64), uses it to compile a soak whose every constant
# divide/mod lives in its own leaf function (so divmagic fires), links it with
# the aarch64 cross gcc, and runs it under qemu-aarch64. Each rewritten divide is
# checked against a volatile-divisor hardware-div oracle over a wide input sweep;
# any mismatch is a real divmagic miscompile.
#
# Usage:  tools/arm64divmagic-docker.sh <mcc-arm64> [workdir]
#   <mcc-arm64>  host mcc-arm64 binary; only its presence gates the test
#                (the actual compiler used is rebuilt in-container).
# Exit:   0 all checks pass, 0 fails · 1 a divmagic miscompile · 77 skipped
#         (no docker / no mcc-arm64 / cannot run linux/amd64 / no qemu-aarch64).

set -eu
. "$(dirname "$0")/dockergate.sh"

MCC="${1:-}"
WORK="${2:-./w-arm64divmagic}"
IMAGE_BUILD="${MCC_DIVMAGIC_BUILD_IMAGE:-debian:bookworm-slim}"

dg_need_bin "$MCC" "arm64 mcc"
dg_need_docker
dg_need_platform linux/amd64 "$IMAGE_BUILD"
# qemu-aarch64 gate: only proceed if the build image can run an arm64 binary.
if ! dg_docker run --rm --platform linux/amd64 "$IMAGE_BUILD" sh -c '
       export DEBIAN_FRONTEND=noninteractive
       apt-get update -qq >/dev/null 2>&1
       apt-get install -y -qq gcc-aarch64-linux-gnu qemu-user-static >/dev/null 2>&1
       printf "int main(){return 42;}\n" > /t.c
       aarch64-linux-gnu-gcc -static /t.c -o /t.a64 && qemu-aarch64-static /t.a64
       [ $? -eq 42 ]' >/dev/null 2>&1; then
	echo "SKIP: cannot build/run an arm64 binary under qemu-aarch64 in the container"; exit 77
fi

# Repo root is the parent of this script's tools/ dir.
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$SCRIPT_DIR/.." && pwd)
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"

rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS=$(cd "$WORK" && pwd)
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"

# --- generate the soak C (bash: dash's echo mangles the \n in printf fmts) ---
cat > "$WORK_ABS/gen.sh" <<'GEN'
#!/usr/bin/env bash
set -eu
OUT="$1"

S32="7 3 5 6 9 10 11 13 25 49 100 127 255 1000 65535 65537 1000000 2147483647 -3 -7 -100 -1000 -2147483647 2 4 8 16 64 128 256 1024 65536"
U32="3 5 7 9 11 13 25 100 127 255 1000 65535 65537 1000000 2147483647 4294967295 2 4 8 16 256 1024 65536"
S64="7 3 11 13 100 1000 65537 1000000 1000000007 4294967296 1000000000000 -7 -1000 -4294967296 2 8 1024"
U64="3 7 11 13 100 1000 65537 1000000 1000000007 4294967296 1000000000000 2 8 1024"

{
echo 'extern int printf(const char *, ...);'
echo 'typedef long long ll; typedef unsigned long long ull; typedef unsigned u;'
echo '#define IMIN (-2147483647 - 1)'
echo '#define IMAX 2147483647'
echo '#define LMIN (-9223372036854775807LL - 1)'
echo '#define LMAX 9223372036854775807LL'
echo 'static long fails = 0, checks = 0;'
echo 'static void rep(const char*k,ll c,ll x,ll g,ll e){ if(fails<40) printf("MISMATCH %s div=%lld x=%lld got=%lld exp=%lld\n",k,c,x,g,e); fails++; }'
echo 'static int   rS(int x,int d){ volatile int vd=d; return x/vd; }'
echo 'static int   rSm(int x,int d){ volatile int vd=d; return x%vd; }'
echo 'static u     rU(u x,u d){ volatile u vd=d; return x/vd; }'
echo 'static u     rUm(u x,u d){ volatile u vd=d; return x%vd; }'
echo 'static ll    rS64(ll x,ll d){ volatile ll vd=d; return x/vd; }'
echo 'static ll    rS64m(ll x,ll d){ volatile ll vd=d; return x%vd; }'
echo 'static ull   rU64(ull x,ull d){ volatile ull vd=d; return x/vd; }'
echo 'static ull   rU64m(ull x,ull d){ volatile ull vd=d; return x%vd; }'

i=0; for C in $S32; do echo "static int s${i}d(int x){return x/($C);} static int s${i}m(int x){return x%($C);}"; i=$((i+1)); done
i=0; for C in $U32; do echo "static u u${i}d(u x){return x/(${C}u);} static u u${i}m(u x){return x%(${C}u);}"; i=$((i+1)); done
i=0; for C in $S64; do echo "static ll q${i}d(ll x){return x/(${C}LL);} static ll q${i}m(ll x){return x%(${C}LL);}"; i=$((i+1)); done
i=0; for C in $U64; do echo "static ull w${i}d(ull x){return x/(${C}ull);} static ull w${i}m(ull x){return x%(${C}ull);}"; i=$((i+1)); done

echo 'struct S32{int(*d)(int);int(*m)(int);int c;};'
echo 'static struct S32 s32[]={'
i=0; for C in $S32; do echo "{s${i}d,s${i}m,$C},"; i=$((i+1)); done
echo '};'
echo 'struct U32{u(*d)(u);u(*m)(u);u c;};'
echo 'static struct U32 u32[]={'
i=0; for C in $U32; do echo "{u${i}d,u${i}m,${C}u},"; i=$((i+1)); done
echo '};'
echo 'struct S64{ll(*d)(ll);ll(*m)(ll);ll c;};'
echo 'static struct S64 s64[]={'
i=0; for C in $S64; do echo "{q${i}d,q${i}m,${C}LL},"; i=$((i+1)); done
echo '};'
echo 'struct U64{ull(*d)(ull);ull(*m)(ull);ull c;};'
echo 'static struct U64 u64[]={'
i=0; for C in $U64; do echo "{w${i}d,w${i}m,${C}ull},"; i=$((i+1)); done
echo '};'

cat <<'EOF'
static int in32[]={0,1,-1,2,-2,3,-3,7,-7,8,-8,15,16,100,-100,127,128,255,256,1000,-1000,12345,-12345,65535,65536,65537,-65536,1000000,-1000000,IMAX,IMIN,IMAX-1,IMIN+1};
static ll in64[]={0,1,-1,7,-7,1000,-1000,100000,-100000,4294967295LL,4294967296LL,4294967297LL,-4294967296LL,1000000000000LL,-1000000000000LL,LMAX,LMIN,LMAX-1,LMIN+1};

int main(void){
	int i,k; long x;
	int n32=(int)(sizeof s32/sizeof s32[0]);
	int nu32=(int)(sizeof u32/sizeof u32[0]);
	int n64=(int)(sizeof s64/sizeof s64[0]);
	int nu64=(int)(sizeof u64/sizeof u64[0]);
	int mI=(int)(sizeof in32/sizeof in32[0]);
	int mL=(int)(sizeof in64/sizeof in64[0]);

	for(i=0;i<n32;i++) for(k=0;k<mI;k++){ int xv=in32[k]; if(xv==IMIN&&s32[i].c==-1)continue;
		checks++; if(s32[i].d(xv)!=rS(xv,s32[i].c)) rep("s32/",s32[i].c,xv,s32[i].d(xv),rS(xv,s32[i].c));
		checks++; if(s32[i].m(xv)!=rSm(xv,s32[i].c)) rep("s32%",s32[i].c,xv,s32[i].m(xv),rSm(xv,s32[i].c)); }
	for(i=0;i<nu32;i++) for(k=0;k<mI;k++){ u xv=(u)in32[k];
		checks++; if(u32[i].d(xv)!=rU(xv,u32[i].c)) rep("u32/",(ll)u32[i].c,(ll)xv,(ll)u32[i].d(xv),(ll)rU(xv,u32[i].c));
		checks++; if(u32[i].m(xv)!=rUm(xv,u32[i].c)) rep("u32%",(ll)u32[i].c,(ll)xv,(ll)u32[i].m(xv),(ll)rUm(xv,u32[i].c)); }
	for(i=0;i<n64;i++) for(k=0;k<mL;k++){ ll xv=in64[k]; if(xv==LMIN&&s64[i].c==-1)continue;
		checks++; if(s64[i].d(xv)!=rS64(xv,s64[i].c)) rep("s64/",s64[i].c,xv,s64[i].d(xv),rS64(xv,s64[i].c));
		checks++; if(s64[i].m(xv)!=rS64m(xv,s64[i].c)) rep("s64%",s64[i].c,xv,s64[i].m(xv),rS64m(xv,s64[i].c)); }
	for(i=0;i<nu64;i++) for(k=0;k<mL;k++){ ull xv=(ull)in64[k];
		checks++; if(u64[i].d(xv)!=rU64(xv,u64[i].c)) rep("u64/",(ll)u64[i].c,(ll)xv,(ll)u64[i].d(xv),(ll)rU64(xv,u64[i].c));
		checks++; if(u64[i].m(xv)!=rU64m(xv,u64[i].c)) rep("u64%",(ll)u64[i].c,(ll)xv,(ll)u64[i].m(xv),(ll)rU64m(xv,u64[i].c)); }

	/* wide strided sweeps for the hottest divisors */
	for(i=0;i<n32;i++) for(x=-3000000L;x<=3000000L;x+=101){ int xv=(int)x; if(xv==IMIN&&s32[i].c==-1)continue;
		checks++; if(s32[i].d(xv)!=rS(xv,s32[i].c)) rep("sw-s32/",s32[i].c,xv,s32[i].d(xv),rS(xv,s32[i].c));
		checks++; if(s32[i].m(xv)!=rSm(xv,s32[i].c)) rep("sw-s32%",s32[i].c,xv,s32[i].m(xv),rSm(xv,s32[i].c)); }
	for(i=0;i<nu32;i++) for(x=0L;x<=6000000L;x+=131){ u xv=(u)x;
		checks++; if(u32[i].d(xv)!=rU(xv,u32[i].c)) rep("sw-u32/",(ll)u32[i].c,(ll)xv,(ll)u32[i].d(xv),(ll)rU(xv,u32[i].c));
		checks++; if(u32[i].m(xv)!=rUm(xv,u32[i].c)) rep("sw-u32%",(ll)u32[i].c,(ll)xv,(ll)u32[i].m(xv),(ll)rUm(xv,u32[i].c)); }
	for(i=0;i<n64;i++) for(x=-3000000L;x<=3000000L;x+=137){ ll xv=(ll)x*100003LL; if(xv==LMIN&&s64[i].c==-1)continue;
		checks++; if(s64[i].d(xv)!=rS64(xv,s64[i].c)) rep("sw-s64/",s64[i].c,xv,s64[i].d(xv),rS64(xv,s64[i].c));
		checks++; if(s64[i].m(xv)!=rS64m(xv,s64[i].c)) rep("sw-s64%",s64[i].c,xv,s64[i].m(xv),rS64m(xv,s64[i].c)); }
	for(i=0;i<nu64;i++) for(x=0L;x<=6000000L;x+=139){ ull xv=(ull)x*100003ULL;
		checks++; if(u64[i].d(xv)!=rU64(xv,u64[i].c)) rep("sw-u64/",(ll)u64[i].c,(ll)xv,(ll)u64[i].d(xv),(ll)rU64(xv,u64[i].c));
		checks++; if(u64[i].m(xv)!=rU64m(xv,u64[i].c)) rep("sw-u64%",(ll)u64[i].c,(ll)xv,(ll)u64[i].m(xv),(ll)rU64m(xv,u64[i].c)); }

	printf("checks=%ld fails=%ld\n",checks,fails);
	return fails?1:0;
}
EOF
} > "$OUT"
echo "generated $OUT ($(wc -l < "$OUT") lines)"
GEN

# --- single linux/amd64 container: build the optimizer-enabled arm64 cross mcc
#     (amd64 host binary emitting arm64), compile the soak with divmagic, confirm
#     it fired, link with the aarch64 cross gcc, and run under qemu-aarch64. ---
echo "== docker linux/amd64: build optimizer-enabled arm64 cross mcc + soak + qemu run =="
dg_docker run --rm --platform linux/amd64 \
	-v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE_BUILD" bash -c '
	set -e
	export DEBIAN_FRONTEND=noninteractive
	apt-get update >/dev/null 2>&1
	apt-get install -y gcc binutils bash gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-user-static >/dev/null 2>&1
	# fast local copy of the pieces the build reads (RO bind is slow for many small reads)
	mkdir -p /b
	cp -a /repo/src /repo/include /repo/runtime /b/
	cd /b
	echo "-- building mcc-arm64-opt --"
	gcc -O1 -w -DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_ARM64=1 \
		-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 \
		-I src/arch/riscv64 -I src/objfmt -I src/formats -I include \
		src/mcc.c -o /w/mcc-arm64-opt
	echo "-- generating soak --"
	bash /w/gen.sh /w/soak.c
	echo "-- compiling soak with divmagic (MCC_AST_DIVMAGIC=1, -O2) --"
	MCC_AST_DIVMAGIC=1 /w/mcc-arm64-opt -O2 -I /b/runtime/include -c /w/soak.c -o /w/soak.o
	echo "-- confirm divmagic fired (native arm64 high-multiply) --"
	nmulhi=$(aarch64-linux-gnu-objdump -d /w/soak.o | grep -Ec "smulh|umulh|smull|umull" || true)
	ndiv=$(aarch64-linux-gnu-objdump -d /w/soak.o | grep -Ec "sdiv|udiv" || true)
	echo "   arm64 mulhi(smulh/umulh/smull/umull)=$nmulhi residual(sdiv/udiv)=$ndiv"
	if [ "$nmulhi" -lt 20 ]; then
		echo "FAIL  divmagic did not fire (expected arm64 mulhi >= 20)"
		exit 2
	fi
	echo "-- link (aarch64 static) + run under qemu-aarch64 --"
	aarch64-linux-gnu-gcc -static /w/soak.o -o /w/soak
	rc=0
	qemu-aarch64-static /w/soak || rc=$?
	echo "soak exit=$rc"
	exit $rc
'
