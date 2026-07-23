#!/usr/bin/env bash
set -eu
. "$(dirname "$0")/dockergate.sh"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
WORK="${1:-./w-i386inline}"
rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"
IMAGE_BUILD="debian:bookworm-slim"
IMAGE_386="${MCC_I386_DOCKER_IMAGE:-i386/debian:bullseye-slim}"

dg_need_docker
dg_need_platform linux/386 "$IMAGE_386"

# --- stage 1: build optimizer-enabled i386 cross mcc in linux/amd64, prove
#     graft via the compiler's own marker + objdump, and emit tested/ref/main
#     C sources for the qemu differential (run in stage 2 under linux/386). ---
dg_docker run --rm --platform linux/amd64 \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE_BUILD" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends \
  gcc binutils libc6-dev libc6-dev-i386 \
  ca-certificates >/dev/null 2>&1 \
  || { echo "SKIP: apt install of i386 toolchain failed"; exit 77; }

mkdir -p /b
cp -a /repo/src /repo/include /repo/runtime /b/
cp -a /repo/.git /b/ 2>/dev/null || true

INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 \
     -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
DEF="-DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_I386=1"

echo "== build inline-capable (optimizer) i386 cross mcc =="
cd /b
gcc -O1 -w $DEF $INC src/mcc.c -o /w/mcc-i386-opt
echo "   built /w/mcc-i386-opt"

RD="/b/runtime/include"
INLENV="MCC_AST_REPLAY_DUMP=1 MCC_AST_TEMPLATES=0 MCC_AST_PROMOTE=0 MCC_AST_INLINE=1"

echo "== fixture-parity graft evidence (tests/ast/replay/inline.c) =="
cp /repo/tests/ast/replay/inline.c /w/fx.c
env $INLENV /w/mcc-i386-opt -O1 -I $RD -c /w/fx.c -o /w/fx.o 2>/w/fxdump.txt || true
echo "-- i386 candidate classification (graftable vs retained-only) --"
grep -E "\[ast-inline\] candidate (sumpt|addpt|sumbig|mkpair|add) " /w/fxdump.txt || true

echo "== graft-fires objdump evidence: leaf caller programs, OFF vs ON =="
# Each caller is a REPLAYED leaf whose callee is a static helper. With graft ON
# the callee body is spliced into the caller and the direct call is gone. The
# i386 SysV cdecl passes ALL args (incl. structs) on the stack, so small structs
# are NOT reg-pairs; we test which struct-by-value sizes the Tier-4 capture
# models vs desyncs.
cat > /w/gate.c <<EOF
extern int printf(const char*,...);
struct Pair { int a, b; };
struct Big  { long long a, b, c, d; };
static int addf(int a, int b){ return a + b; }
static int scalef(int x, int k){ return x * k; }
static long long llmul(long long a, long long b){ return a * b + a; }
static double areaf(double w, double h){ return w * h; }
static float fmixf(float a, float b){ return a * 3.0f + b; }
static int sumpair(struct Pair p){ return p.a + p.b; }
static long long sumbigf(struct Big b){ return b.a + b.b + b.c + b.d; }
static int derefsum(const int *a, int n){ int s = 0, i; for(i=0;i<n;i++) s += a[i]; return s; }

int c_scalar(int x, int y){ int r = addf(x, y); return scalef(r, 3) + addf(r, x); }
long long c_longlong(long long a, long long b){ return llmul(a, b) + llmul(b, a); }
double c_float(double a, double b){ return areaf(a, b) + areaf(b, a); }
float c_floatf(float a, float b){ return fmixf(a, b) + fmixf(b, a); }
int c_smallstruct(int x, int y){ struct Pair p; p.a = x; p.b = y; return sumpair(p) * 2 + sumpair(p); }
long long c_bigstruct(long long a, long long b, long long c, long long d){ struct Big s; s.a=a; s.b=b; s.c=c; s.d=d; return sumbigf(s) + a; }
int c_pointer(const int *a, int n){ return derefsum(a, n) + derefsum(a, n); }
EOF
env MCC_AST_TEMPLATES=0 MCC_AST_PROMOTE=0 MCC_AST_INLINE=0 \
  /w/mcc-i386-opt -O1 -I $RD -c /w/gate.c -o /w/gate_off.o 2>/dev/null
env $INLENV /w/mcc-i386-opt -O1 -I $RD -c /w/gate.c -o /w/gate_on.o 2>/w/gatedump.txt

OD=objdump
echo "-- graft dump for gate callers --"
grep -E "\[ast-inline\] (grafted|candidate) (addf|scalef|llmul|areaf|fmixf|sumpair|sumbigf|derefsum)" /w/gatedump.txt || true

GRAFT_OK=1
# Ground truth for "did the callee graft" is the compilers own marker
# [ast-inline] grafted <callee>. objdump -dr corroborates: a grafted callee
# leaves NO R_386_PLT32/R_386_PC32 call reloc to that callee symbol in the
# caller body (struct-by-value marshalling memcpy/memmove relocs are expected
# and are NOT calls to the callee).
$OD -dr /w/gate_on.o > /w/gate_on.dis
marker_graft() { # callee expect(yes|no) note
  ce="$1"; e="$2"; nt="$3"
  if grep -q "\[ast-inline\] grafted $ce$" /w/gatedump.txt; then g=yes; else g=no; fi
  if [ "$g" = "$e" ]; then echo "   OK   $ce : grafted=$g $nt";
  else echo "   MISS $ce : grafted=$g, expected $e $nt"; GRAFT_OK=0; fi
}
call_reloc() { # caller callee -> count of call relocs to callee in caller body
  awk -v f="<$1>:" -v s="$2" "/<.*>:/{p=(\$0 ~ f)} p && /R_386_(PLT32|PC32)[[:space:]]/ && \$0 ~ (s\"\$\"){c++} END{print c+0}" /w/gate_on.dis
}
echo "-- [ast-inline] grafted markers (compiler ground truth) + objdump corroboration --"
marker_graft addf     yes "(scalar args, all-on-stack)"
marker_graft scalef   yes "(scalar args, all-on-stack)"
marker_graft areaf    yes "(double args)"
marker_graft fmixf    yes "(float args)"
marker_graft sumpair  yes "(8-byte struct-by-value on stack)"
marker_graft derefsum yes "(pointer + loop)"
# EMPIRICAL i386 desync #1: long long args are a 64-bit STACK PAIR on i386
# SysV cdecl; the Tier-4 slot capture does not model the split 64-bit param, so
# llmul stays retained-only (mirrors the reg-pair/indirect desyncs on the 64-bit
# arches, but here it is the 32-bit long-long hazard). Correct finding.
marker_graft llmul    no  "(long long args: 64-bit stack pair not modeled -- retained)"
# EMPIRICAL i386 desync #2: 32-byte struct-by-value passed as a large stack copy;
# the by-value copy at the callsite is not modeled, so sumbigf stays retained,
# mirroring arm64/riscv64 sumbig.
marker_graft sumbigf  no  "(32-byte struct-by-value: large stack copy not modeled -- retained)"
echo "-- objdump: residual R_386 call reloc to each grafted callee (must be 0) --"
for pair in "c_scalar addf" "c_scalar scalef" "c_float areaf" "c_floatf fmixf" "c_smallstruct sumpair" "c_pointer derefsum"; do
  set -- $pair
  rc=$(call_reloc "$1" "$2")
  if [ "$rc" = 0 ]; then echo "   OK   $1 -> $2 : 0 call relocs (grafted)";
  else echo "   MISS $1 -> $2 : $rc call relocs remain"; GRAFT_OK=0; fi
done
rc=$(call_reloc c_bigstruct sumbigf)
echo "   INFO c_bigstruct -> sumbigf : $rc call reloc(s) (retained; expected >=1)"
rc=$(call_reloc c_longlong llmul)
echo "   INFO c_longlong -> llmul   : $rc call reloc(s) (retained; expected >=1)"

echo "== emit differential TUs (tested/ref/main) for stage-2 qemu run =="
cat > /w/tested.c <<EOF
struct Pair { int a, b; };
struct Big  { long long a, b, c, d; };
static int addf(int a, int b){ return a + b; }
static int scalef(int x, int k){ return x * k; }
static long long llmul(long long a, long long b){ return a * b + a; }
static double areaf(double w, double h){ return w * h; }
static float fmixf(float a, float b){ return a * 3.0f + b; }
static int sumpair(struct Pair p){ return p.a + p.b; }
static long long sumbigf(struct Big b){ return b.a + b.b + b.c + b.d; }
static int derefsum(const int *a, int n){ int s = 0, i; for(i=0;i<n;i++) s += a[i]; return s; }
int c_scalar(int x, int y){ int r = addf(x, y); return scalef(r, 3) + addf(r, x); }
long long c_longlong(long long a, long long b){ return llmul(a, b) + llmul(b, a); }
double c_float(double a, double b){ return areaf(a, b) + areaf(b, a); }
float c_floatf(float a, float b){ return fmixf(a, b) + fmixf(b, a); }
int c_smallstruct(int x, int y){ struct Pair p; p.a = x; p.b = y; return sumpair(p) * 2 + sumpair(p); }
long long c_bigstruct(long long a, long long b, long long c, long long d){ struct Big s; s.a=a; s.b=b; s.c=c; s.d=d; return sumbigf(s) + a; }
int c_pointer(const int *a, int n){ return derefsum(a, n) + derefsum(a, n); }
EOF
sed -e "s/\bc_scalar\b/c_scalar_REF/" \
    -e "s/\bc_longlong\b/c_longlong_REF/" \
    -e "s/\bc_float\b/c_float_REF/" \
    -e "s/\bc_floatf\b/c_floatf_REF/" \
    -e "s/\bc_smallstruct\b/c_smallstruct_REF/" \
    -e "s/\bc_bigstruct\b/c_bigstruct_REF/" \
    -e "s/\bc_pointer\b/c_pointer_REF/" /w/tested.c > /w/refimpl.c
cat > /w/main.c <<EOF
extern int printf(const char*,...);
typedef long long ll;
extern int    c_scalar(int,int);        extern int    c_scalar_REF(int,int);
extern ll     c_longlong(ll,ll);         extern ll     c_longlong_REF(ll,ll);
extern double c_float(double,double);    extern double c_float_REF(double,double);
extern float  c_floatf(float,float);     extern float  c_floatf_REF(float,float);
extern int    c_smallstruct(int,int);    extern int    c_smallstruct_REF(int,int);
extern ll     c_bigstruct(ll,ll,ll,ll);  extern ll     c_bigstruct_REF(ll,ll,ll,ll);
extern int    c_pointer(const int*,int); extern int    c_pointer_REF(const int*,int);
static long fails=0, checks=0;
static void rep(const char*k,ll a,ll b,ll g,ll e){ if(fails<40) printf("MISMATCH %s a=%lld b=%lld got=%lld exp=%lld\n",k,a,b,g,e); fails++; }
int main(void){
  int i,j;
  static int V[]={0,1,-1,2,-2,7,-7,100,-100,255,256,1000,-1000,65535,-65535,32767,-32768,12345,-98765,2000000000,-2000000000};
  int n=(int)(sizeof V/sizeof V[0]);
  for(i=0;i<n;i++) for(j=0;j<n;j++){
    int x=V[i],y=V[j];
    checks++; if(c_scalar(x,y)!=c_scalar_REF(x,y)) rep("scalar",x,y,c_scalar(x,y),c_scalar_REF(x,y));
    checks++; if(c_smallstruct(x,y)!=c_smallstruct_REF(x,y)) rep("smallstruct",x,y,c_smallstruct(x,y),c_smallstruct_REF(x,y));
    { ll a=(ll)x*100003LL, b=(ll)y; ll g=c_longlong(a,b), e=c_longlong_REF(a,b);
      checks++; if(g!=e) rep("longlong",a,b,g,e); }
  }
  for(i=-500;i<=500;i++) for(j=-500;j<=500;j+=25){
    double a=(double)i*8.0, b=(double)j*0.5;
    double g=c_float(a,b), e=c_float_REF(a,b);
    checks++; if(g!=e){ union{double d;ll l;}ug,ue; ug.d=g; ue.d=e; rep("float",i,j,ug.l,ue.l); }
    { float fa=(float)i*0.25f, fb=(float)j*2.0f; float fg=c_floatf(fa,fb), fe=c_floatf_REF(fa,fb);
      checks++; if(fg!=fe){ union{float f;int l;}ug2,ue2; ug2.f=fg; ue2.f=fe; rep("floatf",i,j,ug2.l,ue2.l); } }
  }
  for(i=0;i<n;i++) for(j=0;j<n;j++){
    ll a=(ll)V[i]*100003L, b=(ll)V[j], c=a^b, d=(a+b)*3;
    checks++; if(c_bigstruct(a,b,c,d)!=c_bigstruct_REF(a,b,c,d)) rep("bigstruct",a,b,c_bigstruct(a,b,c,d),c_bigstruct_REF(a,b,c,d));
  }
  { static int arr[64]; int k;
    for(k=0;k<64;k++){ arr[k]=(int)((k*2654435761u)^(k<<3)^0x55); }
    for(k=0;k<=64;k++){
      checks++; if(c_pointer(arr,k)!=c_pointer_REF(arr,k)) rep("pointer",k,0,c_pointer(arr,k),c_pointer_REF(arr,k));
    }
  }
  printf("checks=%ld fails=%ld\n",checks,fails);
  return fails?1:0;
}
EOF

echo "-- compile tested.c with mcc graft ON --"
env $INLENV /w/mcc-i386-opt -O1 -I $RD -c /w/tested.c -o /w/tested.o >/dev/null 2>&1
echo "-- direct-call residue in tested.o (grafted callees should not appear) --"
$OD -d /w/tested.o | grep -Eo "<(addf|scalef|llmul|areaf|fmixf|sumpair|derefsum)>" | sort | uniq -c || echo "   (none: fully grafted)"
env MCC_AST_TEMPLATES=0 MCC_AST_PROMOTE=0 MCC_AST_INLINE=0 \
  /w/mcc-i386-opt -O1 -I $RD -c /w/tested.c -o /w/tested_off.o >/dev/null 2>&1
echo "GRAFT_OK=$GRAFT_OK" > /w/graft_ok.txt
echo "-- stage1 GRAFT_OK=$GRAFT_OK --"
if [ "$GRAFT_OK" != 1 ]; then echo "I386INLINE FAIL (graft evidence)"; exit 1; fi
'

echo "== docker linux/386: compile ref+main with gcc -m32, link, run differential =="
dg_docker run --rm --platform linux/386 -v "$WP":/w -w /w "$IMAGE_386" sh -c '
set -e
if ! command -v gcc >/dev/null || [ ! -e /usr/lib/i386-linux-gnu/crti.o ] && [ ! -e /usr/lib/crti.o ]; then
  apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed in linux/386"; exit 77; }
  apt-get install -y gcc libc6-dev >/dev/null 2>&1 || { echo "SKIP: apt install gcc/libc6-dev failed in linux/386"; exit 77; }
fi
echo "-- compile ref + main with gcc -m32 -O2 --"
gcc -m32 -O2 -c /w/refimpl.c -o /w/refimpl.o
gcc -m32 -O2 -c /w/main.c    -o /w/main.o
echo "-- link + run (graft ON) --"
gcc -m32 -static /w/main.o /w/tested.o /w/refimpl.o -o /w/difftest 2>/dev/null \
  || gcc -m32 /w/main.o /w/tested.o /w/refimpl.o -o /w/difftest
rc=0; /w/difftest || rc=$?
echo "difftest(ON) exit=$rc"
echo "-- control: graft OFF, same differential --"
gcc -m32 -static /w/main.o /w/tested_off.o /w/refimpl.o -o /w/difftest_off 2>/dev/null \
  || gcc -m32 /w/main.o /w/tested_off.o /w/refimpl.o -o /w/difftest_off
rc2=0; /w/difftest_off || rc2=$?
echo "difftest(OFF) exit=$rc2"
if [ "$rc" != 0 ] || [ "$rc2" != 0 ]; then echo "I386INLINE FAIL (differential)"; exit 1; fi
echo "I386INLINE PASS"
'
