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
OPTLBL=$OPT${C2_FORCE:+ FORCED}
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
#
# On a Darwin host they are the two Mach-O keys, and which of the two is native
# depends on the silicon: on Apple silicon arm64-osx is native and x86_64-osx
# runs under Rosetta; on an Intel mac x86_64-osx is native and arm64-osx has no
# runner at all. Nothing ELF and nothing PE executes there.
case "$(uname -s 2>/dev/null)" in
MINGW*|MSYS*|CYGWIN*) HOSTKIND=win32 ;;
Darwin)               HOSTKIND=darwin ;;
*)                    HOSTKIND=linux ;;
esac
NATOUT=$OUT
if [ "$HOSTKIND" = win32 ]; then
	KEYS_ALL="x86_64-win32 i386-win32"
	# Env var VALUES are not path-converted by MSYS (argv is), so the parser
	# leg's MCC_REPLAY_IR_OUT must be a native path or mcc fails to open it and
	# the [rir-*] diagnostics land in the captured output -- every golden then
	# mismatches on leg B for a harness reason.
	NATOUT=$(cygpath -m "$OUT")
elif [ "$HOSTKIND" = darwin ]; then
	case "$(uname -m)" in
	arm64)  KEYS_ALL="arm64-osx x86_64-osx" ;;
	*)      KEYS_ALL="x86_64-osx" ;;
	esac
else
	KEYS_ALL="x86_64 i386 arm arm64 riscv64"
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
		$FORCEENV $_extra \
		"$R" "$_mcc" "$_bdir" "$S/runtime/include" "$S/tests" "$_work" \
		> "$OUT/$_key.$_tag.txt" 2>/dev/null || true
}

# C2_FORCE=1 -- the forced--O0 arm, and the ONLY way -O0 enters this instrument.
# The differential turns on production, and production is
# rir_prod_env = ast_replay_env && !rir_env with ast_replay_env =
# `optimize >= 1 || embed_jit || MCC_FORCE_REPLAY || MCC_RIR_FORCE`
# (src/mccast.c:1923-1925). At a plain -O0 no term holds, so BOTH legs ship the
# parser's bytes and the differential is NONE over a population with zero
# Replay_IR in it -- a green row that measures nothing. Measured on this tree at
# -O0 on tests/exec/programs/grep.c: [rir-prod-total] used=0 without this, used=9
# fallback=4 with it. Do not read a -O0 row that was taken without it.
#
# Note this is NOT the same thing C2_FORCE does in tools/c2_sweep.sh. That
# script runs under MCC_REPLAY_IR=5, and capture is armed by
# `rir_env || rir_prod_env` (src/mccrir.c:535), so rir_env alone already gives
# the byte board a full -O0 population -- there the 28 gates are the whole
# effect and MCC_FORCE_REPLAY is inert. Here rir_env is 0 in the arena leg, so
# MCC_FORCE_REPLAY is what arms the arena and the gates only make the -O0 leg
# comparable with an optimized one.
#
# The gate list is derived by the same regex over src/ that tools/c2_sweep.sh
# uses, so a renamed gate cannot silently drop out of the set, and a derivation
# of zero aborts rather than measuring -O0 with every pass off and calling it
# parity.
FORCEENV=
if [ -n "$C2_FORCE" ]; then
	gates=$(grep -hoE 'ast_env_gate\("MCC_AST_[A-Z0-9_]+", *o4 \|\| s1->optimize >= 1\)' \
			"$S"/src/*.c | sed -E 's/.*"(MCC_AST_[A-Z0-9_]+)".*/\1/' | sort -u)
	ngate=$(printf '%s\n' $gates | grep -c .)
	if [ "$ngate" -eq 0 ]; then
		echo "c2_equiv: C2_FORCE derived 0 gates from $S/src -- the ast_env_gate" >&2
		echo "  spelling changed, so this run would measure -O0 with every pass" >&2
		echo "  off and call it parity" >&2
		exit 1
	fi
	FORCEENV="MCC_FORCE_REPLAY=1"
	for g in $gates; do FORCEENV="$FORCEENV $g=1"; done
	echo "c2_equiv: C2_FORCE -- $ngate optimize>=1 gate(s) forced on" >&2
fi

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
				printf '%-10s UNMEASURABLE  runtime pieces absent in %s (need i386-win32-libmccrt.a, lib-i386-win32/, include/)\n' "$k" "$BUILD"
				rc=1; continue
			fi
			rm -rf "$bdir"
			mkdir -p "$bdir/lib"
			cp "$BUILD/i386-win32-libmccrt.a" "$BUILD"/i386-win32-*.o "$bdir/lib/"
			cp "$BUILD/lib-i386-win32/"* "$bdir/lib/"
			cp "$S/runtime/win32/lib/"*.def "$bdir/lib/"
			cp -r "$BUILD/include" "$bdir/include" ;;
		*)
			printf '%-10s UNMEASURABLE  no runner on this host (see W1/W2)\n' "$k"
			continue ;;
		esac
	elif [ "$HOSTKIND" = darwin ]; then
		# Mach-O. cpu/os drive req_met (tests/runner.c:32-33); getting them wrong
		# does not fail a golden loudly, it silently runs the ELF-only ones and
		# reports them as mismatches, or skips ones that should have run.
		os=Darwin
		cpu=${k%-osx}
		case "$k" in
		*-osx) ;;
		*)
			printf '%-10s UNMEASURABLE  no runner on this host (see W1/W2)\n' "$k"
			continue ;;
		esac
		if [ "$cpu" != "$(uname -m)" ]; then
			# The non-native Mach-O key. On Apple silicon x86_64 runs under
			# Rosetta, which is a RUN-time prefix only -- the compiler is a host
			# binary either way. And a cross Mach-O link needs the SDK's
			# libSystem, which the runner only adds when it thinks it is cross,
			# and it decides that from MCC_TEST_RUNEMU alone (tests/runner.c:578).
			# So the run prefix is also what turns the -L on: both are required,
			# and neither works without the other.
			[ "$cpu/$(uname -m)" = "x86_64/arm64" ] || {
				printf '%-10s UNMEASURABLE  no runner for %s on %s (W1)\n' "$k" "$cpu" "$(uname -m)"
				rc=1; continue; }
			arch -x86_64 /usr/bin/true >/dev/null 2>&1 || {
				printf '%-10s UNMEASURABLE  Rosetta absent: arch -x86_64 cannot execute\n' "$k"
				rc=1; continue; }
			emu="arch -x86_64"
			sysroot=$(xcrun --show-sdk-path 2>/dev/null || true)
			[ -n "$sysroot" ] && [ -d "$sysroot/usr/lib" ] || {
				printf '%-10s UNMEASURABLE  no macOS SDK (xcrun --show-sdk-path)\n' "$k"
				rc=1; continue; }
		fi
	else
	case "$k" in
	x86_64)  mcc=$BUILD/mcc; emu=; sysroot= ;;
	i386)    emu=qemu-i386 ;;
	arm)     emu=qemu-arm ;;
	arm64)   emu=qemu-aarch64 ;;
	riscv64) emu=qemu-riscv64 ;;
	*)
		printf '%-10s UNMEASURABLE  no runner on this host (see W1/W2)\n' "$k"
		continue ;;
	esac
	if [ "$k" != x86_64 ]; then
		sysroot=$S/vendor/gentoo-stage3-$k-glibc
		if [ ! -d "$sysroot" ]; then
			printf '%-10s UNMEASURABLE  sysroot absent: %s\n' "$k" "$sysroot"
			rc=1; continue
		fi
		if ! command -v "$emu" >/dev/null 2>&1; then
			printf '%-10s UNMEASURABLE  emulator absent: %s\n' "$k" "$emu"
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
		printf '%-10s UNMEASURABLE  no compiler: %s\n' "$k" "$mcc"
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
		printf '%-10s %s UNMEASURABLE  pass=%s/%s fail=%s/%s -- nothing executed, no differential reported\n' \
			"$k" "$OPTLBL" "$pa" "$pb" "$fa" "$fb"
		rc=1; continue
	fi

	grep -E '^FAIL' "$OUT/$k.arena.txt"  | awk '{print $2}' | sort > "$OUT/$k.arena.fail"
	grep -E '^FAIL' "$OUT/$k.parser.txt" | awk '{print $2}' | sort > "$OUT/$k.parser.fail"
	only_parser=$(comm -13 "$OUT/$k.arena.fail" "$OUT/$k.parser.fail" | tr '\n' ' ')
	only_arena=$(comm -23 "$OUT/$k.arena.fail" "$OUT/$k.parser.fail" | tr '\n' ' ')

	if [ -z "$only_parser" ] && [ -z "$only_arena" ]; then
		printf '%-10s %s pass=%s/%s  differential: NONE  (%s goldens agree)\n' \
			"$k" "$OPTLBL" "$pa" "$pb" "$pa"
	else
		printf '%-10s %s pass=%s/%s  DIFFERENTIAL  arena-only:[%s] parser-only:[%s]\n' \
			"$k" "$OPTLBL" "$pa" "$pb" "$only_arena" "$only_parser"
		rc=1
	fi
done
exit $rc
