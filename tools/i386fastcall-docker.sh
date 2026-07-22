#!/bin/sh
# Validate the i386 __attribute__((fastcall)) ABI of an mcc i386 cross-compiler
# against a native i386 gcc, using a linux/386 Docker container for the parts
# that must run as i386 (the reference gcc build, the cross-link, and execution).
#
# The host arch is irrelevant: mcc runs on the host and emits i386 ELF objects;
# everything i386-native happens inside the container. This is the execution
# path that lets non-i386 hosts (e.g. arm64 macOS, where qemu-i386 user-mode and
# 32-bit wine are unavailable) exercise the fastcall ABI that CMake's
# i386-fastcall-abi test otherwise skips.
#
# Usage:  tools/i386fastcall-docker.sh <mcc-i386> [workdir]
# Exit:   0 all checks pass · 1 a check failed · 77 skipped (no docker/mcc-i386)

set -eu

MCC="${1:-}"
WORK="${2:-./w-i386fastcall}"
IMAGE="${MCC_I386_DOCKER_IMAGE:-i386/debian:bullseye-slim}"

if [ -z "$MCC" ] || [ ! -x "$MCC" ]; then
	echo "SKIP: i386 mcc not found at '${MCC:-<unset>}'"
	exit 77
fi
if ! command -v docker >/dev/null 2>&1; then
	echo "SKIP: docker not available"
	exit 77
fi
if ! docker run --rm --platform linux/386 "$IMAGE" true >/dev/null 2>&1; then
	echo "SKIP: cannot run linux/386 containers ($IMAGE)"
	exit 77
fi

rm -rf "$WORK"
mkdir -p "$WORK"
WORK_ABS=$(cd "$WORK" && pwd)

cat > "$WORK_ABS/callee.c" <<'EOF'
struct P2 { int x, y; };
int __attribute__((fastcall)) mix_ll(int a, long long b, int c){ return (int)(a+b+c); }
int __attribute__((fastcall)) small(char a, short b, int c){ return a+b+c; }
int __attribute__((fastcall)) ptr2(int *a, int *b){ return *a + *b; }
int __attribute__((fastcall)) ll_first(long long a, int b){ return (int)(a+b); }
int __attribute__((fastcall)) fs(int a, struct P2 p, int b){ return a*1000+p.x*100+p.y*10+b; }
EOF

cat > "$WORK_ABS/caller.c" <<'EOF'
struct P2 { int x, y; };
int __attribute__((fastcall)) mix_ll(int a, long long b, int c);
int __attribute__((fastcall)) small(char a, short b, int c);
int __attribute__((fastcall)) ptr2(int *a, int *b);
int __attribute__((fastcall)) ll_first(long long a, int b);
int __attribute__((fastcall)) fs(int a, struct P2 p, int b);
int main(void){
	int x=10, y=20, f=0; struct P2 p={2,3};
	if (mix_ll(1,100,3)!=104) f|=1;
	if (small(1,2,3)!=6) f|=2;
	if (ptr2(&x,&y)!=30) f|=4;
	if (ll_first(100,5)!=105) f|=8;
	if (fs(1,p,4)!=1234) f|=16;
	return f;
}
EOF

# float-in-a-register-position fastcall is unsupported; mcc must reject it.
cat > "$WORK_ABS/unsup.c" <<'EOF'
int __attribute__((fastcall)) f(double a,int b); int g(){ return f(1.0,2); }
EOF

echo "== host: mcc-i386 -> i386 ELF objects =="
"$MCC" -c "$WORK_ABS/callee.c" -o "$WORK_ABS/callee_mcc.o"
"$MCC" -c "$WORK_ABS/caller.c" -o "$WORK_ABS/caller_mcc.o"

echo "== host: mcc-i386 must reject float-before-reg fastcall =="
if "$MCC" -c "$WORK_ABS/unsup.c" -o "$WORK_ABS/unsup.o" >/dev/null 2>&1; then
	echo "FAIL  unsupported float-before-reg fastcall should error"
	exit 1
fi
echo "PASS  unsupported float-before-reg fastcall rejected"

echo "== docker linux/386: gcc reference build + cross-link + run =="
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm --platform linux/386 -v "$WORK_ABS":/w -w /w "$IMAGE" sh -c '
	set -e
	command -v gcc >/dev/null || { apt-get update >/dev/null 2>&1; apt-get install -y gcc >/dev/null 2>&1; }
	gcc -m32 -O0 -c callee.c -o callee_gcc.o
	gcc -m32 -O0 -c caller.c -o caller_gcc.o
	fail=0
	for combo in \
		"caller_gcc.o callee_mcc.o:gcc-caller -> mcc-callee" \
		"caller_mcc.o callee_gcc.o:mcc-caller -> gcc-callee" \
		"caller_mcc.o callee_mcc.o:mcc-caller -> mcc-callee"; do
		objs=${combo%%:*}; name=${combo##*:}
		if gcc -m32 $objs -o prog 2>/dev/null && ./prog; then
			echo "PASS  $name"
		else
			echo "FAIL  $name (exit $?)"; fail=1
		fi
	done
	exit $fail
'
