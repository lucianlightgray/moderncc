#!/bin/sh
# Runtime-verify mcc's PE (Windows) codegen under wine: for each win32 cross
# target, assemble a -B tree (crt objects + libmcc1.a + the .def import libs),
# link every tests/qemu/conformance/*.c into a PE executable, and run it under
# wine. Each conformance program self-checks and must exit 0.
#
# Usage: run_pe_wine.sh <src-dir> <cross-build-dir> <work-dir>
# Skips (exit 0) cleanly if wine or the win32 cross compilers are absent.
set -eu

SRC=$1
XB=$2
WORK=$3
CONF="$SRC/tests/qemu/conformance"

# pick a wine: prefer plain wine/wine64, fall back to a proton build
pick() { for c in "$@"; do command -v "$c" >/dev/null 2>&1 && { echo "$c"; return; }; done; }
WINE64=$(pick wine64 wine wine64-proton-10.0.4)
WINE32=$(pick wine wine32 wine-proton-10.0.4)
if [ -z "${WINE64:-}" ] && [ -z "${WINE32:-}" ]; then
    echo "SKIP: no wine found"; exit 0
fi

WINEDEBUG=-all; export WINEDEBUG
WINEPREFIX="$WORK/.wineprefix"; export WINEPREFIX

status=0
for tgt in x86_64-win32 i386-win32; do
    MCC="$XB/$tgt-mcc"
    [ -x "$MCC" ] || { echo "SKIP $tgt: no $tgt-mcc"; continue; }
    [ "$tgt" = i386-win32 ] && WINE=$WINE32 || WINE=$WINE64
    [ -n "$WINE" ] || { echo "SKIP $tgt: no matching wine"; continue; }

    B="$WORK/B-$tgt"
    rm -rf "$B"; mkdir -p "$B/lib"
    cp "$SRC"/runtime/win32/lib/*.def "$B/lib/" 2>/dev/null || true
    cp "$XB/lib-$tgt"/*.o "$B/lib/" 2>/dev/null || true
    cp "$XB/lib-$tgt"/*.o "$B/"     2>/dev/null || true
    cp "$XB/$tgt-libmcc1.a" "$B/lib/" 2>/dev/null || true
    cp "$XB/$tgt-libmcc1.a" "$B/"     2>/dev/null || true

    for f in "$CONF"/*.c; do
        n=$(basename "$f" .c)
        exe="$WORK/pe_${tgt}_${n}.exe"
        if ! "$MCC" -B"$B" -I"$SRC/runtime/win32/include" -I"$SRC/runtime/include" \
                 "$f" -o "$exe" 2>"$WORK/err.txt"; then
            echo "FAIL $tgt/$n (compile): $(head -1 "$WORK/err.txt")"; status=1; continue
        fi
        if "$WINE" "$exe" >/dev/null 2>&1; then
            echo "PASS $tgt/$n"
        else
            echo "FAIL $tgt/$n (run, rc=$?)"; status=1
        fi
        rm -f "$exe"
    done
done
exit $status
