#!/bin/sh
# x86_64-win32 self-host fixpoint + determinism driver.
#
# selfhost-fixpoint.py is POSIX-only (-ldl, no .exe, unregistered without
# MCC_EMBED_MCCRT), so the mingw cell had no way to prove or refute win32
# self-build nondeterminism on a full mcc-built-by-mcc chain. This stands one
# up, all locally on an x86_64 Windows host (no docker, no qemu):
#
#   1. stage1: a stage0 mcc.exe self-compiles src/mcc.c and links a stage1
#      mcc.exe (a mcc-built mcc). A minimal, embed-jit-free config so the driver
#      needs only the source tree, a stage0 mcc and the configured build dir --
#      no machine-specific embed-jit libdirs. Codegen determinism is what the
#      nondeterminism question is about, and it is orthogonal to the embed blob.
#   2. determinism: stage1 recompiles src/mcc.c twice; the two objects must be
#      byte-identical (nondeterminism would diverge here).
#   3. fixpoint: stage1 builds stage2, stage2 recompiles src/mcc.c; o2 == o3
#      byte-identical is the self-host fixpoint (o1 == o2 is not required -- the
#      stage0 may be built by a different compiler).
#
# Usage:  tools/win32-selfhost.sh <mcc.exe> <build-dir> [opt]
# Exit:   0 deterministic and a fixpoint · 1 a check failed · 77 skipped
set -eu

MCC=${1:-}
BUILD=${2:-}
OPT=${3:--O1}
root=$(cd "$(dirname "$0")/.." && pwd)

skip() { echo "win32-selfhost: SKIP ($1)"; exit 77; }
[ -n "$MCC" ] && [ -x "$MCC" ] || skip "no stage0 mcc ($MCC)"
[ -n "$BUILD" ] && [ -d "$BUILD" ] || skip "no build dir ($BUILD) for generated headers"

# This driver builds a PE self-host; a non-PE mcc is out of scope here.
case $("$MCC" -dumpmachine 2>/dev/null || echo "") in
	*mingw*|*windows*|*win32*|*pe*) ;;
	*) skip "stage0 mcc does not target PE ($("$MCC" -dumpmachine 2>/dev/null))" ;;
esac

DEFS="-DCC_NAME=CC_clang -DMCC_CONFIG_OPTIMIZER=1 -DMCC_CONFIG_PREDEFS=1 -DMCC_TARGET_PE=1 -DMCC_TARGET_X86_64=1"
INCS="-I$BUILD -I$root/src -I$root/src/arch/i386 -I$root/src/arch/x86_64 -I$root/src/arch/arm -I$root/src/arch/arm64 -I$root/src/arch/riscv64 -I$root/src/objfmt -I$root/src/formats -I$root/include -I$root"
# win32 needs two -B prefixes: the build dir (lib/libmccrt.a, lib/runmain.o)
# layered under the source runtime (runtime/win32/include over runtime/include).
RT="-B$root/runtime/win32 -I$root/runtime/include"
LNK="-B$BUILD -B$root/runtime/win32 -lkernel32 -luser32 -ladvapi32"

w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT

compile() { "$1" -c -w "$OPT" $RT $DEFS $INCS "$root/src/mcc.c" -o "$2"; }
link()    { "$1" $LNK "$2" -o "$3"; }

echo "== stage1: stage0 mcc self-compiles and links a mcc-built mcc =="
compile "$MCC" "$w/o1.o" || skip "stage0 cannot compile src/mcc.c (header/config path)"
link    "$MCC" "$w/o1.o" "$w/mcc1.exe" || skip "stage0 cannot link a stage1 (runtime path)"

echo "== determinism: stage1 recompiles src/mcc.c twice =="
compile "$w/mcc1.exe" "$w/o2.o"  || { echo "FAIL: stage1 cannot self-compile"; exit 1; }
compile "$w/mcc1.exe" "$w/o2b.o" || { echo "FAIL: stage1 second self-compile"; exit 1; }
if ! cmp -s "$w/o2.o" "$w/o2b.o"; then
	echo "FAIL: NONDETERMINISTIC -- stage1 produced two different objects for one input"
	exit 1
fi

echo "== fixpoint: stage2 recompiles src/mcc.c =="
link    "$w/mcc1.exe" "$w/o2.o" "$w/mcc2.exe" || { echo "FAIL: cannot link stage2"; exit 1; }
compile "$w/mcc2.exe" "$w/o3.o" || { echo "FAIL: stage2 cannot self-compile"; exit 1; }
if ! cmp -s "$w/o2.o" "$w/o3.o"; then
	echo "FAIL: not a fixpoint -- o2 != o3"
	exit 1
fi

echo "win32-selfhost: OK $OPT -- deterministic (o2==o2b) and a fixpoint (o2==o3), $(wc -c <"$w/o2.o") B"
