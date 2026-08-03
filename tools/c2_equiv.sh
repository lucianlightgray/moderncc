#!/bin/sh
# The arena-vs-parser semantic differential.
#
# The C2 board compares BYTES. This compares BEHAVIOUR. The whole instrument is
# one line in src/mccrir.c -- rir_prod_env = ast_replay_env && !rir_env, with no
# gate term -- so the same source built twice at the same -O, with MCC_REPLAY_IR
# unset and then set, ships the arena's emission and then the parser's bytes.
# Run both, compare program output, and a match is a semantic proof for every
# body that executed. No effect model, no disassembly, and indifferent to
# looping and nesting because it observes the whole program.
#
#   tools/c2_equiv.sh <builddir> [key|all] [opt] [outdir]
#
# tests/runner.c already compares stdout against a golden, so a key's leg is
# "did any golden change verdict", not a text diff of its own.
#
# Two things this script refuses to do, both paid for once already:
#
#  - It will not print a differential for a key whose pass count is ZERO. The
#    four ELF keys were once run without sysroot flags, compiled nothing, and
#    reported "differential: NONE" on 0 passed / 252 failed -- a green row out of
#    an empty population. A differential over nothing is not a clean result.
#  - It will not measure a key whose sysroot or emulator is absent. Same reason
#    tools/o0_ab.sh:174 refuses: the run still exits 0 and still prints a
#    plausible row.
#
# MCC_REPLAY_IR_OUT is not optional. Without it the [rir-*] diagnostics land in
# the captured output and every golden mismatches on leg B.
set -e

BUILD=$1
KEYARG=${2:-all}
OPT=${3:--O1}
OUT=${4:-/tmp/c2equiv}

if [ -z "$BUILD" ]; then
	echo "usage: tools/c2_equiv.sh <builddir> [key|all] [opt] [outdir]" >&2
	exit 2
fi

S=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$(cd "$BUILD" && pwd)
R=$BUILD/exec_runner
[ -x "$R" ] || { echo "c2_equiv: no exec_runner in $BUILD" >&2; exit 2; }
mkdir -p "$OUT"

# Only keys whose output can actually be RUN on this host appear here. The
# Mach-O keys and the ARM/ARM64 PE keys cannot execute on an x86_64 Linux host
# at all (W1/W2), so they are not listed rather than listed and skipped -- a
# semantic differential is meaningless without execution.
KEYS_ALL="x86_64 i386 arm arm64 riscv64"

case "$KEYARG" in
all) KEYS=$KEYS_ALL ;;
*)   KEYS=$KEYARG ;;
esac

# leg <tag> <key> <mcc> <sysroot> <runemu> <cpu>  -> writes $OUT/<key>.<tag>.txt
leg() {
	_tag=$1; _key=$2; _mcc=$3; _sys=$4; _emu=$5; _cpu=$6
	_work=$OUT/$_key.$_tag.work
	rm -rf "$_work"
	if [ "$_tag" = parser ]; then
		_extra="MCC_REPLAY_IR=1 MCC_REPLAY_IR_OUT=$OUT/$_key.rir.log"
	else
		_extra=
	fi
	# MCC_TEST_RUNEMU, never MCC_TEST_EMU: the latter prefixes the COMPILER, and
	# mcc-<key> is a host binary that merely targets <key>, so qemu rejects it as
	# "Invalid ELF image for this architecture".
	env MCC_TEST_EMU= MCC_TEST_CPU="$_cpu" MCC_TEST_OS=Linux \
		MCC_TEST_ASM=1 MCC_TEST_BCHECK=0 MCC_TEST_BACKTRACE=0 \
		MCC_TEST_SYSROOT="$_sys" MCC_TEST_RUNEMU="$_emu" MCC_TEST_OPT="$OPT" \
		$_extra \
		"$R" "$_mcc" "$BUILD" "$S/runtime/include" "$S/tests" "$_work" \
		> "$OUT/$_key.$_tag.txt" 2>/dev/null || true
}

# summary line is "exec runner: N passed, M failed, K skipped (of T)"
passof() { sed -n 's/^exec runner: \([0-9]*\) passed.*/\1/p' "$1"; }
failof() { sed -n 's/^exec runner: [0-9]* passed, \([0-9]*\) failed.*/\1/p' "$1"; }

rc=0
for k in $KEYS; do
	sysroot=
	emu=
	cpu=$k
	mcc=$BUILD/mcc-$k
	case "$k" in
	x86_64)  mcc=$BUILD/mcc; emu=; sysroot= ;;
	i386)    emu=qemu-i386 ;;
	arm)     emu=qemu-arm ;;
	arm64)   emu=qemu-aarch64 ;;
	riscv64) emu=qemu-riscv64 ;;
	*)
		printf '%-9s UNMEASURABLE  no runner on this host (see W1/W2)\n' "$k"
		continue ;;
	esac
	if [ "$k" != x86_64 ]; then
		sysroot=$S/vendor/gentoo-stage3-$k-glibc
		if [ ! -d "$sysroot" ]; then
			printf '%-9s UNMEASURABLE  sysroot absent: %s\n' "$k" "$sysroot"
			rc=1; continue
		fi
		if ! command -v "$emu" >/dev/null 2>&1; then
			printf '%-9s UNMEASURABLE  emulator absent: %s\n' "$k" "$emu"
			rc=1; continue
		fi
		# qemu-user without -L resolves the interpreter against the HOST root, so
		# an i386 binary loads the host's x86_64 libc and dies with "CPU ISA level
		# is lower than required" -- which reads as a codegen failure and is not
		# one. The sysroot has to be handed to the emulator, not only to mcc.
		emu="$emu -L $sysroot"
	fi
	if [ ! -x "$mcc" ]; then
		printf '%-9s UNMEASURABLE  no compiler: %s\n' "$k" "$mcc"
		rc=1; continue
	fi

	leg arena  "$k" "$mcc" "$sysroot" "$emu" "$cpu"
	leg parser "$k" "$mcc" "$sysroot" "$emu" "$cpu"

	pa=$(passof "$OUT/$k.arena.txt"); pb=$(passof "$OUT/$k.parser.txt")
	fa=$(failof "$OUT/$k.arena.txt"); fb=$(failof "$OUT/$k.parser.txt")
	: "${pa:=0}" "${pb:=0}" "${fa:=0}" "${fb:=0}"

	# The refusal that matters. A differential over a population that never ran
	# is not "NONE", it is nothing, and it reads identical to a clean result.
	if [ "$pa" -eq 0 ] || [ "$pb" -eq 0 ]; then
		printf '%-9s %s UNMEASURABLE  pass=%s/%s fail=%s/%s -- nothing executed, no differential reported\n' \
			"$k" "$OPT" "$pa" "$pb" "$fa" "$fb"
		rc=1; continue
	fi

	grep -E '^FAIL' "$OUT/$k.arena.txt"  | awk '{print $2}' | sort > "$OUT/$k.arena.fail"
	grep -E '^FAIL' "$OUT/$k.parser.txt" | awk '{print $2}' | sort > "$OUT/$k.parser.fail"
	only_parser=$(comm -13 "$OUT/$k.arena.fail" "$OUT/$k.parser.fail" | tr '\n' ' ')
	only_arena=$(comm -23 "$OUT/$k.arena.fail" "$OUT/$k.parser.fail" | tr '\n' ' ')

	if [ -z "$only_parser" ] && [ -z "$only_arena" ]; then
		printf '%-9s %s pass=%s/%s  differential: NONE  (%s goldens agree)\n' \
			"$k" "$OPT" "$pa" "$pb" "$pa"
	else
		printf '%-9s %s pass=%s/%s  DIFFERENTIAL  arena-only:[%s] parser-only:[%s]\n' \
			"$k" "$OPT" "$pa" "$pb" "$only_arena" "$only_parser"
		rc=1
	fi
done
exit $rc
