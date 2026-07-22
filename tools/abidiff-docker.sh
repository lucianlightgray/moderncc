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

case "$ARCH" in
	arm64) IMAGE="arm64v8/debian:bookworm-slim"; PLAT="linux/arm64"; MDEF="-DMCC_TARGET_ARM64=1" ;;
	amd64) IMAGE="debian:bookworm-slim";         PLAT="linux/amd64"; MDEF="-DMCC_TARGET_X86_64=1" ;;
	*) echo "SKIP: unsupported arch '$ARCH' (arm64|amd64)"; exit 77 ;;
esac

if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! docker info >/dev/null 2>&1; then echo "SKIP: docker daemon not available"; exit 77; fi
if ! MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
     docker run --rm --platform "$PLAT" "$IMAGE" true >/dev/null 2>&1; then
	echo "SKIP: cannot run $PLAT containers ($IMAGE)"; exit 77
fi

MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm --platform "$PLAT" -e MDEF="$MDEF" -e ARCH="$ARCH" \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends gcc libc6-dev ca-certificates >/dev/null 2>&1 \
  || { echo "SKIP: apt install of gcc failed"; exit 77; }

mkdir -p /b; cp -a /repo/src /repo/include /repo/runtime /b/
INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
cd /b
echo "== build native-arch cross mcc ($MDEF) =="
gcc -O1 -w -DMCC_CONFIG_OPTIMIZER=1 $MDEF $INC src/mcc.c -o /w/mcc
echo "   built /w/mcc"

# --- self-contained ABI corpus (no system headers) ---------------------------
cat > /w/shared.h <<EOF
struct Small    { int a, b; };                 /* 8B reg-pair */
struct Odd      { char c; int i; short s; };   /* padded */
struct Big      { long long a, b, c, d; };     /* 32B indirect */
struct Mixed    { int i; double d; };          /* INTEGER+SSE (x86_64) */
struct Float4   { float a, b, c, d; };          /* homogeneous float aggregate (arm64 HFA) */
struct DblPair  { double x, y; };               /* 2xSSE / HFA-double */

int            small_sum(struct Small p);
struct Small   small_make(int a, int b);
long long      odd_sum(struct Odd o);
long long      big_sum(struct Big b);
struct Big     big_scale(struct Big b, long long k);
double         mixed_sum(struct Mixed m);
float          float4_sum(struct Float4 f);
struct DblPair dbl_swap(struct DblPair p);
long long      stack_args(char c, short s, int i, long long l,
                          int a, int b, int cc, int d, int e, int f);
double         fp_args(float f0, double d0, float f1, double d1,
                       float f2, double d2, float f3, double d3);
long long      var_sum(int count, ...);
EOF

cat > /w/lib.c <<EOF
#include "shared.h"
int            small_sum(struct Small p){ return p.a + p.b; }
struct Small   small_make(int a, int b){ struct Small r; r.a=a; r.b=b; return r; }
long long      odd_sum(struct Odd o){ return (long long)o.c + o.i + o.s; }
long long      big_sum(struct Big b){ return b.a + b.b + b.c + b.d; }
struct Big     big_scale(struct Big b, long long k){ struct Big r; r.a=b.a*k; r.b=b.b*k; r.c=b.c*k; r.d=b.d*k; return r; }
double         mixed_sum(struct Mixed m){ return (double)m.i + m.d; }
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
  { struct Mixed m; m.i=3; m.d=0.5; k++; if(mixed_sum(m)!=3.5) return k; }
  { struct Float4 f; f.a=1.5f; f.b=2.25f; f.c=-0.75f; f.d=4.0f; k++; if(float4_sum(f)!=7.0f) return k; }
  { struct DblPair p,r; p.x=2.0; p.y=8.0; r=dbl_swap(p); k++; if(r.x!=8.0||r.y!=2.0) return k; }
  { k++; if(stack_args((char)1,(short)2,3,4LL,5,6,7,8,9,10)!=55LL) return k; }
  { k++; if(fp_args(1.0f,2.0,3.0f,4.0,5.0f,6.0,7.0f,8.0)!=36.0) return k; }
  { k++; if(var_sum(4, 10LL, 20LL, 30LL, 40LL)!=100LL) return k; }
  return 0;
}
EOF

RD=/b/runtime/include
echo "== compile each TU with mcc and gcc =="
/w/mcc  -O1 -I $RD -I /w -c /w/lib.c  -o /w/lib_mcc.o
/w/mcc  -O1 -I $RD -I /w -c /w/main.c -o /w/main_mcc.o
gcc     -O2       -I /w -c /w/lib.c  -o /w/lib_gcc.o
gcc     -O2       -I /w -c /w/main.c -o /w/main_gcc.o

link_run() { # lib.o main.o -> prints exit code on stdout, "LINKFAIL" if link fails
  if ! gcc "$1" "$2" -o /w/prog 2>/w/lderr; then
    sed "s/^/      /" /w/lderr >&2; echo LINKFAIL; return
  fi
  rc=0; /w/prog || rc=$?
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
