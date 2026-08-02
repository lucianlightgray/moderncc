#!/bin/sh
# A shared object must not get local-exec TLS.
#
# This guards a SILENT WRONG ANSWER, not a link failure. Before the refusal
# landed, `-shared` on arm/arm64/riscv64 linked at rc=0 with no diagnostic and
# baked a thread-pointer-relative offset of 0 with no dynamic relocations, so
# the library read the EXECUTABLE's first thread-local instead of its own --
# `get()` returned 1 rather than 7 once the main program had any __thread of
# its own. Dynamic TLS (GD) is not implemented on these backends, so the only
# correct behaviour available is to refuse.
#
# Asserts the diagnostic AND a non-zero exit, because mcc writes the output
# file before the relocation pass reports: a test that only checked for the
# file's absence would pass vacuously.
#
# Usage: shared-tls-refused.sh <arch> <mcc> <crossdir> <sysroot> <workdir>
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
# shellcheck disable=SC2086
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
