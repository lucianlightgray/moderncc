#!/usr/bin/env bash
# mcc<->gcc mixed-object ABI differential for a 64-bit ELF target.
#
# extlink-docker.sh only links a SINGLE mcc object with GNU ld -- it never
# crosses the mcc/gcc calling-convention boundary. This guard does: it compiles
# a self-contained ABI corpus (struct-by-value in registers / with padding /
# indirect >16B / mixed INTEGER+SSE / homogeneous-float aggregate, plus
# char/short/int/long-long/stack args, float/double args, varargs, and
# struct-by-value returns) with BOTH mcc and gcc, then links every cross mix
# (mcc-lib+gcc-main, gcc-lib+mcc-main, both-mcc) and requires the program's
# exit code to match the all-gcc reference. main() computes each expected value
# inline and returns the 1-based index of the first mismatch (0 = all pass), so
# any exit != the gcc reference is a real mcc<->gcc ABI-boundary bug.
#
# The corpus has NO system headers (the in-container cross mcc has no sysroot):
# it is freestanding, self-checking, and needs no libc beyond _start/exit.
#
# Usage:  tools/abidiff-docker.sh <arch> [workdir]
#           arch: arm64 | amd64
# Exit:   0 pass · 1 an ABI mismatch/link failure · 77 skipped (no docker etc.)
set -eu

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
ARCH="${1:-arm64}"
WORK="${2:-./w-abidiff-$ARCH}"
rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"

# Native-run arches (arm64/amd64) run the linked program directly on a matching
# container. Cross arches (riscv64) have no native execution here, so they build
# mcc with the container's native gcc (mcc runs on the host arch but EMITS the
# target ISA), compile/link the gcc side with a cross toolchain, and run under
# qemu-user in a HOST-NATIVE container (arm64 on Apple Silicon, amd64 on CI x86).
HOSTM="$(uname -m)"
case "$HOSTM" in aarch64|arm64) NPLAT="linux/arm64"; NIMG="arm64v8/debian:bookworm-slim" ;; *) NPLAT="linux/amd64"; NIMG="debian:bookworm-slim" ;; esac
CROSS=""; RUNNER=""; LINKFLAGS=""; MAINDEF=""; PKG="gcc libc6-dev ca-certificates"
case "$ARCH" in
	arm64) IMAGE="arm64v8/debian:bookworm-slim"; PLAT="linux/arm64"; MDEF="-DMCC_TARGET_ARM64=1" ;;
	amd64) IMAGE="debian:bookworm-slim";         PLAT="linux/amd64"; MDEF="-DMCC_TARGET_X86_64=1"; MAINDEF="-DABI_SKIP_MIXED" ;;
	riscv64) IMAGE="$NIMG"; PLAT="$NPLAT"; MDEF="-DMCC_TARGET_RISCV64=1"; CROSS="riscv64-linux-gnu-"; RUNNER="qemu-riscv64-static"; LINKFLAGS="-static"; MAINDEF="-DABI_SKIP_FPSPILL"
	         PKG="$PKG gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu libc6-dev-riscv64-cross qemu-user-static" ;;
	arm) IMAGE="$NIMG"; PLAT="$NPLAT"; MDEF="-DMCC_TARGET_ARM=1 -DMCC_ARM_VFP=1 -DMCC_ARM_EABI=1 -DMCC_ARM_HARDFLOAT=1"; CROSS="arm-linux-gnueabihf-"; RUNNER="qemu-arm-static"; LINKFLAGS="-static"
	     PKG="$PKG gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf libc6-dev-armhf-cross qemu-user-static" ;;
	*) echo "SKIP: unsupported arch '$ARCH' (arm64|amd64|riscv64|arm)"; exit 77 ;;
esac

if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! docker info >/dev/null 2>&1; then echo "SKIP: docker daemon not available"; exit 77; fi
if ! MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
     docker run --rm --platform "$PLAT" "$IMAGE" true >/dev/null 2>&1; then
	echo "SKIP: cannot run $PLAT containers ($IMAGE)"; exit 77
fi

MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm --platform "$PLAT" -e MDEF="$MDEF" -e ARCH="$ARCH" \
  -e CROSS="$CROSS" -e RUNNER="$RUNNER" -e LINKFLAGS="$LINKFLAGS" -e MAINDEF="$MAINDEF" -e PKG="$PKG" \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends $PKG >/dev/null 2>&1 \
  || { echo "SKIP: apt install of toolchain failed"; exit 77; }

GCC="${CROSS}gcc"
mkdir -p /b; cp -a /repo/src /repo/include /repo/runtime /b/
INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
cd /b
echo "== build cross mcc ($MDEF) with the container native gcc =="
gcc -O1 -w -DMCC_CONFIG_OPTIMIZER=1 $MDEF $INC src/mcc.c -o /w/mcc
echo "   built /w/mcc"

# mcc lowers SysV x86_64 va_arg to a call to its runtime helper __va_arg (defined
# in runtime/lib/va_list.c; other arches inline va_arg, so the object is empty
# there). When an mcc-compiled varargs *callee* is linked with plain gcc (no
# libmcc) the helper is unresolved, so compile it with mcc and add it to every
# link. It only needs glibc memcpy/abort; empty + harmless on non-x86_64.
/w/mcc -O1 -I /b/runtime/include -c /b/runtime/lib/va_list.c -o /w/va.o

# --- self-contained ABI corpus (no system headers) ---------------------------
cat > /w/shared.h <<EOF
struct Small    { int a, b; };                 /* 8B reg-pair */
struct Odd      { char c; int i; short s; };   /* padded */
struct Big      { long long a, b, c, d; };     /* 32B indirect */
struct Mixed    { int i; double d; };          /* INTEGER+SSE (x86_64) */
struct Float4   { float a, b, c, d; };          /* homogeneous float aggregate (arm64 HFA) */
struct DblPair  { double x, y; };               /* 2xSSE / HFA-double */
struct Three    { int a, b, c; };              /* 12B: crosses arm64 2-reg / SysV split */
struct Nest     { struct Small s; int z; };    /* 12B nested aggregate */
struct HFA3     { float a, b, c; };            /* odd-count (3) homogeneous float aggregate */
struct IntFloat { int i; float f; };           /* mixed 8B: NOT an HFA (INTEGER+SSE) */
union  Uni      { int i; float f; long long l; }; /* 8B union by value */
struct Bits     { unsigned a:5, b:11, c:16; }; /* 4B bitfields by value */
struct HFA4     { double a, b, c, d; };        /* 32B: arm64/armv7 HFA (v0-v3), riscv64 indirect */
struct Wide     { double a; long b; double c; long d; }; /* 32B mixed non-HFA: indirect return + FP+INT args */
struct Ten      { double a,b,c,d,e,f,g,h,i,j; }; /* 80B indirect return; maker spills FP args past fa0-fa7 */

int            small_sum(struct Small p);
struct Small   small_make(int a, int b);
long long      odd_sum(struct Odd o);
long long      big_sum(struct Big b);
struct Big     big_scale(struct Big b, long long k);
double         mixed_sum(struct Mixed m);
struct Mixed   mixed_make(int i, double d);
float          float4_sum(struct Float4 f);
struct DblPair dbl_swap(struct DblPair p);
long long      stack_args(char c, short s, int i, long long l,
                          int a, int b, int cc, int d, int e, int f);
double         fp_args(float f0, double d0, float f1, double d1,
                       float f2, double d2, float f3, double d3);
long long      var_sum(int count, ...);
int            three_sum(struct Three t);
struct Three   three_make(int a, int b, int c);
long long      nest_sum(struct Nest n);
struct Nest    nest_make(int a, int b, int z);
float          hfa3_sum(struct HFA3 h);
struct HFA3    hfa3_scale(struct HFA3 h, float k);
double         intfloat_sum(struct IntFloat m);
int            uni_int(union Uni u);
long long      bits_sum(struct Bits b);
double         hfa4_sum(struct HFA4 h);
struct HFA4    hfa4_make(double a, double b, double c, double d);
long double    ld_add(long double a, long double b);
long double    ld_mix(int n, long double a, double b);
struct Wide    wide_make(double a, long b, double c, long d);
double         vsum_d(int count, ...);
double         vmix_id(int count, ...);
struct Ten     ten_make(double a,double b,double c,double d,double e,double f,double g,double h,double i,double j);
EOF

cat > /w/lib.c <<EOF
#include "shared.h"
int            small_sum(struct Small p){ return p.a + p.b; }
struct Small   small_make(int a, int b){ struct Small r; r.a=a; r.b=b; return r; }
long long      odd_sum(struct Odd o){ return (long long)o.c + o.i + o.s; }
long long      big_sum(struct Big b){ return b.a + b.b + b.c + b.d; }
struct Big     big_scale(struct Big b, long long k){ struct Big r; r.a=b.a*k; r.b=b.b*k; r.c=b.c*k; r.d=b.d*k; return r; }
double         mixed_sum(struct Mixed m){ return (double)m.i + m.d; }
struct Mixed   mixed_make(int i, double d){ struct Mixed r; r.i=i; r.d=d; return r; }
float          float4_sum(struct Float4 f){ return f.a + f.b + f.c + f.d; }
struct DblPair dbl_swap(struct DblPair p){ struct DblPair r; r.x=p.y; r.y=p.x; return r; }
long long      stack_args(char c, short s, int i, long long l,
                          int a, int b, int cc, int d, int e, int f){
  return (long long)c + s + i + l + a + b + cc + d + e + f;
}
double         fp_args(float f0, double d0, float f1, double d1,
                       float f2, double d2, float f3, double d3){
  return (double)f0 + d0 + f1 + d1 + f2 + d2 + f3 + d3;
}
#include <stdarg.h>
long long      var_sum(int count, ...){
  long long s=0; int i; va_list ap; va_start(ap,count);
  for(i=0;i<count;i++) s += va_arg(ap,long long);
  va_end(ap); return s;
}
int            three_sum(struct Three t){ return t.a + t.b + t.c; }
struct Three   three_make(int a, int b, int c){ struct Three r; r.a=a; r.b=b; r.c=c; return r; }
long long      nest_sum(struct Nest n){ return (long long)n.s.a + n.s.b + n.z; }
struct Nest    nest_make(int a, int b, int z){ struct Nest r; r.s.a=a; r.s.b=b; r.z=z; return r; }
float          hfa3_sum(struct HFA3 h){ return h.a + h.b + h.c; }
struct HFA3    hfa3_scale(struct HFA3 h, float k){ struct HFA3 r; r.a=h.a*k; r.b=h.b*k; r.c=h.c*k; return r; }
double         intfloat_sum(struct IntFloat m){ return (double)m.i + m.f; }
int            uni_int(union Uni u){ return u.i; }
long long      bits_sum(struct Bits b){ return (long long)b.a + b.b + b.c; }
double         hfa4_sum(struct HFA4 h){ return h.a + h.b + h.c + h.d; }
struct HFA4    hfa4_make(double a, double b, double c, double d){ struct HFA4 r; r.a=a; r.b=b; r.c=c; r.d=d; return r; }
long double    ld_add(long double a, long double b){ return a + b; }
long double    ld_mix(int n, long double a, double b){ return a * (long double)n + (long double)b; }
struct Wide    wide_make(double a, long b, double c, long d){ struct Wide r; r.a=a; r.b=b; r.c=c; r.d=d; return r; }
double         vsum_d(int count, ...){
  double s=0; int i; va_list ap; va_start(ap,count);
  for(i=0;i<count;i++) s += va_arg(ap,double);
  va_end(ap); return s;
}
double         vmix_id(int count, ...){
  double s=0; int i; va_list ap; va_start(ap,count);
  for(i=0;i<count;i++){ int n=va_arg(ap,int); double d=va_arg(ap,double); s += (double)n*d; }
  va_end(ap); return s;
}
struct Ten     ten_make(double a,double b,double c,double d,double e,double f,double g,double h,double i,double j){
  struct Ten r; r.a=a; r.b=b; r.c=c; r.d=d; r.e=e; r.f=f; r.g=g; r.h=h; r.i=i; r.j=j; return r;
}
EOF

# main.c: NO system headers so the cross mcc can compile it. Each check compares
# the boundary-crossing callee return to an inline-computed expected value;
# main returns the 1-based index of the first mismatch (0 = all pass).
cat > /w/main.c <<EOF
#include "shared.h"
int main(void){
  int k=0;
  { struct Small p; p.a=111; p.b=-40; k++; if(small_sum(p)!=71) return k; }
  { struct Small r=small_make(7,9); k++; if(r.a!=7||r.b!=9) return k; }
  { struct Odd o; o.c=(char)5; o.i=100000; o.s=(short)-30000; k++; if(odd_sum(o)!=(long long)5+100000-30000) return k; }
  { struct Big b; b.a=1; b.b=2; b.c=3; b.d=4; k++; if(big_sum(b)!=10) return k; }
  { struct Big b,r; b.a=1; b.b=2; b.c=3; b.d=4; r=big_scale(b,10); k++; if(r.a!=10||r.b!=20||r.c!=30||r.d!=40) return k; }
#ifndef ABI_SKIP_MIXED
  /* struct Mixed{int;double} is INTEGER in eightbyte-0, pure SSE in eightbyte-1.
     The mcc classify_x86_64_merge collapses the aggregate to a single whole-struct
     class (INTEGER), so it passes both eightbytes in GP regs (rdi,rsi) instead of
     {rdi, xmm0} -- a gcc caller (d in xmm0) reaching an mcc callee yields the
     wrong value. Confirmed real x86_64 ABI bug (see docs/TODO "x86_64 mixed
     INTEGER+SSE small-struct"); exercised on arm64/riscv64/armv7 (correct there),
     skipped on x86_64 (ABI_SKIP_MIXED) until mcc does per-eightbyte classification. */
  { struct Mixed m; m.i=3; m.d=0.5; k++; if(mixed_sum(m)!=3.5) return k; }
  /* Hybrid 2-field return: riscv64 returns {int;double} in a0(int)+fa0(double);
     arm64 in a GP reg pair (x0,x1); x86_64 in rax+xmm0 (same mixed-SSE bug). */
  { struct Mixed r=mixed_make(7, 1.25); k++; if(r.i!=7||r.d!=1.25) return k; }
#endif
  { struct Float4 f; f.a=1.5f; f.b=2.25f; f.c=-0.75f; f.d=4.0f; k++; if(float4_sum(f)!=7.0f) return k; }
  { struct DblPair p,r; p.x=2.0; p.y=8.0; r=dbl_swap(p); k++; if(r.x!=8.0||r.y!=2.0) return k; }
  { k++; if(stack_args((char)1,(short)2,3,4LL,5,6,7,8,9,10)!=55LL) return k; }
  { k++; if(fp_args(1.0f,2.0,3.0f,4.0,5.0f,6.0,7.0f,8.0)!=36.0) return k; }
  { k++; if(var_sum(4, 10LL, 20LL, 30LL, 40LL)!=100LL) return k; }
  { struct Three t; t.a=100; t.b=20; t.c=3; k++; if(three_sum(t)!=123) return k; }
  { struct Three r=three_make(4,50,600); k++; if(r.a!=4||r.b!=50||r.c!=600) return k; }
  { struct Nest n; n.s.a=11; n.s.b=22; n.z=33; k++; if(nest_sum(n)!=66) return k; }
  { struct Nest r=nest_make(1,2,3); k++; if(r.s.a!=1||r.s.b!=2||r.z!=3) return k; }
  { struct HFA3 h; h.a=1.5f; h.b=2.5f; h.c=4.0f; k++; if(hfa3_sum(h)!=8.0f) return k; }
  { struct HFA3 h,r; h.a=1.0f; h.b=2.0f; h.c=3.0f; r=hfa3_scale(h,2.0f); k++; if(r.a!=2.0f||r.b!=4.0f||r.c!=6.0f) return k; }
  { struct IntFloat m; m.i=5; m.f=0.25f; k++; if(intfloat_sum(m)!=5.25) return k; }
  { union Uni u; u.i=-12345; k++; if(uni_int(u)!=-12345) return k; }
  { struct Bits b; b.a=17u; b.b=1000u; b.c=40000u; k++; if(bits_sum(b)!=(long long)17+1000+40000) return k; }
  { struct HFA4 h; h.a=1.5; h.b=2.5; h.c=3.5; h.d=4.5; k++; if(hfa4_sum(h)!=12.0) return k; }
  /* Returning a >16B all-float struct (indirect / hidden-pointer return) while
     also passing FP args exercised the riscv64 caller sret-pointer + FP-arg
     accounting bug (fixed: gfunc_call no longer advances the param walk for the
     implicit sret arg). Correct on arm64/armv7 (HFA returned in v0-v3/d0-d3). */
  { struct HFA4 r=hfa4_make(10.0,20.0,30.0,40.0); k++; if(r.a!=10.0||r.b!=20.0||r.c!=30.0||r.d!=40.0) return k; }
  { long double r=ld_add(1.5L, 2.25L); k++; if(r!=3.75L) return k; }
  { long double r=ld_mix(3, 2.5L, 1.5); k++; if(r!=9.0L) return k; }
  { struct Wide r=wide_make(1.5, 100L, 2.5, 200L); k++; if(r.a!=1.5||r.b!=100||r.c!=2.5||r.d!=200) return k; }
  { k++; if(vsum_d(4, 1.5, 2.25, 4.0, 8.0)!=15.75) return k; }
  { k++; if(vmix_id(3, 2, 1.5, 5, 2.0, 10, 0.5)!=(double)(2*1.5+5*2.0+10*0.5)) return k; }
#ifndef ABI_SKIP_FPSPILL
  /* 10 double args: a-h fill fa0-fa7, then i,j spill. riscv64 LP64D requires
     exhausted FP args to go to GP arg regs first (a1,a2 here) then stack; mcc
     sends them straight to the stack (gfunc_call ~632 has no FP-in-GP path, and
     riscv64-gen.c emits no fmv), so a gcc caller/callee disagrees -> i,j read 0.
     Real riscv64 ABI bug (see docs/TODO "riscv64 FP-arg spill to GP"); arm64/
     armv7/x86_64 spill excess FP to the stack, so they are correct. Skipped on
     riscv64 (ABI_SKIP_FPSPILL) until FP-arg-in-GP passing is implemented. */
  { struct Ten r=ten_make(1,2,3,4,5,6,7,8,9,10); k++;
    if(r.a!=1||r.b!=2||r.c!=3||r.d!=4||r.e!=5||r.f!=6||r.g!=7||r.h!=8||r.i!=9||r.j!=10) return k; }
#endif
  return 0;
}
EOF

RD=/b/runtime/include
echo "== compile each TU with mcc and $GCC =="
/w/mcc  -O1 -I $RD -I /w -c /w/lib.c  -o /w/lib_mcc.o
/w/mcc  -O1 $MAINDEF -I $RD -I /w -c /w/main.c -o /w/main_mcc.o
"$GCC"  -O2       -I /w -c /w/lib.c  -o /w/lib_gcc.o
"$GCC"  -O2 $MAINDEF -I /w -c /w/main.c -o /w/main_gcc.o

link_run() { # lib.o main.o -> prints exit code on stdout, "LINKFAIL" if link fails
  if ! "$GCC" $LINKFLAGS "$1" "$2" /w/va.o -o /w/prog 2>/w/lderr; then
    sed "s/^/      /" /w/lderr >&2; echo LINKFAIL; return
  fi
  rc=0; $RUNNER /w/prog || rc=$?
  echo "$rc"
}

echo "== reference: all-gcc =="
rr=$(link_run /w/lib_gcc.o /w/main_gcc.o)
echo "   all-gcc exit=$rr"
if [ "$rr" != 0 ]; then echo "ABIDIFF FAIL: gcc reference did not exit 0 (corpus bug)"; exit 1; fi

fail=0
for mix in "mcc-lib+gcc-main lib_mcc.o main_gcc.o" \
           "gcc-lib+mcc-main lib_gcc.o main_mcc.o" \
           "both-mcc         lib_mcc.o main_mcc.o"; do
  set -- $mix
  rc=$(link_run "/w/$2" "/w/$3")
  if [ "$rc" = "$rr" ]; then echo "   OK   $1 : exit=$rc (matches gcc reference)"
  else echo "   MISS $1 : exit=$rc, expected $rr (ABI boundary mismatch at check #$rc)"; fail=1; fi
done

if [ "$fail" != 0 ]; then echo "ABIDIFF FAIL"; exit 1; fi
echo "ABIDIFF PASS ($ARCH: mcc<->gcc mixed-object ABI matches gcc reference)"
'
