#!/bin/sh

set -eu
. "$(dirname "$0")/dockergate.sh"

MCC="${1:-}"
WORK="${2:-./w-i386tls}"
IMAGE="${MCC_I386_DOCKER_IMAGE:-i386/debian:bullseye-slim}"

dg_need_bin "$MCC" "i386 mcc"
dg_need_docker
dg_need_platform linux/386 "$IMAGE"

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
dg_docker run --rm --platform linux/386 -v "$WORK_ABS":/w -w /w "$IMAGE" sh -c '
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
