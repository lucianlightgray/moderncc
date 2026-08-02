#!/bin/sh
# riscv64 -fasan-shadow must label the faulting access READ or WRITE.
#
# The check is emitted once, at indir(), where the parser does not yet know
# whether the lvalue will be loaded or stored, so gen_asan_shadow_check sets
# bit 6 of the granule-offset register on the trap path and expr_eq patches the
# immediate to also set bit 7 when it sees an assignment token.
#
# The clean case is not optional here. Inserting the ori shifted the ebreak by
# one instruction, and the first version of this port left the "shadow byte is
# zero, skip" branch pointing at the ebreak instead of past it -- every VALID
# access trapped. Both fault cases still printed the right label, so only a
# program that must NOT fault catches it.
#
# Usage: asan-accesstype-riscv64.sh <mcc> <crossdir> <sysroot> <workdir> <runtime-src>
set -e

MCC=$1
CROSS=$2
SR=$3
W=$4
RTSRC=$5
[ -n "$MCC" ] && [ -n "$W" ] && [ -n "$RTSRC" ] || {
	echo "usage: asan-accesstype-riscv64.sh <mcc> <crossdir> <sysroot> <workdir> <runtime-src>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no riscv64 mcc at $MCC"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no riscv64 sysroot at $SR"; exit 77; }
[ -f "$RTSRC" ] || { echo "SKIP: no asan runtime source at $RTSRC"; exit 77; }
QEMU=$(command -v qemu-riscv64 || command -v qemu-riscv64-static || true)
[ -n "$QEMU" ] || { echo "SKIP: no qemu-riscv64"; exit 77; }

rm -rf "$W"; mkdir -p "$W"
CC="$MCC -B $CROSS --sysroot=$SR -I$SR/usr/include -L$SR/usr/lib64 -L$SR/lib64"

# shellcheck disable=SC2086
$CC -c "$RTSRC" -o "$W/asan.o" >"$W/rt.log" 2>&1 || {
	echo "SKIP: riscv64 mcc could not build the asan runtime"; sed 's/^/  /' "$W/rt.log" | head -5; exit 77; }

build_run() {
	# shellcheck disable=SC2086
	$CC -fasan-shadow -c "$W/$1.c" -o "$W/$1.o" >"$W/$1.log" 2>&1 || return 1
	# shellcheck disable=SC2086
	$CC "$W/$1.o" "$W/asan.o" -o "$W/$1.bin" >>"$W/$1.log" 2>&1 || return 1
	"$QEMU" -L "$SR" "$W/$1.bin" >"$W/$1.out" 2>&1
	return 0
}

fails=0
expect() {
	name=$1; want=$2
	if ! build_run "$name"; then
		echo "FAIL: could not build/link $name"; sed 's/^/  /' "$W/$name.log" | head -5
		fails=$((fails + 1)); return
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
cat >"$W/ok.c" <<'EOF'
extern void *malloc(unsigned long);
int main(void) {
	char *p = malloc(64);
	int i;
	for (i = 0; i < 64; i++)
		p[i] = (char)i;
	return p[63] == 63 ? 0 : 7;
}
EOF

expect wr "WRITE of size 1"
expect rd "READ of size 1"
expect w4 "WRITE of size 4"
expect uaf "READ of size 1"

if ! build_run ok; then
	echo "FAIL: could not build/link the clean program"; fails=$((fails + 1))
else
	if ! "$QEMU" -L "$SR" "$W/ok.bin" >"$W/ok.out" 2>&1; then
		echo "FAIL: a program with no invalid access trapped under -fasan-shadow"
		sed 's/^/  /' "$W/ok.out" | head -8
		fails=$((fails + 1))
	fi
fi

[ "$fails" -eq 0 ] || exit 1
echo "PASS: riscv64 -fasan-shadow labels READ/WRITE and leaves valid accesses alone"
