#!/bin/sh
set -e

ARCH=$1
MCC=$2
CROSS=$3
SR=$4
W=$5
RTSRC=$6
[ -n "$ARCH" ] && [ -n "$MCC" ] && [ -n "$W" ] && [ -n "$RTSRC" ] || {
	echo "usage: asan-accesstype.sh <arch> <mcc> <crossdir> <sysroot> <workdir> <runtime-src>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no $ARCH mcc at $MCC"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR"; exit 77; }
[ -f "$RTSRC" ] || { echo "SKIP: no asan runtime source at $RTSRC"; exit 77; }

ABI=""
case "$ARCH" in
riscv64) EMU=qemu-riscv64 ;;
arm64)   EMU=qemu-aarch64 ;;
i386)    EMU=qemu-i386 ;;
arm)     EMU=qemu-arm; ABI="-mfloat-abi hard" ;;
*) echo "unsupported arch '$ARCH'" >&2; exit 2 ;;
esac
QEMU=$(command -v "$EMU" || command -v "$EMU-static" || true)
[ -n "$QEMU" ] || { echo "SKIP: no $EMU"; exit 77; }

rm -rf "$W"; mkdir -p "$W"
CC="$MCC $ABI -B $CROSS --sysroot=$SR -I$SR/usr/include"
CC="$CC -L$SR/usr/lib64 -L$SR/lib64 -L$SR/usr/lib -L$SR/lib"

$CC -c "$RTSRC" -o "$W/asan.o" >"$W/rt.log" 2>&1 || {
	echo "SKIP: $ARCH mcc could not build the asan runtime"
	sed 's/^/  /' "$W/rt.log" | head -5
	exit 77
}

build_run() {
	$CC -fasan-shadow -c "$W/$1.c" -o "$W/$1.o" >"$W/$1.log" 2>&1 || return 1
	$CC "$W/$1.o" "$W/asan.o" -o "$W/$1.bin" >>"$W/$1.log" 2>&1 || return 1
	"$QEMU" -L "$SR" "$W/$1.bin" >"$W/$1.out" 2>&1
	return 0
}

fails=0
expect() {
	name=$1
	want=$2
	if ! build_run "$name"; then
		echo "FAIL: could not build/link $name"
		sed 's/^/  /' "$W/$name.log" | head -5
		fails=$((fails + 1))
		return
	fi
	got=$(grep -oE '(READ|WRITE) of size [0-9]+|access size [0-9]+' "$W/$name.out" | head -1)
	if [ "$got" != "$want" ]; then
		echo "FAIL: $name expected '$want', got '$got'"
		sed 's/^/  /' "$W/$name.out" | head -8
		fails=$((fails + 1))
	fi
}

cat >"$W/wr.c" <<'EOF'
extern void *malloc(unsigned long);
int main(void) { char *p = malloc(8); p[12] = 1; return 0; }
EOF
cat >"$W/rd.c" <<'EOF'
extern void *malloc(unsigned long);
int main(void) { char *p = malloc(8); char *q = malloc(64); q[0] = p[12]; return 0; }
EOF
cat >"$W/w4.c" <<'EOF'
extern void *malloc(unsigned long);
int main(void) { char *p = malloc(8); *(int *)(p + 12) = 5; return 0; }
EOF
cat >"$W/uaf.c" <<'EOF'
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
int main(void) { char *p = malloc(8); free(p); printf("%d\n", p[0]); return 0; }
EOF
cat >"$W/sm.c" <<'EOF'
extern void *malloc(unsigned long);
struct S { int a, b, c, d, e; };
int main(void) { struct S *s = malloc(8); s->e = 1; return 0; }
EOF
cat >"$W/ok.c" <<'EOF'
extern void *malloc(unsigned long);
struct S { int a, b, c, d, e; };
int main(void) {
	char *p = malloc(64);
	struct S *s = malloc(sizeof(struct S));
	int i;
	for (i = 0; i < 64; i++)
		p[i] = (char)i;
	s->a = 1;
	s->e = 5;
	return (p[63] == 63 && s->a + s->e == 6) ? 0 : 7;
}
EOF

expect wr "WRITE of size 1"
expect rd "READ of size 1"
expect w4 "WRITE of size 4"
expect uaf "READ of size 1"
expect sm "WRITE of size 4"

if ! build_run ok; then
	echo "FAIL: could not build/link the clean program"
	fails=$((fails + 1))
elif ! "$QEMU" -L "$SR" "$W/ok.bin" >"$W/ok.out" 2>&1; then
	echo "FAIL: a program with no invalid access trapped under -fasan-shadow"
	sed 's/^/  /' "$W/ok.out" | head -8
	fails=$((fails + 1))
fi

[ "$fails" -eq 0 ] || exit 1
echo "PASS: $ARCH -fasan-shadow labels READ/WRITE and leaves valid accesses alone"
