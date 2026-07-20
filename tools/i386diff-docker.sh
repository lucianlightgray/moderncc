#!/bin/sh
# Broad i386 codegen differential: compile a battery of freestanding C programs
# with an mcc i386 cross-compiler AND with a native i386 gcc, run both inside a
# linux/386 Docker container, and require identical exit codes. Each program
# folds its computation into a checksum returned as the process exit code, so no
# libc/headers are needed at compile time (mcc-i386 has no libc headers on the
# host) and the comparison is a pure mcc-vs-gcc differential.
#
# mcc objects are linked with mcc's i386 runtime (i386-libmccrt.a) so builtin
# helper calls (e.g. __builtin_popcount) resolve; gcc builds from source.
#
# Usage:  tools/i386diff-docker.sh <mcc-i386> <i386-libmccrt.a> [workdir]
# Exit:   0 all match · 1 a divergence · 77 skipped (no docker/mcc-i386/runtime)

set -eu

MCC="${1:-}"
RT="${2:-}"
WORK="${3:-./w-i386diff}"
IMAGE="${MCC_I386_DOCKER_IMAGE:-i386/debian:bullseye-slim}"

if [ -z "$MCC" ] || [ ! -x "$MCC" ]; then echo "SKIP: i386 mcc not found at '${MCC:-<unset>}'"; exit 77; fi
if [ -z "$RT" ] || [ ! -f "$RT" ]; then echo "SKIP: i386 runtime not found at '${RT:-<unset>}'"; exit 77; fi
if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! docker run --rm --platform linux/386 "$IMAGE" true >/dev/null 2>&1; then
	echo "SKIP: cannot run linux/386 containers ($IMAGE)"; exit 77
fi

rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS=$(cd "$WORK" && pwd)
cp "$RT" "$WORK_ABS/librt.a"

cat > "$WORK_ABS/d1.c" <<'EOF'
int main(void){ long long a=0x123456789ABCDEFLL,b=-3LL,s=0; unsigned long long u=~0ULL;
  s+=a*b; s+=a>>5; s+=(long long)(u>>60); s+=a%7; s^=a<<11; s+=(unsigned)a/13;
  return (int)((s ^ (s>>32)) % 251); }
EOF
cat > "$WORK_ABS/d2.c" <<'EOF'
struct S{int a; long long b; char c; short d;};
struct S mk(int x){ struct S s; s.a=x*7; s.b=(long long)x*100003; s.c=(char)(x*3); s.d=(short)(x*257); return s; }
int main(void){ int s=0,i; for(i=-5;i<20;i++){ struct S v=mk(i); s+=v.a+(int)v.b+v.c+v.d; } return (s&0x7fffffff)%251; }
EOF
cat > "$WORK_ABS/d3.c" <<'EOF'
int main(void){ unsigned s=0; int i; double d=1.0; float f;
  for(i=1;i<=20;i++){ f=(float)i/3.0f; d=d*1.0000001+f; s+=(unsigned)(d*1000.0)%9973; }
  s+=(unsigned)(-1.5f); s+=(unsigned)(2147483647.0/3.0);
  return (int)(s%251); }
EOF
cat > "$WORK_ABS/d4.c" <<'EOF'
int main(void){ int s=0,i; for(i=0;i<64;i++){ unsigned m=(1u<<(i%32)); int v=(int)m; s+=(v<0)?-v:v; s^=(m>>1); s+=__builtin_popcount(m); } return (s&0x7fffffff)%251; }
EOF
cat > "$WORK_ABS/d5.c" <<'EOF'
int fib(int n){ return n<2?n:fib(n-1)+fib(n-2); }
int ack(int m,int n){ return m==0?n+1: n==0?ack(m-1,1): ack(m-1,ack(m,n-1)); }
int main(void){ return (fib(20)+ack(2,3)*100)%251; }
EOF
cat > "$WORK_ABS/d6.c" <<'EOF'
int main(void){ signed char sc=-1; unsigned char uc=200; short sh=-30000; unsigned short uh=60000;
  int s=0; s+=sc; s+=uc; s+=sh; s+=uh; s+=(int)sc*(int)uc; s+=(unsigned)sh; s+=(long long)sh*uh%97;
  s+=(sc<<3); s+=(uh>>2); return (s&0x7fffffff)%251; }
EOF
cat > "$WORK_ABS/d7.c" <<'EOF'
int main(void){ long double x=1.0L, s=0; int i; for(i=1;i<=25;i++){ x=x*1.1L+0.5L; s+=x/(long double)i; }
  long long r=(long long)(s*1000.0L); return (int)(r%251<0?-r%251:r%251); }
EOF
cat > "$WORK_ABS/d8.c" <<'EOF'
#include <stdarg.h>
int isum(int n, ...){ va_list ap; va_start(ap,n); long long s=0; int i; for(i=0;i<n;i++) s+=va_arg(ap,int); va_end(ap); return (int)(s%251); }
double dsum(int n, ...){ va_list ap; va_start(ap,n); double s=0; int i; for(i=0;i<n;i++) s+=va_arg(ap,double); va_end(ap); return s; }
int main(void){ int a=isum(6,10,20,30,40,50,60); int b=(int)(dsum(3,1.5,2.5,3.0)*10); return (a+b)%251; }
EOF
cat > "$WORK_ABS/d9.c" <<'EOF'
struct Big{ long long a[8]; };
struct Big mk(int x){ struct Big b; int i; for(i=0;i<8;i++) b.a[i]=(long long)x*i*100003; return b; }
int use(struct Big b){ long long s=0; int i; for(i=0;i<8;i++) s+=b.a[i]; return (int)(s%251); }
int main(void){ int s=0,i; for(i=1;i<=10;i++){ struct Big b=mk(i); s+=use(b); } return (s&0x7fffffff)%251; }
EOF
cat > "$WORK_ABS/d10.c" <<'EOF'
int main(void){ _Complex double z=1.0+2.0*1.0i; int i; for(i=0;i<10;i++) z=z*z/(0.9+0.1*1.0i);
  double re=__real__ z, im=__imag__ z; long long r=(long long)((re+im)*1000.0); return (int)(r%251<0?-(r%251):r%251); }
EOF

PROGS="d1 d2 d3 d4 d5 d6 d7 d8 d9 d10"
echo "== host: mcc-i386 -> i386 ELF objects at -O0 and -O2 =="
for t in $PROGS; do
	"$MCC" -O0 -c "$WORK_ABS/$t.c" -o "$WORK_ABS/${t}_m0.o"
	"$MCC" -O2 -c "$WORK_ABS/$t.c" -o "$WORK_ABS/${t}_m2.o"
done

echo "== docker linux/386: run mcc (-O0 and -O2, mcc.o + runtime) vs gcc, diff exit codes =="
docker run --rm --platform linux/386 -v "$WORK_ABS":/w -w /w "$IMAGE" sh -c '
	command -v gcc >/dev/null || { apt-get update >/dev/null 2>&1; apt-get install -y gcc >/dev/null 2>&1; }
	fail=0
	for t in '"$PROGS"'; do
		if ! gcc -m32 -O2 ${t}.c -o ${t}_ge 2>/dev/null; then echo "FAIL  $t (gcc build)"; fail=1; continue; fi
		grc=0; ./${t}_ge || grc=$?
		for o in 0 2; do
			if ! gcc -m32 ${t}_m${o}.o librt.a -o ${t}_me${o} 2>/dev/null; then echo "FAIL  $t -O$o (mcc-object link)"; fail=1; continue; fi
			mrc=0; ./${t}_me${o} || mrc=$?
			if [ "$mrc" = "$grc" ]; then echo "OK    $t -O$o (mcc=gcc=$mrc)"; else echo "DIFF  $t -O$o mcc=$mrc gcc=$grc"; fail=1; fi
		done
	done
	exit $fail
'
