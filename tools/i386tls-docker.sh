#!/bin/sh
# Validate an mcc i386 cross-compiler's thread-local (__thread) codegen using a
# linux/386 Docker container for the parts that must run as i386 (the link
# against 32-bit libc + TLS runtime, and execution).
#
# Two checks:
#   1. non-PIC __thread: mcc emits R_386_TLS_LE; the linked exe must run and see
#      correct per-thread values (exit 0).
#   2. -fPIC __thread: mcc emits global-dynamic (GD) for globals and local-dynamic
#      (LDM) for statics; the linked PIE must run and see correct per-thread
#      values (exit 0). A compile-time error or a runtime mismatch is a regression.
#
# mcc runs on the host and emits i386 ELF; everything i386-native happens in the
# container. Lets non-i386 hosts (arm64 macOS: no qemu-i386 user-mode, no viable
# 32-bit wine) exercise i386 TLS codegen.
#
# Usage:  tools/i386tls-docker.sh <mcc-i386> [workdir]
# Exit:   0 all checks pass · 1 a check failed · 77 skipped (no docker/mcc-i386)

set -eu

MCC="${1:-}"
WORK="${2:-./w-i386tls}"
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

cat > "$WORK_ABS/tls.c" <<'EOF'
__thread int g_tls = 111;
static __thread int l_tls = 222;
__thread long z_tls;
int get_g(void){ return g_tls; }
int get_l(void){ return l_tls; }
int main(void){
	g_tls += 1; l_tls += 2; z_tls += 5;
	return (get_g()==112 && get_l()==224 && z_tls==5) ? 0 : 1;
}
EOF

echo "== host: mcc-i386 -fPIC __thread -> i386 ELF object (GD globals, LDM statics) =="
"$MCC" -fPIC -c "$WORK_ABS/tls.c" -o "$WORK_ABS/pic.o"

echo "== host: mcc-i386 non-PIC __thread -> i386 ELF object (R_386_TLS_LE) =="
"$MCC" -c "$WORK_ABS/tls.c" -o "$WORK_ABS/def.o"

echo "== docker linux/386: link + run both objects =="
docker run --rm --platform linux/386 -v "$WORK_ABS":/w -w /w "$IMAGE" sh -c '
	set -e
	command -v gcc >/dev/null || { apt-get update >/dev/null 2>&1; apt-get install -y gcc >/dev/null 2>&1; }
	gcc -m32 def.o -o prog
	if ./prog; then
		echo "PASS  non-PIC __thread links and runs with correct per-thread values"
	else
		echo "FAIL  non-PIC __thread runtime check (exit $?)"; exit 1
	fi
	gcc -m32 -pie pic.o -o prog_pic
	if ./prog_pic; then
		echo "PASS  -fPIC __thread (GD/LDM) links and runs with correct per-thread values"
	else
		echo "FAIL  -fPIC __thread runtime check (exit $?)"; exit 1
	fi
'
