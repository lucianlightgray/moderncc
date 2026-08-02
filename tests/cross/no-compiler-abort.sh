#!/bin/sh
# No target may CRASH the compiler on the corpus.
#
# A failed compile is a diagnostic; a SIGABRT or SIGSEGV is a bug in mcc with
# no output file and no message the user can act on. riscv64 aborted on
# tests/exec/structs_unions/struct_byval.c at -O1 and above for an unknown
# number of rounds -- assert(vtop->r == (VT_LOCAL | VT_LVAL)) in
# arch_transfer_ret_regs -- because every existing per-target check counts
# ok/fail and a crash simply lands in the fail column next to the files that
# legitimately do not compile for want of a header.
#
# So this asserts on the SIGNAL, not on the exit status: rc >= 128 fails,
# anything else is fine. It deliberately does not care how many files compile,
# which keeps it immune to the low-fn sysroot trap that makes census numbers
# unreliable.
#
# Usage: no-compiler-abort.sh <arch> <mcc> <crossdir> <sysroot|-> <corpus> [opt ...]
set -eu

ARCH=$1
MCC=$2
CROSS=$3
SR=$4
CORPUS=$5
shift 5
OPTS="$*"
[ -n "$OPTS" ] || OPTS="-O2"

[ -x "$MCC" ] || { echo "SKIP: no $ARCH mcc at $MCC"; exit 77; }
[ -d "$CORPUS" ] || { echo "SKIP: no corpus at $CORPUS"; exit 77; }

EXTRA=""
case "$ARCH" in
arm|arm-win32|arm-wince) EXTRA="-mfloat-abi hard" ;;
esac
case "$ARCH" in
*-win32|*-wince)
	R=$(dirname "$CROSS")
	EXTRA="$EXTRA -I$R/runtime/include -I$R/runtime/win32/include -I$R/runtime/win32/include/winapi"
	;;
esac
if [ "$SR" != "-" ]; then
	[ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR"; exit 77; }
	EXTRA="$EXTRA --sysroot=$SR -I$SR/usr/include"
fi

OBJ=$(mktemp)
trap 'rm -f "$OBJ"' EXIT

n=0
crashed=0
for o in $OPTS; do
	for f in $(find "$CORPUS" -name '*.c' ! -name 'runner.c' | sort); do
		n=$((n + 1))
		rc=0
		# shellcheck disable=SC2086
		"$MCC" -w "$o" -B "$CROSS" $EXTRA -c "$f" -o "$OBJ" >/dev/null 2>&1 || rc=$?
		if [ "$rc" -ge 128 ]; then
			crashed=$((crashed + 1))
			echo "CRASH rc=$rc  $MCC $o $f"
		fi
	done
done

if [ "$n" -eq 0 ]; then
	echo "FAIL: no files were compiled; this check is vacuous"
	exit 1
fi
if [ "$crashed" -ne 0 ]; then
	echo "FAIL: $ARCH crashed the compiler on $crashed of $n compile(s) over $OPTS"
	exit 1
fi
echo "PASS: $ARCH did not crash on any of $n compile(s) over $OPTS"
