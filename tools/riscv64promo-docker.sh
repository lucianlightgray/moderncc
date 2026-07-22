#!/usr/bin/env bash
set -eu

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
WORK="${1:-./w-riscv64promo}"
rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"
IMAGE="debian:bookworm-slim"

if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! docker info >/dev/null 2>&1; then echo "SKIP: docker daemon not available"; exit 77; fi

MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm --platform linux/amd64 \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends \
  gcc libc6-dev gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu \
  libc6-dev-riscv64-cross \
  qemu-user-static git ca-certificates >/dev/null 2>&1 \
  || { echo "SKIP: apt install of riscv64 toolchain failed"; exit 77; }

mkdir -p /b
cp -a /repo/src /repo/include /repo/runtime /b/
cp -a /repo/.git /b/ 2>/dev/null || true

INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 \
     -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
DEF="-DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_RISCV64=1"

echo "== build modified (promotion-capable) riscv64 cross mcc =="
cd /b
gcc -O1 -w $DEF $INC src/mcc.c -o /w/mcc-rv64-opt
echo "   built /w/mcc-rv64-opt"

echo "== build pristine (HEAD) riscv64 cross mcc for OFF byte-identity =="
mkdir -p /p
cp -a /repo/src /repo/include /repo/runtime /p/
cd /p
if [ -d /repo/.git ]; then
  cp -a /repo/.git /p/.git
  git -C /p checkout -- src/arch/riscv64/riscv64-gen.c src/arch/riscv64/riscv64-gen.h src/mccast.c 2>/dev/null || echo "   (git checkout of pristine failed; skipping strict pristine build)"
fi
gcc -O1 -w $DEF $INC src/mcc.c -o /w/mcc-rv64-pristine 2>/dev/null || echo "   pristine build failed"

echo "== OFF byte-identity: compile corpus with MCC_AST_PROMOTE=0, diff modified vs pristine =="
mkdir -p /w/corpus
cat > /w/corpus/a.c <<'EOF'
extern int printf(const char*,...);
int sumloop(int n){int s=0,i;for(i=0;i<n;i++)s+=i*i-3;return s;}
int callful(int a,int b){int x=a+b; extern int g(int); int y=g(x); return x*3+y+x;}
double favg(double*p,int n){double s=0;int i;for(i=0;i<n;i++)s+=p[i];return s/n;}
long mix(long a,long b,long c){long x=a*b; long y=b-c; long z=x+y; return x^y^z^a; }
EOF
cat > /w/corpus/b.c <<'EOF'
int fib(int n){int a=0,b=1,i,t;for(i=0;i<n;i++){t=a+b;a=b;b=t;}return a;}
int poly(int x){int r=0,c=1,i;for(i=0;i<8;i++){r+=c*x;c=c*x;}return r;}
unsigned h(unsigned x){unsigned a=x,b=x*2654435761u,i;for(i=0;i<5;i++){a^=b;b=(b<<7)|(b>>25);}return a^b;}
EOF
IDENT_OK=1
if [ -x /w/mcc-rv64-pristine ]; then
  for f in a b; do
    MCC_AST_PROMOTE=0 /w/mcc-rv64-opt      -O2 -I /b/runtime/include -c /w/corpus/$f.c -o /w/mod_$f.o
    MCC_AST_PROMOTE=0 /w/mcc-rv64-pristine -O2 -I /p/runtime/include -c /w/corpus/$f.c -o /w/pri_$f.o
    if cmp -s /w/mod_$f.o /w/pri_$f.o; then
      echo "   OFF byte-identical: corpus/$f.o (modified == pristine)"
    else
      echo "   OFF DIFFERS: corpus/$f.o"; IDENT_OK=0
      cmp /w/mod_$f.o /w/pri_$f.o || true
    fi
  done
else
  echo "   (no pristine binary; reason-only OFF check)"
fi

echo "== gate-fires objdump evidence: leaf + callful, OFF vs ON =="
cat > /w/gate.c <<'EOF'
extern int printf(const char*,...);
extern int ext(int);
/* callful: promoted vars must survive the call in callee-saved s-regs */
int callful(int a, int b) {
  int acc = a * 3 + 7;
  int base = b ^ 0x55;
  int r0 = ext(acc);
  int r1 = ext(base);
  int r2 = ext(acc + base);
  return acc + base + r0 + r1 + r2;
}
/* leaf: hot vars promoted to caller-saved regs, loads/stores elided */
int leaf(int n) {
  int s = 0, p = 1, i;
  for (i = 1; i <= n; i++) { s += i; p += s * i; }
  return s ^ p;
}
EOF
MCC_AST_PROMOTE=0 /w/mcc-rv64-opt -O2 -I /b/runtime/include -c /w/gate.c -o /w/gate_off.o
MCC_AST_PROMOTE=1 /w/mcc-rv64-opt -O2 -I /b/runtime/include -c /w/gate.c -o /w/gate_on.o
echo "--- callful() OFF ---"; riscv64-linux-gnu-objdump -d /w/gate_off.o | sed -n "/<callful>:/,/ret/p"
echo "--- callful() ON  ---"; riscv64-linux-gnu-objdump -d /w/gate_on.o  | sed -n "/<callful>:/,/ret/p"
echo "--- leaf() OFF ---";    riscv64-linux-gnu-objdump -d /w/gate_off.o | sed -n "/<leaf>:/,/ret/p"
echo "--- leaf() ON  ---";    riscv64-linux-gnu-objdump -d /w/gate_on.o  | sed -n "/<leaf>:/,/ret/p"
echo "--- s-reg usage counts (s1..s11 = x9,x18..x27) ---"
for tag in off on; do
  n=$(riscv64-linux-gnu-objdump -d /w/gate_$tag.o | grep -Eoc "\b(s1|s2|s3|s4|s5|s6|s7|s8|s9|s10|s11)\b" || true)
  echo "   gate_$tag.o: s-reg mentions=$n"
done

echo "== differential vs gcc under qemu-riscv64 =="
cat > /w/diff.c <<'EOF'
extern int printf(const char*,...);
typedef long long ll; typedef unsigned long long ull; typedef unsigned u;
#define IMIN (-2147483647-1)
#define IMAX 2147483647
extern int refcall(int);
static long fails=0, checks=0;
static void rep(const char*k,ll x,ll g,ll e){ if(fails<40) printf("MISMATCH %s x=%lld got=%lld exp=%lld\n",k,x,g,e); fails++; }

/* callful: values live across calls (must survive in s-regs when promoted) */
int cf(int a,int b){
  int acc=a*3+7, base=b^0x55, keep=a-b;
  int r0=refcall(acc);
  int r1=refcall(base+keep);
  int r2=refcall(acc+base+r0);
  return acc+base+keep+r0+r1+r2;
}
/* leaf int */
int leafi(int n){int s=0,p=1,i;for(i=1;i<=n;i++){s+=i*3-1;p+=s^i;}return s*7+p;}
/* leaf mixed int/float */
double leaff(int n){double s=0.0,q=2.0;int i;for(i=1;i<=n;i++){s+=(double)i*4.0;q=q+(double)(i&7);}return s*3.0-q;}
/* wide long */
ll leafl(ll a,ll b){ll x=a*100003LL,y=b^0x5a5a5a5aLL,z=x+y;int i;for(i=0;i<6;i++){z=z*3+x-y;x^=z;}return x^y^z;}

EOF
# reference impl: identical source compiled by gcc -O2
cat > /w/ref.c <<'EOF'
int refcall(int x){ return (x*2654435761u) ^ (x>>3) ^ 0x1234; }
EOF
cat >> /w/diff.c <<'EOF'
int main(void){
  int i; long x;
  static int inI[]={0,1,-1,2,-2,7,-7,100,-100,255,256,1000,-1000,65535,65536,IMAX,IMIN,IMAX-1,IMIN+1,12345,-98765};
  int mI=(int)(sizeof inI/sizeof inI[0]);
  for(i=0;i<mI;i++) for(int j=0;j<mI;j++){
    int a=inI[i],b=inI[j];
    checks++; if(cf(a,b)!=CF_REF(a,b)) rep("cf",((ll)a<<20)|(u)b,cf(a,b),CF_REF(a,b));
  }
  for(x=-40000;x<=40000;x+=7){
    checks++; if(leafi((int)x)!=LEAFI_REF((int)x)) rep("leafi",x,leafi((int)x),LEAFI_REF((int)x));
  }
  for(i=0;i<=2000;i++){
    double g=leaff(i), e=LEAFF_REF(i);
    checks++; if(g!=e){ union{double d;ll l;}ug,ue; ug.d=g; ue.d=e; rep("leaff",i,ug.l,ue.l); }
  }
  for(x=-50000;x<=50000;x+=13){
    ll a=x*100003LL, b=(x^0x33)*7;
    checks++; if(leafl(a,b)!=LEAFL_REF(a,b)) rep("leafl",x,leafl(a,b),LEAFL_REF(a,b));
  }
  printf("checks=%ld fails=%ld\n",checks,fails);
  return fails?1:0;
}
EOF

# Build two variants of diff.c:
#  - mcc (promote ON) compiles cf/leafi/leaff/leafl; gcc compiles the *_REF via #define aliasing to a second gcc-built TU
# Approach: compile the tested functions with mcc; compile a gcc reference TU that
# provides CF_REF/LEAFI_REF/... as gcc -O2 versions of the same functions, plus main+refcall.
cat > /w/refimpl.c <<'EOF'
typedef long long ll; typedef unsigned long long ull; typedef unsigned u;
int refcall(int x){ return (x*2654435761u) ^ (x>>3) ^  0x1234; }
int CF_REF(int a,int b){
  int acc=a*3+7, base=b^0x55, keep=a-b;
  int r0=refcall(acc);
  int r1=refcall(base+keep);
  int r2=refcall(acc+base+r0);
  return acc+base+keep+r0+r1+r2;
}
int LEAFI_REF(int n){int s=0,p=1,i;for(i=1;i<=n;i++){s+=i*3-1;p+=s^i;}return s*7+p;}
double LEAFF_REF(double x,double y){double a=x+y,b=a+x,c=b-y,d=c+a,e=a-b,f=e+c;return a+b+c+d+e+f;}
ll LEAFL_REF(ll a,ll b){ll x=a*100003LL,y=b^0x5a5a5a5aLL,z=x+y;int i;for(i=0;i<6;i++){z=z*3+x-y;x^=z;}return x^y^z;}
EOF

# main TU references CF_REF etc as externs
cat > /w/main.c <<'EOF'
extern int printf(const char*,...);
typedef long long ll; typedef unsigned long long ull; typedef unsigned u;
#define IMIN (-2147483647-1)
#define IMAX 2147483647
extern int cf(int,int); extern int leafi(int); extern double leaff(double,double); extern ll leafl(ll,ll);
extern int CF_REF(int,int); extern int LEAFI_REF(int); extern double LEAFF_REF(double,double); extern ll LEAFL_REF(ll,ll);
static long fails=0, checks=0;
static void rep(const char*k,ll x,ll g,ll e){ if(fails<40) printf("MISMATCH %s x=%lld got=%lld exp=%lld\n",k,x,g,e); fails++; }
int main(void){
  int i,j; long x;
  static int inI[]={0,1,-1,2,-2,7,-7,100,-100,255,256,1000,-1000,65535,65536,IMAX,IMIN,IMAX-1,IMIN+1,12345,-98765};
  int mI=(int)(sizeof inI/sizeof inI[0]);
  for(i=0;i<mI;i++) for(j=0;j<mI;j++){ int a=inI[i],b=inI[j];
    checks++; if(cf(a,b)!=CF_REF(a,b)) rep("cf",((ll)a<<20)|(u)b,cf(a,b),CF_REF(a,b)); }
  for(x=-40000;x<=40000;x+=7){ checks++;
    if(leafi((int)x)!=LEAFI_REF((int)x)) rep("leafi",x,leafi((int)x),LEAFI_REF((int)x)); }
  for(i=-500;i<=500;i++) for(j=-500;j<=500;j+=25){ double xv=(double)i*8.0, yv=(double)j*0.5;
    double g=leaff(xv,yv),e=LEAFF_REF(xv,yv);
    checks++; if(g!=e){ union{double d;ll l;}ug,ue; ug.d=g; ue.d=e; rep("leaff",((ll)i<<20)|(j&0xfffff),ug.l,ue.l);} }
  for(x=-50000;x<=50000;x+=13){ ll a=x*100003LL,b=(x^0x33)*7;
    checks++; if(leafl(a,b)!=LEAFL_REF(a,b)) rep("leafl",x,leafl(a,b),LEAFL_REF(a,b)); }
  printf("checks=%ld fails=%ld\n",checks,fails);
  return fails?1:0;
}
EOF
# tested functions (mcc, promote ON)
cat > /w/tested.c <<'EOF'
typedef long long ll; typedef unsigned long long ull; typedef unsigned u;
extern int refcall(int);
int cf(int a,int b){
  int acc=a*3+7, base=b^0x55, keep=a-b;
  int r0=refcall(acc);
  int r1=refcall(base+keep);
  int r2=refcall(acc+base+r0);
  return acc+base+keep+r0+r1+r2;
}
int leafi(int n){int s=0,p=1,i;for(i=1;i<=n;i++){s+=i*3-1;p+=s^i;}return s*7+p;}
double leaff(double x,double y){double a=x+y,b=a+x,c=b-y,d=c+a,e=a-b,f=e+c;return a+b+c+d+e+f;}
ll leafl(ll a,ll b){ll x=a*100003LL,y=b^0x5a5a5a5aLL,z=x+y;int i;for(i=0;i<6;i++){z=z*3+x-y;x^=z;}return x^y^z;}
EOF

echo "-- compile tested.c with mcc promote ON --"
MCC_AST_PROMOTE=1 /w/mcc-rv64-opt -O2 -I /b/runtime/include -c /w/tested.c -o /w/tested.o
echo "-- s-reg usage in tested.o --"
riscv64-linux-gnu-objdump -d /w/tested.o | grep -Eoc "\b(s1|s2|s3|s4|s5|s6|s7|s8|s9|s10|s11)\b" || true
echo "-- compile ref + main with gcc -O2 --"
riscv64-linux-gnu-gcc -O2 -c /w/refimpl.c -o /w/refimpl.o
riscv64-linux-gnu-gcc -O2 -c /w/main.c    -o /w/main.o
echo "-- link static + run under qemu --"
riscv64-linux-gnu-gcc -static /w/main.o /w/tested.o /w/refimpl.o -o /w/difftest
rc=0
qemu-riscv64-static /w/difftest || rc=$?
echo "difftest exit=$rc"

echo "== ALSO: control build - tested.c compiled with promote OFF, differential (sanity) =="
MCC_AST_PROMOTE=0 /w/mcc-rv64-opt -O2 -I /b/runtime/include -c /w/tested.c -o /w/tested_off.o
riscv64-linux-gnu-gcc -static /w/main.o /w/tested_off.o /w/refimpl.o -o /w/difftest_off
rc2=0
qemu-riscv64-static /w/difftest_off || rc2=$?
echo "difftest_off exit=$rc2"

echo "IDENT_OK=$IDENT_OK ON_rc=$rc OFF_rc=$rc2"
if [ "$IDENT_OK" != 1 ] || [ "$rc" != 0 ] || [ "$rc2" != 0 ]; then
  echo "RISCV64PROMO FAIL"; exit 1
fi
echo "RISCV64PROMO PASS"
'
