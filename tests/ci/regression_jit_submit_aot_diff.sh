#!/bin/sh
# Differential guard for the backend-override codegen path (mcc_jit_submit_ast /
# MCC_JIT_SUBMIT_AOT): the runtime JIT recompiles hot functions FROM the backend-
# submitted AST instead of the shipped intent. This path is a fresh codegen route,
# so it must match the JIT-off reference on real programs (the historical default-on
# failure mode was JIT variants miscompiling real code). Each program is run three
# ways and all three outputs must agree; the override path must actually fire.
# Args: $1 = mcc binary.
set -e
MCC="$1"
[ -x "$MCC" ] || { echo "regression_jit_submit_aot_diff: mcc not executable: $MCC"; exit 1; }
TMP="${TMPDIR:-/tmp}/regr_submit_$$"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT
fail=0

cat > "$TMP/p1.c" <<'EOF'
#include <stdio.h>
__attribute__((noinline)) static long fib(int n){ return n<2?n:fib(n-1)+fib(n-2); }
__attribute__((noinline)) static unsigned coll(unsigned n){unsigned s=0;while(n>1){n=n&1?3*n+1:n/2;s++;}return s;}
int main(void){ long a=0; unsigned b=0; int i; for(i=0;i<32;i++){a+=fib(i%20);b+=coll((unsigned)(i+1));} printf("%ld %u\n",a,b); return 0; }
EOF
cat > "$TMP/p2.c" <<'EOF'
#include <stdio.h>
__attribute__((noinline)) static int gcd(int a,int b){while(b){int t=a%b;a=b;b=t;}return a;}
__attribute__((noinline)) static long poly(long x){return ((x*x+3*x)/7)%1000 - (x*13)%17;}
int main(void){long s=0;int i;for(i=1;i<50000;i++){s+=gcd(i,i*3+7);s+=poly(i);}printf("%ld\n",s);return 0;}
EOF
cat > "$TMP/p3.c" <<'EOF'
#include <stdio.h>
__attribute__((noinline)) static double mix(double x){return x*1.5 - (x>0? x/3.0 : -x);}
__attribute__((noinline)) static unsigned bits(unsigned x){unsigned c=0;while(x){c+=x&1;x>>=1;}return c;}
int main(void){double d=0;unsigned t=0;int i;for(i=0;i<40000;i++){d+=mix((double)(i%97));t+=bits((unsigned)i*2654435761u);}printf("%.3f %u\n",d,t);return 0;}
EOF

san() { sed 's/\x1b\[[0-9;]*[A-Za-z]//g' | tr -d '\r' | sed 's/[[:space:]]*$//'; }

# The MCC_JIT_SUBMIT_AOT / override path is the runtime mode-6 JIT dispatch. On
# arm64 that path is mutually exclusive with the -O4 superoptimizer search (the
# search's MEMORY re-emit corrupts the arm64 dispatch slot's symbol), so the arm64
# mode-6 entry is gated on !ast_search_env (mccast.c, commit 77bd8ba8). Since the
# -O search became default-ON at -O4 (commit 672b4ffb), pin MCC_AST_SEARCH=0 on the
# -O4 JIT runs below so the override actually fires on arm64. The -O2 MCC_JIT=0
# reference never runs the search regardless.
for p in p1 p2 p3; do
	ref=$(env MCC_JIT=0 "$MCC" -O2 -run "$TMP/$p.c" 2>/dev/null | san)
	nos=$(env XDG_CACHE_HOME="$TMP/n-$p" MCC_AST_SEARCH=0 MCC_JIT=1 MCC_JIT_HOT_THRESHOLD=50 \
		"$MCC" -O4 -run "$TMP/$p.c" 2>/dev/null | san)
	sub=$(env XDG_CACHE_HOME="$TMP/s-$p" MCC_AST_SEARCH=0 MCC_JIT=1 MCC_JIT_SUBMIT_AOT=1 MCC_JIT_HOT_THRESHOLD=50 \
		"$MCC" -O4 -run "$TMP/$p.c" 2>/dev/null | san)
	ov=$(env XDG_CACHE_HOME="$TMP/v-$p" MCC_AST_SEARCH=0 MCC_JIT=1 MCC_JIT_SUBMIT_AOT=1 MCC_JIT_HOT_THRESHOLD=50 MCC_JIT_VERBOSE=1 \
		"$MCC" -O4 -run "$TMP/$p.c" 2>&1 >/dev/null | grep -cE 'mccjit-override' || true)
	m="OK"
	[ "$ref" = "$nos" ] || { m="FAIL(jit!=ref)"; fail=1; }
	[ "$ref" = "$sub" ] || { m="FAIL(submit!=ref)"; fail=1; }
	[ "${ov:-0}" -ge 1 ] || { m="FAIL(override-not-fired)"; fail=1; }
	echo "$p: ref=[$ref] jit=[$nos] submit=[$sub] overrides=$ov $m"
done

[ "$fail" = 0 ] && echo "regression_jit_submit_aot_diff: ALL PASS" || echo "regression_jit_submit_aot_diff: FAIL"
exit $fail
