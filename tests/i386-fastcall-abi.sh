#!/bin/bash
# i386 __fastcall ABI cross-check: verify that the i386 tcc backend's fastcall
# register assignment matches gcc -m32, in both call directions and tcc<->tcc.
#
# This cannot run in the normal ctest suite (which drives the x86_64 tcc, where
# __fastcall is a no-op), so it is a standalone manual check. It needs an i386
# tcc and a working `gcc -m32` (32-bit multilib).
#
# Usage:  tests/i386-fastcall-abi.sh /path/to/i386-tcc
#   e.g.  cmake -S . -B build-i386 -DTCC_ENABLE_CROSS=ON
#         cmake --build build-i386 --target i386-tcc
#         tests/i386-fastcall-abi.sh build-i386/i386-tcc
set -u
ITCC="${1:-build-i386/i386-tcc}"
command -v gcc >/dev/null || { echo "SKIP: no gcc"; exit 0; }
echo 'int main(){return 0;}' | gcc -m32 -x c - -o /dev/null 2>/dev/null \
    || { echo "SKIP: gcc -m32 unavailable (no 32-bit multilib)"; exit 0; }
[ -x "$ITCC" ] || { echo "SKIP: i386 tcc not found at $ITCC"; exit 0; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/callee.c" <<'EOF'
/* fastcall register-assignment edge cases: long long blocks the register
   budget; char/short are promoted into a register; pointers use registers;
   a long long as the first arg goes entirely on the stack; an 8-byte struct
   reserves both register slots (blocks), and an int after it spills. */
struct P2 { int x, y; };
int __attribute__((fastcall)) mix_ll(int a, long long b, int c){ return (int)(a+b+c); }
int __attribute__((fastcall)) small(char a, short b, int c){ return a+b+c; }
int __attribute__((fastcall)) ptr2(int *a, int *b){ return *a + *b; }
int __attribute__((fastcall)) ll_first(long long a, int b){ return (int)(a+b); }
int __attribute__((fastcall)) fs(int a, struct P2 p, int b){ return a*1000+p.x*100+p.y*10+b; }
EOF
cat > "$tmp/caller.c" <<'EOF'
struct P2 { int x, y; };
int __attribute__((fastcall)) mix_ll(int a, long long b, int c);
int __attribute__((fastcall)) small(char a, short b, int c);
int __attribute__((fastcall)) ptr2(int *a, int *b);
int __attribute__((fastcall)) ll_first(long long a, int b);
int __attribute__((fastcall)) fs(int a, struct P2 p, int b);
int main(void){
    int x=10, y=20, f=0;
    struct P2 p={2,3};
    if (mix_ll(1,100,3)   != 104)  f|=1;
    if (small(1,2,3)      != 6)    f|=2;
    if (ptr2(&x,&y)       != 30)   f|=4;
    if (ll_first(100,5)   != 105)  f|=8;
    if (fs(1,p,4)         != 1234) f|=16; /* int->ecx, struct+int->stack */
    return f;   /* 0 = ABI matches gcc */
}
EOF

fail=0
"$ITCC" -c "$tmp/callee.c" -o "$tmp/e_t.o" || fail=1
"$ITCC" -c "$tmp/caller.c" -o "$tmp/l_t.o" || fail=1
gcc -m32 -O0 -c "$tmp/callee.c" -o "$tmp/e_g.o"
gcc -m32 -O0 -c "$tmp/caller.c" -o "$tmp/l_g.o"

check(){ # name objs...
    local name="$1"; shift
    if gcc -m32 "$@" -o "$tmp/run" 2>/dev/null && "$tmp/run"; then
        echo "PASS  $name"
    else
        echo "FAIL  $name (exit $?)"; fail=1
    fi
}
check "tcc caller -> gcc callee" "$tmp/e_g.o" "$tmp/l_t.o"
check "gcc caller -> tcc callee" "$tmp/e_t.o" "$tmp/l_g.o"
check "tcc caller -> tcc callee" "$tmp/e_t.o" "$tmp/l_t.o"

# the one unsupported corner must be REJECTED, not silently miscompiled
echo 'int __attribute__((fastcall)) f(double a,int b); int g(){ return f(1.0,2); }' > "$tmp/unsup.c"
if "$ITCC" -c "$tmp/unsup.c" -o /dev/null 2>/dev/null; then
    echo "FAIL  unsupported float-before-reg case should error"; fail=1
else
    echo "PASS  unsupported float-before-reg case errors cleanly"
fi

[ "$fail" = 0 ] && echo "ALL OK" || echo "FAILURES"
exit "$fail"
