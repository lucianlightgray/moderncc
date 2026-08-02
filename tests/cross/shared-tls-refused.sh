#!/bin/sh
set -e

ARCH=$1
MCC=$2
CROSS=$3
SR=$4
W=$5
[ -n "$ARCH" ] && [ -n "$MCC" ] && [ -n "$W" ] || {
	echo "usage: shared-tls-refused.sh <arch> <mcc> <crossdir> <sysroot> <workdir>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no $ARCH mcc at $MCC"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR"; exit 77; }

rm -rf "$W"; mkdir -p "$W"
cat >"$W/t.c" <<'EOF'
__thread int tv = 7;
int get(void) { return tv; }
EOF

ABI=""
LIBS="-L$SR/usr/lib64 -L$SR/lib64 -L$SR/usr/lib -L$SR/lib"
[ "$ARCH" = arm ] && ABI="-mfloat-abi hard"

rc=0
"$MCC" -O1 $ABI -shared -B "$CROSS" "--sysroot=$SR" "-I$SR/usr/include" $LIBS \
	"$W/t.c" -o "$W/t.so" >"$W/out" 2>"$W/err" || rc=$?

if [ "$rc" -eq 0 ]; then
	echo "FAIL: -shared with TLS linked cleanly on $ARCH; it used to bake a"
	echo "  local-exec offset and read the executable's thread-local instead"
	exit 1
fi
if ! grep -q "dynamic TLS is not implemented for $ARCH" "$W/err"; then
	echo "FAIL: $ARCH refused, but not with the local-exec TLS diagnostic:"
	sed 's/^/  /' "$W/err" | head -3
	exit 1
fi
echo "PASS: $ARCH refuses local-exec TLS in a shared object"
