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
#
# On a Windows x86_64 host (Git Bash) the executable keys are the two x86 PE
# keys instead: x86_64-win32 is the native compiler and i386-win32 runs under
# WOW64. The ELF keys are the unmeasurable ones there.
case "$(uname -s 2>/dev/null)" in
MINGW*|MSYS*|CYGWIN*) HOSTKIND=win32 ;;
*)                    HOSTKIND=linux ;;
esac
if [ "$HOSTKIND" = win32 ]; then
	KEYS_ALL="x86_64-win32 i386-win32"
	# Env var VALUES are not path-converted by MSYS (argv is), so the parser
	# leg's MCC_REPLAY_IR_OUT must be a native path or mcc fails to open it and
	# the [rir-*] diagnostics land in the captured output -- every golden then
	# mismatches on leg B for a harness reason.
	NATOUT=$(cygpath -m "$OUT")
else
	KEYS_ALL="x86_64 i386 arm arm64 riscv64"
	NATOUT=$OUT
fi

case "$KEYARG" in
all) KEYS=$KEYS_ALL ;;
*)   KEYS=$KEYARG ;;
esac

# leg <tag> <key> <mcc> <bdir> <sysroot> <runemu> <cpu> <os>
#   -> writes $OUT/<key>.<tag>.txt
leg() {
	_tag=$1; _key=$2; _mcc=$3; _bdir=$4; _sys=$5; _emu=$6; _cpu=$7; _os=$8
	_work=$OUT/$_key.$_tag.work
	rm -rf "$_work"
	if [ "$_tag" = parser ]; then
		_extra="MCC_REPLAY_IR=1 MCC_REPLAY_IR_OUT=$NATOUT/$_key.rir.log"
	else
		_extra=
	fi
	# MCC_TEST_RUNEMU, never MCC_TEST_EMU: the latter prefixes the COMPILER, and
	# mcc-<key> is a host binary that merely targets <key>, so qemu rejects it as
	# "Invalid ELF image for this architecture".
	env MCC_TEST_EMU= MCC_TEST_CPU="$_cpu" MCC_TEST_OS="$_os" \
		MCC_TEST_ASM=1 MCC_TEST_BCHECK=0 MCC_TEST_BACKTRACE=0 \
		MCC_TEST_SYSROOT="$_sys" MCC_TEST_RUNEMU="$_emu" MCC_TEST_OPT="$OPT" \
		$_extra \
		"$R" "$_mcc" "$_bdir" "$S/runtime/include" "$S/tests" "$_work" \
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
	os=Linux
	bdir=$BUILD
	mcc=$BUILD/mcc-$k
	if [ "$HOSTKIND" = win32 ]; then
		case "$k" in
		x86_64-win32)
			# The exact configuration the native exec/ ctest cells run green
			# with: mcc + -B <builddir>, no sysroot, no runner prefix.
			mcc=$BUILD/mcc; cpu=x86_64; os=WIN32 ;;
		i386-win32)
			cpu=i386; os=WIN32
			# The runner passes a single -B and mcc-i386-win32's baked MCCDIR
			# is the INSTALL prefix, which need not exist. -B <builddir> alone
			# fails: the archive is at <builddir>/i386-win32-libmccrt.a, the
			# crt objects in <builddir>/lib-i386-win32/, and the .def import
			# stubs only in runtime/win32/lib/. Assemble the one <B>/lib the
			# runner can point at, plus <B>/include for the win32 headers.
			bdir=$OUT/pe-bdir-$k
			if [ ! -f "$BUILD/i386-win32-libmccrt.a" ] || \
			   [ ! -d "$BUILD/lib-i386-win32" ] || [ ! -d "$BUILD/include" ]; then
				printf '%-9s UNMEASURABLE  runtime pieces absent in %s (need i386-win32-libmccrt.a, lib-i386-win32/, include/)\n' "$k" "$BUILD"
				rc=1; continue
			fi
			rm -rf "$bdir"
			mkdir -p "$bdir/lib"
			cp "$BUILD/i386-win32-libmccrt.a" "$BUILD"/i386-win32-*.o "$bdir/lib/"
			cp "$BUILD/lib-i386-win32/"* "$bdir/lib/"
			cp "$S/runtime/win32/lib/"*.def "$bdir/lib/"
			cp -r "$BUILD/include" "$bdir/include" ;;
		*)
			printf '%-9s UNMEASURABLE  no runner on this host (see W1/W2)\n' "$k"
			continue ;;
		esac
	else
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
	fi
	if [ ! -x "$mcc" ]; then
		printf '%-9s UNMEASURABLE  no compiler: %s\n' "$k" "$mcc"
		rc=1; continue
	fi

	leg arena  "$k" "$mcc" "$bdir" "$sysroot" "$emu" "$cpu" "$os"
	leg parser "$k" "$mcc" "$bdir" "$sysroot" "$emu" "$cpu" "$os"

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
