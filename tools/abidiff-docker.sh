#!/usr/bin/env bash
set -eu
. "$(dirname "$0")/dockergate.sh"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
ARCH="${1:-arm64}"
WORK="${2:-./w-abidiff-$ARCH}"
rm -rf "$WORK"; mkdir -p "$WORK"
dg_need_mount "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"

HOSTM="$(uname -m)"
case "$HOSTM" in aarch64|arm64) NPLAT="linux/arm64"; NIMG="arm64v8/debian:bookworm-slim" ;; *) NPLAT="linux/amd64"; NIMG="debian:bookworm-slim" ;; esac
CROSS=""; RUNNER=""; LINKFLAGS=""; MAINDEF=""; PKG="gcc libc6-dev ca-certificates"
case "$ARCH" in
	arm64) IMAGE="arm64v8/debian:bookworm-slim"; PLAT="linux/arm64"; MDEF="-DMCC_TARGET_ARM64=1" ;;
	amd64) IMAGE="debian:bookworm-slim";         PLAT="linux/amd64"; MDEF="-DMCC_TARGET_X86_64=1" ;;
	riscv64) IMAGE="$NIMG"; PLAT="$NPLAT"; MDEF="-DMCC_TARGET_RISCV64=1"; CROSS="riscv64-linux-gnu-"; RUNNER="qemu-riscv64-static"; LINKFLAGS="-static"
	         PKG="$PKG gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu libc6-dev-riscv64-cross qemu-user-static" ;;
	arm) IMAGE="$NIMG"; PLAT="$NPLAT"; MDEF="-DMCC_TARGET_ARM=1 -DMCC_ARM_VFP=1 -DMCC_ARM_EABI=1 -DMCC_ARM_HARDFLOAT=1"; CROSS="arm-linux-gnueabihf-"; RUNNER="qemu-arm-static"; LINKFLAGS="-static"
	     PKG="$PKG gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf libc6-dev-armhf-cross qemu-user-static" ;;
	*) echo "SKIP: unsupported arch '$ARCH' (arm64|amd64|riscv64|arm)"; exit 77 ;;
esac

dg_need_docker
dg_need_platform "$PLAT" "$IMAGE"

dg_docker run --rm --platform "$PLAT" -e MDEF="$MDEF" -e ARCH="$ARCH" \
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

/w/mcc -O1 -I /b/runtime/include -c /b/runtime/lib/va_list.c -o /w/va.o

cat > /w/shared.h <<EOF
struct Small    { int a, b; };
struct Odd      { char c; int i; short s; };
struct Big      { long long a, b, c, d; };
struct Mixed    { int i; double d; };
struct Mixed2   { double d; int i; };
struct M3       { float f; long l; };
struct FA       { float f[2]; int i; };
struct F4A      { float f[4]; };
struct D2A      { double d[2]; };
struct In2      { float a, b; };
struct NHFA     { struct In2 p; float c; };
struct DNest    { struct { double x; } a; double b; };
struct SArr     { struct In2 v[2]; };
struct BoolM    { _Bool ok; double d; };
struct S3       { char a,b,c; };
struct S7       { char a,b,c,d,e,f,g; };
struct SF1      { float f; };
struct LD1      { long double ld; };
struct FB       { float f; int b:20; };
struct DB       { double d; int b:20; };
struct FCS      { float _Complex z; };
struct DFI      { double d; float f; int i; };
struct Float4   { float a, b, c, d; };
struct DblPair  { double x, y; };
struct Three    { int a, b, c; };
struct Nest     { struct Small s; int z; };
struct HFA3     { float a, b, c; };
struct IntFloat { int i; float f; };
union  Uni      { int i; float f; long long l; };
struct Bits     { unsigned a:5, b:11, c:16; };
struct HFA4     { double a, b, c, d; };
struct Wide     { double a; long b; double c; long d; };
struct Ten      { double a,b,c,d,e,f,g,h,i,j; };
struct A16      { long long a, b; } __attribute__((aligned(16)));
struct Packed   { char a; long long b; int c; } __attribute__((packed));
struct PackS    { char a; int b; } __attribute__((packed));
struct PackN    { char x; struct PackS s; } __attribute__((packed));
struct GData    { int a; char b; double c; long long d[3]; long e; };
extern struct GData g_data;
extern __thread long g_tls;
long tls_get(void); void tls_set(long v);
extern __thread long g_tls_z;
extern int the_alias;
long get_wk(void); long get_alias(void); long get_tlsz(void);
struct P16      { long long a, b; };
struct BF       { unsigned a:3; int b:5; unsigned c:20; long long d:40; int e:12; };

int            small_sum(struct Small p);
struct Small   small_make(int a, int b);
long long      odd_sum(struct Odd o);
long long      big_sum(struct Big b);
struct Big     big_scale(struct Big b, long long k);
double         mixed_sum(struct Mixed m);
struct Mixed   mixed_make(int i, double d);
double         mixed2_sum(struct Mixed2 m);
struct Mixed2  mixed2_make(double d, int i);
long           m3_sum(struct M3 m);
struct Mixed   mixed_after(int a,int b,int c,int d,int e, struct Mixed m);
double         fa_sum(struct FA x);
float          f4a_sum(struct F4A x);
struct F4A     f4a_mk(float a,float b,float c,float d);
double         d2a_sum(struct D2A x);
struct D2A     d2a_mk(double a,double b);
float          nhfa_sum(struct NHFA x);
struct NHFA    nhfa_mk(float a,float b,float c);
double         dnest_sum(struct DNest x);
float          sarr_sum(struct SArr x);
double         boolm_sum(struct BoolM x);
long           s3_sum(struct S3 x);
long           s7_sum(struct S7 x);
float          sf1_sum(struct SF1 x);
struct SF1     sf1_mk(float f);
long double    ld1_sum(struct LD1 x);
double         fb_sum(struct FB x);
double         db_sum(struct DB x);
float          fcs_re(struct FCS x);
double         dfi_sum(struct DFI x);
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
long long      a16_after_int(int pad, struct A16 s);
long long      straddle(long long a, long long b, long long c, long long d,
                        long long e, long long f, long long g, struct P16 s);
signed char    sc_make(int x);
unsigned char  uc_make(int x);
short          sh_make(int x);
unsigned short ush_make(int x);
long long      packed_sum(struct Packed p);
struct Packed  packed_make(char a, long long b, int c);
long long      packs_sum(struct PackS p);
struct PackS   packs_make(char a, int b);
long long      packn_sum(struct PackN p);
float _Complex  cx_cf_id(float _Complex z);
double _Complex cx_cd_id(double _Complex z);
double _Complex cx_cd_after7(double a,double b,double c,double d,double e,double f,double g, double _Complex z);
float _Complex  cx_cf_after7(float a,float b,float c,float d,float e,float f,float g, float _Complex z);
double hfx_nsrn(double a,double b,double c,double d,double e,double f,double g, struct DblPair s, double x);
double hfx_full(double a,double b,double c,double d,double e,double f,double g,double h, struct Float4 s, double x);
EOF

cat > /w/lib.c <<EOF
#include "shared.h"
struct GData   g_data = { 7, 88, 2.5, {100, 200, 300}, -99 };
__thread long  g_tls = 555;
long           tls_get(void){ return g_tls; }
void           tls_set(long v){ g_tls = v; }
__thread long  g_tls_z;
long           get_tlsz(void){ return g_tls_z; }
__attribute__((weak)) int wk_ovr = 111;
int            alias_target = 42;
extern int     the_alias __attribute__((alias("alias_target")));
long           get_wk(void){ return wk_ovr; }
long           get_alias(void){ return the_alias; }
int            small_sum(struct Small p){ return p.a + p.b; }
struct Small   small_make(int a, int b){ struct Small r; r.a=a; r.b=b; return r; }
long long      odd_sum(struct Odd o){ return (long long)o.c + o.i + o.s; }
long long      big_sum(struct Big b){ return b.a + b.b + b.c + b.d; }
struct Big     big_scale(struct Big b, long long k){ struct Big r; r.a=b.a*k; r.b=b.b*k; r.c=b.c*k; r.d=b.d*k; return r; }
double         mixed_sum(struct Mixed m){ return (double)m.i + m.d; }
struct Mixed   mixed_make(int i, double d){ struct Mixed r; r.i=i; r.d=d; return r; }
double         mixed2_sum(struct Mixed2 m){ return m.d + (double)m.i; }
struct Mixed2  mixed2_make(double d, int i){ struct Mixed2 r; r.d=d; r.i=i; return r; }
long           m3_sum(struct M3 m){ return (long)m.f + m.l; }
struct Mixed   mixed_after(int a,int b,int c,int d,int e, struct Mixed m){ struct Mixed r; r.i=m.i+a+b+c+d+e; r.d=m.d; return r; }
double         fa_sum(struct FA x){ return (double)x.f[0]+x.f[1]+x.i; }
float          f4a_sum(struct F4A x){ return x.f[0]+x.f[1]+x.f[2]+x.f[3]; }
struct F4A     f4a_mk(float a,float b,float c,float d){ struct F4A r; r.f[0]=a;r.f[1]=b;r.f[2]=c;r.f[3]=d; return r; }
double         d2a_sum(struct D2A x){ return x.d[0]+x.d[1]; }
struct D2A     d2a_mk(double a,double b){ struct D2A r; r.d[0]=a;r.d[1]=b; return r; }
float          nhfa_sum(struct NHFA x){ return x.p.a+x.p.b+x.c; }
struct NHFA    nhfa_mk(float a,float b,float c){ struct NHFA r; r.p.a=a;r.p.b=b;r.c=c; return r; }
double         dnest_sum(struct DNest x){ return x.a.x+x.b; }
float          sarr_sum(struct SArr x){ return x.v[0].a+x.v[0].b+x.v[1].a+x.v[1].b; }
double         boolm_sum(struct BoolM x){ return (x.ok?1.0:0.0)+x.d; }
long           s3_sum(struct S3 x){ return (long)x.a+x.b+x.c; }
long           s7_sum(struct S7 x){ return (long)x.a+x.b+x.c+x.d+x.e+x.f+x.g; }
float          sf1_sum(struct SF1 x){ return x.f; }
struct SF1     sf1_mk(float f){ struct SF1 r; r.f=f; return r; }
long double    ld1_sum(struct LD1 x){ return x.ld; }
double         fb_sum(struct FB x){ return (double)x.f + x.b; }
double         db_sum(struct DB x){ return x.d + x.b; }
float          fcs_re(struct FCS x){ return __real__ x.z; }
double         dfi_sum(struct DFI x){ return x.d + x.f + x.i; }
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
long long      a16_after_int(int pad, struct A16 s){ return pad + s.a + s.b; }
long long      straddle(long long a, long long b, long long c, long long d,
                        long long e, long long f, long long g, struct P16 s){
  return a+b+c+d+e+f+g + s.a + s.b;
}
signed char    sc_make(int x){ return (signed char)x; }
unsigned char  uc_make(int x){ return (unsigned char)x; }
short          sh_make(int x){ return (short)x; }
unsigned short ush_make(int x){ return (unsigned short)x; }
long long      packed_sum(struct Packed p){ return (long long)p.a + p.b + p.c; }
struct Packed  packed_make(char a, long long b, int c){ struct Packed r; r.a=a; r.b=b; r.c=c; return r; }
long long      packs_sum(struct PackS p){ return (long long)p.a + p.b; }
struct PackS   packs_make(char a, int b){ struct PackS r; r.a=a; r.b=b; return r; }
long long      packn_sum(struct PackN p){ return (long long)p.x + p.s.a + p.s.b; }
float _Complex  cx_cf_id(float _Complex z){ return z; }
double _Complex cx_cd_id(double _Complex z){ return z; }
double _Complex cx_cd_after7(double a,double b,double c,double d,double e,double f,double g, double _Complex z){ return z + (a+b+c+d+e+f+g); }
float _Complex  cx_cf_after7(float a,float b,float c,float d,float e,float f,float g, float _Complex z){ return z + (a+b+c+d+e+f+g); }
double hfx_nsrn(double a,double b,double c,double d,double e,double f,double g, struct DblPair s, double x){ return a+b+c+d+e+f+g+s.x+s.y+x; }
double hfx_full(double a,double b,double c,double d,double e,double f,double g,double h, struct Float4 s, double x){ return a+b+c+d+e+f+g+h+s.a+s.b+s.c+s.d+x; }
EOF

cat > /w/main.c <<EOF
#include "shared.h"
int wk_ovr = 999;
int main(void){
  int k=0;
  { struct Small p; p.a=111; p.b=-40; k++; if(small_sum(p)!=71) return k; }
  { struct Small r=small_make(7,9); k++; if(r.a!=7||r.b!=9) return k; }
  { struct Odd o; o.c=(char)5; o.i=100000; o.s=(short)-30000; k++; if(odd_sum(o)!=(long long)5+100000-30000) return k; }
  { struct Big b; b.a=1; b.b=2; b.c=3; b.d=4; k++; if(big_sum(b)!=10) return k; }
  { struct Big b,r; b.a=1; b.b=2; b.c=3; b.d=4; r=big_scale(b,10); k++; if(r.a!=10||r.b!=20||r.c!=30||r.d!=40) return k; }
  { struct Mixed m; m.i=3; m.d=0.5; k++; if(mixed_sum(m)!=3.5) return k; }
  { struct Mixed r=mixed_make(7, 1.25); k++; if(r.i!=7||r.d!=1.25) return k; }
  { struct Mixed2 m; m.d=2.5; m.i=4; k++; if(mixed2_sum(m)!=6.5) return k; }
  { struct Mixed2 r=mixed2_make(3.5, 9); k++; if(r.d!=3.5||r.i!=9) return k; }
  { struct M3 m; m.f=1.5f; m.l=100; k++; if(m3_sum(m)!=101) return k; }
  { struct Mixed m; m.i=1; m.d=2.0; struct Mixed r=mixed_after(1,2,3,4,5,m);
    k++; if(r.i!=16||r.d!=2.0) return k; }
  { struct FA x; x.f[0]=1.5f; x.f[1]=2.5f; x.i=10; k++; if(fa_sum(x)!=14.0) return k; }
  { struct F4A x; x.f[0]=1.5f;x.f[1]=2.5f;x.f[2]=3.0f;x.f[3]=4.0f; k++; if(f4a_sum(x)!=11.0f) return k; }
  { struct F4A r=f4a_mk(1,2,3,4); k++; if(r.f[0]!=1||r.f[3]!=4) return k; }
  { struct D2A x; x.d[0]=2.5;x.d[1]=8.0; k++; if(d2a_sum(x)!=10.5) return k; }
  { struct D2A r=d2a_mk(3.5,4.5); k++; if(r.d[0]!=3.5||r.d[1]!=4.5) return k; }
  { struct NHFA x; x.p.a=1.5f;x.p.b=2.5f;x.c=3.0f; k++; if(nhfa_sum(x)!=7.0f) return k; }
  { struct NHFA r=nhfa_mk(2,3,4); k++; if(r.p.a!=2||r.p.b!=3||r.c!=4) return k; }
  { struct DNest x; x.a.x=2.5;x.b=8.0; k++; if(dnest_sum(x)!=10.5) return k; }
  { struct SArr x; x.v[0].a=1;x.v[0].b=2;x.v[1].a=3;x.v[1].b=4; k++; if(sarr_sum(x)!=10.0f) return k; }
  { struct BoolM x; x.ok=1;x.d=4.5; k++; if(boolm_sum(x)!=5.5) return k; }
  { struct S3 x; x.a=1;x.b=2;x.c=3; k++; if(s3_sum(x)!=6) return k; }
  { struct S7 x; x.a=1;x.b=2;x.c=3;x.d=4;x.e=5;x.f=6;x.g=7; k++; if(s7_sum(x)!=28) return k; }
  { struct SF1 x; x.f=3.5f; k++; if(sf1_sum(x)!=3.5f) return k; }
  { struct SF1 r=sf1_mk(1.25f); k++; if(r.f!=1.25f) return k; }
  { struct LD1 x; x.ld=2.5L; k++; if(ld1_sum(x)!=2.5L) return k; }
  { struct FB x; x.f=1.5f; x.b=100; k++; if(fb_sum(x)!=101.5) return k; }
  { struct DB x; x.d=2.5; x.b=100; k++; if(db_sum(x)!=102.5) return k; }
  { struct FCS x; x.z=__builtin_complex(3.5f,-1.0f); k++; if(fcs_re(x)!=3.5f) return k; }
  { struct DFI x; x.d=1.5; x.f=2.5f; x.i=6; k++; if(dfi_sum(x)!=10.0) return k; }
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
  { struct HFA4 r=hfa4_make(10.0,20.0,30.0,40.0); k++; if(r.a!=10.0||r.b!=20.0||r.c!=30.0||r.d!=40.0) return k; }
  { long double r=ld_add(1.5L, 2.25L); k++; if(r!=3.75L) return k; }
  { long double r=ld_mix(3, 2.5L, 1.5); k++; if(r!=9.0L) return k; }
  { struct Wide r=wide_make(1.5, 100L, 2.5, 200L); k++; if(r.a!=1.5||r.b!=100||r.c!=2.5||r.d!=200) return k; }
  { k++; if(vsum_d(4, 1.5, 2.25, 4.0, 8.0)!=15.75) return k; }
  { k++; if(vmix_id(3, 2, 1.5, 5, 2.0, 10, 0.5)!=(double)(2*1.5+5*2.0+10*0.5)) return k; }
  { struct Ten r=ten_make(1,2,3,4,5,6,7,8,9,10); k++;
    if(r.a!=1||r.b!=2||r.c!=3||r.d!=4||r.e!=5||r.f!=6||r.g!=7||r.h!=8||r.i!=9||r.j!=10) return k; }
  { struct A16 s; s.a=10; s.b=20; k++; if(a16_after_int(1, s)!=31) return k; }
  { struct P16 s; s.a=100; s.b=200; k++; if(straddle(1,2,3,4,5,6,7,s)!=328) return k; }
  { struct BF x; x.a=5; x.b=-10; x.c=1000000; x.d=-500000000000LL; x.e=-100;
    k++; if(x.a!=5u||x.b!=-10||x.c!=1000000u||x.d!=-500000000000LL||x.e!=-100) return k; }
  { struct BF x; x.a=7; x.b=15; x.c=1048575; x.d=549755813887LL; x.e=2047;
    k++; if(x.a!=7u||x.b!=15||x.c!=1048575u||x.d!=549755813887LL||x.e!=2047) return k; }
  { k++; if(sc_make(-1)!=-1) return k; }
  { k++; if(sc_make(200)!=-56) return k; }
  { k++; if(uc_make(300)!=44) return k; }
  { k++; if(sh_make(70000)!=4464) return k; }
  { k++; if(ush_make(-1)!=65535) return k; }
  { long long r=sc_make(200); k++; if(r!=-56) return k; }
  { unsigned long long r=uc_make(-1); k++; if(r!=255) return k; }
  { struct Packed p; p.a=5; p.b=1000000000000LL; p.c=-7; k++; if(packed_sum(p)!=(long long)5+1000000000000LL-7) return k; }
  { struct Packed r=packed_make(9, 123456789012LL, 42); k++; if(r.a!=9||r.b!=123456789012LL||r.c!=42) return k; }
  { struct PackS p; p.a=3; p.b=1000000; k++; if(packs_sum(p)!=1000003) return k; }
  { struct PackS r=packs_make(7,-9); k++; if(r.a!=7||r.b!=-9) return k; }
  { struct PackN p; p.x=1; p.s.a=2; p.s.b=30000; k++; if(packn_sum(p)!=30003) return k; }
  { k++; if(g_data.a!=7||g_data.b!=88||g_data.c!=2.5||g_data.d[0]!=100||g_data.d[2]!=300||g_data.e!=-99) return k; }
  { k++; if(g_tls!=555) return k; }
  { tls_set(999); k++; if(g_tls!=999) return k; }
  { g_tls = 314; k++; if(tls_get()!=314) return k; }
  { k++; if(g_tls_z!=0) return k; }
  { g_tls_z = 777; k++; if(get_tlsz()!=777) return k; }
  { k++; if(get_wk()!=999) return k; }
  { k++; if(get_alias()!=42) return k; }
  { float _Complex z=__builtin_complex(1.5f,-2.5f); float _Complex r=cx_cf_id(z);
    k++; if(__real__ r!=1.5f || __imag__ r!=-2.5f) return k; }
  { double _Complex z=__builtin_complex(3.5,4.5); double _Complex r=cx_cd_id(z);
    k++; if(__real__ r!=3.5 || __imag__ r!=4.5) return k; }
  { double _Complex z=__builtin_complex(0.5,1.5);
    double _Complex r=cx_cd_after7(1,2,3,4,5,6,7,z);
    k++; if(__real__ r!=28.5 || __imag__ r!=1.5) return k; }
  { float _Complex z=__builtin_complex(0.25f,-0.5f);
    float _Complex r=cx_cf_after7(1,2,3,4,5,6,7,z);
    k++; if(__real__ r!=28.25f || __imag__ r!=-0.5f) return k; }
  { struct DblPair s; s.x=100; s.y=200; k++; if(hfx_nsrn(1,2,3,4,5,6,7,s,1000)!=1328.0) return k; }
  { struct Float4 s; s.a=10; s.b=20; s.c=40; s.d=80; k++; if(hfx_full(1,2,3,4,5,6,7,8,s,1000)!=1186.0) return k; }
  return 0;
}
EOF

RD=/b/runtime/include
echo "== compile each TU with mcc and $GCC =="
/w/mcc  -O1 -I $RD -I /w -c /w/lib.c  -o /w/lib_mcc.o
/w/mcc  -O1 $MAINDEF -I $RD -I /w -c /w/main.c -o /w/main_mcc.o
"$GCC"  -O2       -I /w -c /w/lib.c  -o /w/lib_gcc.o
"$GCC"  -O2 $MAINDEF -I /w -c /w/main.c -o /w/main_gcc.o

link_run() {
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
