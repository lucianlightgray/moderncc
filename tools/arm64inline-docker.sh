#!/usr/bin/env bash
set -eu
. "$(dirname "$0")/dockergate.sh"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
WORK="${1:-./w-arm64inline}"
rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"
IMAGE="debian:bookworm-slim"

dg_need_docker

# Build+run on the host-native platform: linux/amd64 on x86 CI, linux/arm64 on
# an arm64 runner (Apple Silicon or GitHub arm64). Forcing linux/amd64 on an
# arm64 host makes the container's amd64 bash fail with "exec format error"
# (no amd64 binfmt), so we never pin amd64 -- native gcc + qemu-user work either way.
case "$(uname -m)" in
  x86_64|amd64)  HP_PLAT=linux/amd64 ;;
  aarch64|arm64) HP_PLAT=linux/arm64 ;;
  *)             HP_PLAT=linux/amd64 ;;
esac

dg_docker run --rm --platform "$HP_PLAT" \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends \
  gcc libc6-dev gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
  libc6-dev-arm64-cross \
  qemu-user-static git ca-certificates >/dev/null 2>&1 \
  || { echo "SKIP: apt install of arm64 toolchain failed"; exit 77; }

mkdir -p /b
cp -a /repo/src /repo/include /repo/runtime /b/
cp -a /repo/.git /b/ 2>/dev/null || true

INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 \
     -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
DEF="-DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_ARM64=1"

echo "== build inline-capable (optimizer) arm64 cross mcc =="
cd /b
gcc -O1 -w $DEF $INC src/mcc.c -o /w/mcc-arm64-opt
echo "   built /w/mcc-arm64-opt"

RD="/b/runtime/include"
INLENV="MCC_AST_REPLAY_DUMP=1 MCC_AST_TEMPLATES=0 MCC_AST_PROMOTE=0 MCC_AST_INLINE=1"

echo "== fixture-parity graft evidence (tests/ast/replay/inline.c) =="
cp /repo/tests/ast/replay/inline.c /w/fx.c
env $INLENV /w/mcc-arm64-opt -O1 -I $RD -c /w/fx.c -o /w/fx.o 2>/w/fxdump.txt || true
echo "-- arm64 candidate classification (graftable vs retained-only) --"
grep -E "\[ast-inline\] candidate (sumpt|addpt|sumbig|mkpair|add) " /w/fxdump.txt || true

echo "== graft-fires objdump evidence: leaf caller programs, OFF vs ON =="
# Each caller is a REPLAYED leaf whose callee is a static helper. With graft ON
# the callee body is spliced into the caller and the direct bl is gone. On
# AAPCS64 aggregates >16B are passed INDIRECTLY via a hidden pointer
# (VT_LLOCAL|VT_LVAL|VT_STRUCT); the Tier-4 capture now models that class, so
# sumbigf grafts here just like the direct-stack case on x86_64.
cat > /w/gate.c <<EOF
extern int printf(const char*,...);
struct Pair { int a, b; };
struct Big  { long a, b, c, d; };
static int addf(int a, int b){ return a + b; }
static int scalef(int x, int k){ return x * k; }
static double areaf(double w, double h){ return w * h; }
static int sumpair(struct Pair p){ return p.a + p.b; }
static long sumbigf(struct Big b){ return b.a + b.b + b.c + b.d; }
static int derefsum(const int *a, int n){ int s = 0, i; for(i=0;i<n;i++) s += a[i]; return s; }

int c_scalar(int x, int y){ int r = addf(x, y); return scalef(r, 3) + addf(r, x); }
double c_float(double a, double b){ return areaf(a, b) + areaf(b, a); }
int c_smallstruct(int x, int y){ struct Pair p; p.a = x; p.b = y; return sumpair(p) * 2 + sumpair(p); }
long c_bigstruct(long a, long b, long c, long d){ struct Big s; s.a=a; s.b=b; s.c=c; s.d=d; return sumbigf(s) + a; }
int c_pointer(const int *a, int n){ return derefsum(a, n) + derefsum(a, n); }
EOF
env MCC_AST_TEMPLATES=0 MCC_AST_PROMOTE=0 MCC_AST_INLINE=0 \
  /w/mcc-arm64-opt -O1 -I $RD -c /w/gate.c -o /w/gate_off.o 2>/dev/null
env $INLENV /w/mcc-arm64-opt -O1 -I $RD -c /w/gate.c -o /w/gate_on.o 2>/w/gatedump.txt

OD=aarch64-linux-gnu-objdump
echo "-- graft dump for gate callers --"
grep -E "\[ast-inline\] (grafted|candidate) (addf|scalef|areaf|sumpair|sumbigf|derefsum)" /w/gatedump.txt || true

GRAFT_OK=1
# Ground truth for "did the callee graft" is the compiler`s own marker
# [ast-inline] grafted <callee> (this is exactly what tests/ast/replay.cmake
# asserts). We require the marker for every case that must inline. objdump -dr
# then corroborates: a grafted callee leaves NO R_AARCH64_CALL26 reloc to that
# callee symbol in the caller body (residual memmove/memcpy relocs for struct-
# by-value arg marshalling are expected and are NOT calls to the callee).
$OD -dr /w/gate_on.o > /w/gate_on.dis
marker_graft() { # callee expect(yes|no) note
  ce="$1"; e="$2"; nt="$3"
  if grep -q "\[ast-inline\] grafted $ce$" /w/gatedump.txt; then g=yes; else g=no; fi
  if [ "$g" = "$e" ]; then echo "   OK   $ce : grafted=$g $nt";
  else echo "   MISS $ce : grafted=$g, expected $e $nt"; GRAFT_OK=0; fi
}
call_reloc() { # caller callee -> prints count of call relocs to callee in caller body
  awk -v f="<$1>:" -v s="$2" "/<.*>:/{p=(\$0 ~ f)} p && /R_AARCH64_CALL26[[:space:]]/ && \$0 ~ (s\"\$\"){c++} END{print c+0}" /w/gate_on.dis
}
echo "-- [ast-inline] grafted markers (compiler ground truth) + objdump corroboration --"
marker_graft addf     yes "(scalar args)"
marker_graft scalef   yes "(scalar args)"
marker_graft areaf    yes "(double args)"
marker_graft sumpair  yes "(16-byte reg-pair struct-by-value)"
marker_graft derefsum yes "(pointer + loop)"
# Large struct (>16B) passes INDIRECTLY on AAPCS64 (hidden pointer + memcpy).
# The Tier-4 capture now models the VT_LLOCAL|VT_LVAL|VT_STRUCT indirect class,
# so sumbigf grafts here (the by-value copy is re-materialised inline).
marker_graft sumbigf  yes "(32-byte struct-by-value: indirect ABI, now modeled)"
echo "-- objdump: residual R_AARCH64_CALL26 reloc to each grafted callee (must be 0) --"
for pair in "c_scalar addf" "c_scalar scalef" "c_float areaf" "c_smallstruct sumpair" "c_pointer derefsum" "c_bigstruct sumbigf"; do
  set -- $pair
  rc=$(call_reloc "$1" "$2")
  if [ "$rc" = 0 ]; then echo "   OK   $1 -> $2 : 0 call relocs (grafted)";
  else echo "   MISS $1 -> $2 : $rc call relocs remain"; GRAFT_OK=0; fi
done

echo "== differential vs aarch64-linux-gnu-gcc -O2 under qemu =="
# tested.c: callers compiled by mcc with graft ON.  ref TU: SAME callers renamed
# *_REF compiled by gcc -O2.  main sweeps inputs and compares.
cat > /w/tested.c <<EOF
struct Pair { int a, b; };
struct Big  { long a, b, c, d; };
static int addf(int a, int b){ return a + b; }
static int scalef(int x, int k){ return x * k; }
static double areaf(double w, double h){ return w * h; }
static int sumpair(struct Pair p){ return p.a + p.b; }
static long sumbigf(struct Big b){ return b.a + b.b + b.c + b.d; }
static int derefsum(const int *a, int n){ int s = 0, i; for(i=0;i<n;i++) s += a[i]; return s; }
int c_scalar(int x, int y){ int r = addf(x, y); return scalef(r, 3) + addf(r, x); }
double c_float(double a, double b){ return areaf(a, b) + areaf(b, a); }
int c_smallstruct(int x, int y){ struct Pair p; p.a = x; p.b = y; return sumpair(p) * 2 + sumpair(p); }
long c_bigstruct(long a, long b, long c, long d){ struct Big s; s.a=a; s.b=b; s.c=c; s.d=d; return sumbigf(s) + a; }
int c_pointer(const int *a, int n){ return derefsum(a, n) + derefsum(a, n); }
EOF
cat > /w/refimpl.c <<EOF
struct Pair { int a, b; };
struct Big  { long a, b, c, d; };
static int addf(int a, int b){ return a + b; }
static int scalef(int x, int k){ return x * k; }
static double areaf(double w, double h){ return w * h; }
static int sumpair(struct Pair p){ return p.a + p.b; }
static long sumbigf(struct Big b){ return b.a + b.b + b.c + b.d; }
static int derefsum(const int *a, int n){ int s = 0, i; for(i=0;i<n;i++) s += a[i]; return s; }
int c_scalar_REF(int x, int y){ int r = addf(x, y); return scalef(r, 3) + addf(r, x); }
double c_float_REF(double a, double b){ return areaf(a, b) + areaf(b, a); }
int c_smallstruct_REF(int x, int y){ struct Pair p; p.a = x; p.b = y; return sumpair(p) * 2 + sumpair(p); }
long c_bigstruct_REF(long a, long b, long c, long d){ struct Big s; s.a=a; s.b=b; s.c=c; s.d=d; return sumbigf(s) + a; }
int c_pointer_REF(const int *a, int n){ return derefsum(a, n) + derefsum(a, n); }
EOF
cat > /w/main.c <<EOF
extern int printf(const char*,...);
typedef long long ll;
extern int    c_scalar(int,int);        extern int    c_scalar_REF(int,int);
extern double c_float(double,double);    extern double c_float_REF(double,double);
extern int    c_smallstruct(int,int);    extern int    c_smallstruct_REF(int,int);
extern long   c_bigstruct(long,long,long,long); extern long c_bigstruct_REF(long,long,long,long);
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
  }
  for(i=-500;i<=500;i++) for(j=-500;j<=500;j+=25){
    double a=(double)i*8.0, b=(double)j*0.5;
    double g=c_float(a,b), e=c_float_REF(a,b);
    checks++; if(g!=e){ union{double d;ll l;}ug,ue; ug.d=g; ue.d=e; rep("float",i,j,ug.l,ue.l); }
  }
  for(i=0;i<n;i++) for(j=0;j<n;j++){
    long a=(long)V[i]*100003L, b=(long)V[j], c=a^b, d=(a+b)*3;
    checks++; if(c_bigstruct(a,b,c,d)!=c_bigstruct_REF(a,b,c,d)) rep("bigstruct",a,b,c_bigstruct(a,b,c,d),c_bigstruct_REF(a,b,c,d));
  }
  { static int arr[64]; int k; long s=0;
    for(k=0;k<64;k++){ arr[k]=(k*2654435761u)^(k<<3)^0x55; }
    for(k=0;k<=64;k++){
      checks++; if(c_pointer(arr,k)!=c_pointer_REF(arr,k)) rep("pointer",k,0,c_pointer(arr,k),c_pointer_REF(arr,k)); s+=k;
    }
  }
  printf("checks=%ld fails=%ld\n",checks,fails);
  return fails?1:0;
}
EOF

echo "-- compile tested.c with mcc graft ON --"
env $INLENV /w/mcc-arm64-opt -O1 -I $RD -c /w/tested.c -o /w/tested.o >/dev/null 2>&1
echo "-- direct-call residue in tested.o (grafted callees should not appear) --"
$OD -d /w/tested.o | grep -Eo "<(addf|scalef|areaf|sumpair|sumbigf|derefsum)>" | sort | uniq -c || echo "   (none: fully grafted)"
echo "-- compile ref + main with gcc -O2 --"
aarch64-linux-gnu-gcc -O2 -c /w/refimpl.c -o /w/refimpl.o
aarch64-linux-gnu-gcc -O2 -c /w/main.c    -o /w/main.o
echo "-- link static + run under qemu (graft ON) --"
aarch64-linux-gnu-gcc -static /w/main.o /w/tested.o /w/refimpl.o -o /w/difftest
rc=0
qemu-aarch64-static /w/difftest || rc=$?
echo "difftest(ON) exit=$rc"

echo "== control: tested.c with graft OFF, same differential (sanity) =="
env MCC_AST_TEMPLATES=0 MCC_AST_PROMOTE=0 MCC_AST_INLINE=0 \
  /w/mcc-arm64-opt -O1 -I $RD -c /w/tested.c -o /w/tested_off.o >/dev/null 2>&1
aarch64-linux-gnu-gcc -static /w/main.o /w/tested_off.o /w/refimpl.o -o /w/difftest_off
rc2=0
qemu-aarch64-static /w/difftest_off || rc2=$?
echo "difftest(OFF) exit=$rc2"

echo "GRAFT_OK=$GRAFT_OK ON_rc=$rc OFF_rc=$rc2"
if [ "$GRAFT_OK" != 1 ] || [ "$rc" != 0 ] || [ "$rc2" != 0 ]; then
  echo "ARM64INLINE FAIL"; exit 1
fi
echo "ARM64INLINE PASS"
'
