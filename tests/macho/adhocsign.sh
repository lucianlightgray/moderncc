#!/bin/sh
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: adhocsign.sh <mcc> <srcdir> <workdir> [-B<prefix>]" >&2
	exit 2
}

[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: needs a Darwin host"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
command -v codesign >/dev/null 2>&1 || { echo "SKIP: no codesign to verify against"; exit 77; }
command -v python3 >/dev/null 2>&1 || { echo "SKIP: python3 not found"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

cat > "$WORK/sig.c" <<'EOF'
#include <stdio.h>
int add(int a, int b) { return a + b; }
int main(void) { printf("%d\n", add(19, 23)); return 0; }
EOF

sign() {
	MCC_MACHO_ADHOC_SIGN=1 "$MCC" $BFLAG "$WORK/sig.c" -o "$1"
}

mkdir -p "$WORK/one" "$WORK/two"
sign "$WORK/one/a"
ln -sf one/a "$WORK/a" 2>/dev/null || cp "$WORK/one/a" "$WORK/a"
[ -x "$WORK/a" ] || { echo "FAIL: mcc produced no binary" >&2; exit 1; }

out=$("$WORK/a") || { echo "FAIL: the signed binary did not run (kernel rejected the signature?)" >&2; exit 1; }
[ "$out" = "42" ] || { echo "FAIL: wrong output: $out" >&2; exit 1; }
echo "ok: kernel accepted the signature and the binary ran"

codesign -v --verbose=2 "$WORK/a" || { echo "FAIL: codesign rejected mcc's signature" >&2; exit 1; }

codesign -d --verbose=2 "$WORK/a" 2>&1 | grep -q "flags=0x2(adhoc)" || {
	echo "FAIL: not reported as an ad-hoc signature" >&2
	codesign -d --verbose=2 "$WORK/a" 2>&1 >&2
	exit 1
}
echo "ok: codesign validates it as ad-hoc"

sign "$WORK/two/a"
cmp "$WORK/one/a" "$WORK/two/a" || {
	echo "FAIL: two signs of identical input differ; the signature is not reproducible" >&2
	exit 1
}
echo "ok: signature is reproducible"

cp "$WORK/a" "$WORK/tampered"
python3 - "$WORK/tampered" <<'EOF'
import struct, sys
p = sys.argv[1]
blob = open(p, "rb").read()
off = 0x1010
f = open(p, "r+b")
f.seek(off)
b = f.read(1)
f.seek(off)
f.write(bytes([b[0] ^ 0xFF]))
f.close()
EOF
if codesign -v "$WORK/tampered" >/dev/null 2>&1; then
	echo "FAIL: codesign accepted a tampered binary; the page hashes are not real" >&2
	exit 1
fi
echo "ok: a one-byte change invalidates the signature"

echo "macho-adhocsign: OK"
