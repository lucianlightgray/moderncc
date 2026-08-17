#!/bin/sh
# T-mac-30028: -e/--entry (linker entry-point override) must be honored on
# Mach-O EXE output, like ELF/PE.  It was silently ignored (entry hardcoded to
# "main"), so `-Wl,-e,SYM` ran main anyway.
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: entry.sh <mcc> <srcdir> <workdir> [-B<prefix>]" >&2
	exit 2
}

mkdir -p "$WORK"
cat > "$WORK/entry.c" <<'EOF'
#include <stdlib.h>
int main(void) { return 1; }
int altstart(void) { exit(42); }
EOF

# default entry -> main -> exit 1
"$MCC" $BFLAG "$WORK/entry.c" -o "$WORK/def" || exit 1
set +e
"$WORK/def"; rc=$?
set -e
[ "$rc" = 1 ] || { echo "FAIL: default entry ran, expected exit 1, got $rc" >&2; exit 1; }

# -Wl,-e,altstart -> altstart -> exit 42
"$MCC" $BFLAG "$WORK/entry.c" -Wl,-e,altstart -o "$WORK/alt" || exit 1
set +e
"$WORK/alt"; rc=$?
set -e
[ "$rc" = 42 ] || { echo "FAIL: -e altstart not honored, expected exit 42, got $rc" >&2; exit 1; }

# --entry= via -Wl form -> altstart
"$MCC" $BFLAG "$WORK/entry.c" -Wl,--entry=altstart -o "$WORK/alt2" || exit 1
set +e
"$WORK/alt2"; rc=$?
set -e
[ "$rc" = 42 ] || { echo "FAIL: -Wl,--entry= not honored, expected 42, got $rc" >&2; exit 1; }

echo "PASS: Mach-O -e/--entry honored (default main=1, altstart=42)"
exit 0
