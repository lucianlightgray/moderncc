#!/bin/sh
# A struct returned in two registers of different classes drives
# arch_transfer_ret_regs, and the AST replay pushed the sret temp with
# VT_NONLVAL already set -- riscv64's assert(vtop->r == (VT_LOCAL | VT_LVAL))
# then aborted the compiler outright at -O1 and above. x86_64 has no assert
# there and silently tolerated it, so only riscv64 shows it. Compile-only:
# the abort was in the compiler, not the program.
#
# Usage: xferret-nonlval.sh <mcc> <crossdir> <sysroot> <src> <out>
set -eu

MCC=$1
CROSS=$2
SR=$3
SRC=$4
OUT=$5

[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no sysroot at $SR"; exit 77; }

exec "$MCC" -w -O2 -B "$CROSS" "--sysroot=$SR" "-I$SR/usr/include" \
	-c "$SRC" -o "$OUT"
