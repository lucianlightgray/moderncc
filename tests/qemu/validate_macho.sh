#!/bin/sh
# Structurally validate mcc's Mach-O (Darwin) codegen: for each osx cross
# target, link every tests/qemu/conformance/*.c into a Mach-O executable and
# confirm a Mach-O parser (llvm-otool/otool) reads its header and all load
# commands. (Executing Mach-O binaries needs macOS or darling, which are not
# present here; this is the strongest verification available off-Darwin.)
#
# Usage: validate_macho.sh <src-dir> <cross-build-dir> <work-dir>
# Skips (exit 0) when a Mach-O parser or the osx cross compilers are absent.
set -eu

SRC=$1
XB=$2
WORK=$3
CONF="$SRC/tests/qemu/conformance"

pick() { for c in "$@"; do command -v "$c" >/dev/null 2>&1 && { echo "$c"; return; }; done; }
OTOOL=$(pick llvm-otool otool /usr/lib/llvm/22/bin/llvm-otool)
[ -n "${OTOOL:-}" ] || { echo "SKIP: no Mach-O parser (otool/llvm-otool)"; exit 0; }

mkdir -p "$WORK"
status=0
for tgt in x86_64-osx arm64-osx; do
    MCC="$XB/$tgt-mcc"
    [ -x "$MCC" ] || { echo "SKIP $tgt: no $tgt-mcc"; continue; }
    for f in "$CONF"/*.c; do
        n=$(basename "$f" .c)
        # These pull in the target C library (memset/memmove/abort/<string.h>),
        # which needs the macOS SDK (libSystem) — absent here. Validate only the
        # codegen-self-contained programs.
        case $n in aggregates|libc|varargs)
            echo "SKIP $tgt/$n (needs macOS libSystem)"; continue;; esac
        exe="$WORK/macho_${tgt}_${n}"
        if ! "$MCC" -I"$SRC/runtime/include" "$f" -o "$exe" 2>"$WORK/err.txt"; then
            # No macOS SDK here, so a program referencing libSystem (memset,
            # abort, <string.h>, ...) can't be linked; that's an environment
            # limit, not a codegen defect, so skip it rather than fail.
            if grep -q "unresolved reference\|not found" "$WORK/err.txt"; then
                echo "SKIP $tgt/$n (needs macOS libSystem)"; continue
            fi
            echo "FAIL $tgt/$n (link): $(head -1 "$WORK/err.txt")"; status=1; continue
        fi
        # MH_MAGIC_64 (little-endian: cf fa ed fe) and a parseable load-command table
        magic=$(od -An -tx1 -N4 "$exe" | tr -d ' ')
        if [ "$magic" != cffaedfe ]; then
            echo "FAIL $tgt/$n: bad Mach-O magic ($magic)"; status=1
        elif "$OTOOL" -l "$exe" >/dev/null 2>&1; then
            echo "PASS $tgt/$n (valid Mach-O)"
        else
            echo "FAIL $tgt/$n: otool could not parse load commands"; status=1
        fi
        rm -f "$exe"
    done
done
exit $status
