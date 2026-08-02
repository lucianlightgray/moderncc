#!/bin/sh
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
ARCH=$5
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: tlv.sh <mcc> <srcdir> <workdir> [-B<prefix>] [arch]" >&2
	exit 2
}

[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: needs a Darwin host"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
[ -n "$ARCH" ] || ARCH=$(uname -m)

if [ "$(uname -m)" = arm64 ] && [ "$ARCH" = x86_64 ]; then
	echo "SKIP: x86_64 TLV under Rosetta wedges on an arm64 host; run natively"
	exit 77
fi

TIMEOUT=${MCC_TEST_TIMEOUT:-60}
run_bounded() {
	"$@" &
	_cp=$!
	( sleep "$TIMEOUT"; kill -9 "$_cp" 2>/dev/null ) &
	_kp=$!
	_rc=0
	wait "$_cp" || _rc=$?
	kill "$_kp" 2>/dev/null || true
	wait "$_kp" 2>/dev/null || true
	return "$_rc"
}
CC=clang
command -v "$CC" >/dev/null 2>&1 || { echo "SKIP: no clang to build the TLS object"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

SDKL=""
sdk=$(xcrun --show-sdk-path 2>/dev/null || true)
[ -n "$sdk" ] && [ -d "$sdk/usr/lib" ] && SDKL="-L$sdk/usr/lib"

echo 'int main(void) { return 0; }' > "$WORK/probe.c"
"$CC" -arch "$ARCH" "$WORK/probe.c" -o "$WORK/probe" 2>/dev/null ||
	{ echo "SKIP: this clang cannot target $ARCH"; exit 77; }
prc=0
run_bounded "$WORK/probe" >/dev/null 2>&1 || prc=$?
[ "$prc" = 0 ] || { echo "SKIP: this host cannot execute $ARCH images (rc=$prc)"; exit 77; }

cat > "$WORK/lib.c" <<'EOF'
_Thread_local int tv = 42;
int get_tv(void) { return tv; }
void set_tv(int x) { tv = x; }
EOF

cat > "$WORK/own.c" <<'EOF'
extern int printf(const char *, ...);
_Thread_local int a = 5;
_Thread_local int b;
int main(void) { b = a * 3; printf("a=%d b=%d\n", a, b); return 0; }
EOF

cat > "$WORK/imported.c" <<'EOF'
extern int printf(const char *, ...);
int get_tv(void);
void set_tv(int);
int main(void) { printf("tv=%d\n", get_tv()); set_tv(7); printf("tv=%d\n", get_tv()); return 0; }
EOF

cat > "$WORK/mixed.c" <<'EOF'
extern int printf(const char *, ...);
int get_tv(void);
void set_tv(int);
_Thread_local int mine = 11;
int main(void) { set_tv(4); printf("mine=%d tv=%d\n", mine, get_tv()); return 0; }
EOF

"$CC" -arch "$ARCH" -c -O1 "$WORK/lib.c" -o "$WORK/lib.o"

otool -l "$WORK/lib.o" | grep -q __thread_vars || {
	echo "FAIL: $CC -arch $ARCH emitted no __thread_vars section; cases 2 and 3"
	echo "  would be vacuous -- check the compiler above"
	exit 1
}

fail=0
for case in own imported mixed; do
	objs=""
	[ "$case" = own ] || objs="$WORK/lib.o"

	if ! "$MCC" $BFLAG $SDKL -O1 "$WORK/$case.c" $objs -o "$WORK/$case.mcc" 2>"$WORK/$case.err"; then
		echo "FAIL[$case]: mcc could not link:"; sed 's/^/    /' "$WORK/$case.err"
		fail=1; continue
	fi
	"$CC" -arch "$ARCH" -O1 "$WORK/$case.c" $objs -o "$WORK/$case.ref"

	rc=0
	run_bounded "$WORK/$case.mcc" > "$WORK/$case.out" 2>"$WORK/$case.rt" || rc=$?
	if [ "$rc" != 0 ]; then
		echo "FAIL[$case]: mcc-linked image did not run (rc=$rc):"; sed 's/^/    /' "$WORK/$case.rt"
		fail=1; continue
	fi
	run_bounded "$WORK/$case.ref" > "$WORK/$case.refout" 2>/dev/null || {
		echo "FAIL[$case]: the $CC-built reference did not run cleanly"; fail=1; continue
	}
	if ! cmp -s "$WORK/$case.out" "$WORK/$case.refout"; then
		echo "FAIL[$case]: output differs from the $CC-built reference"
		diff "$WORK/$case.refout" "$WORK/$case.out" | sed 's/^/    /' || true
		fail=1; continue
	fi
	echo "  $case: OK ($(tr '\n' ' ' < "$WORK/$case.out"))"
done

[ "$fail" = 0 ] || exit 1
echo "macho-tlv ($ARCH): OK"
