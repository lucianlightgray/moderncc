#!/bin/sh
#
# o0_ab.sh -- the -O0 A/B harness for the Cut to Replay_IR plan.
#
# Two independent measurements per target key, both at -O0, both banked:
#
#   A. Object identity.  Plain -O0 with no MCC_* in the environment.  The AST
#      recorder does not run at -O0 (ast_replay_env needs optimize >= 1; find
#      it by symbol, it is the assignment to ast_replay_env in ast_configure),
#      so no phase of the cut may move a single one of these bytes.  Recorded
#      as a sorted, diffable per-file sha256.
#
#   B. Forced Replay_IR coverage.  MCC_REPLAY_IR=1 MCC_FORCE_REPLAY=1 at -O0
#      arms Replay_IR over every body independently of the recorder.  The bar
#      is faithful + empty == fn.
#
# Usage:
#   tools/o0_ab.sh <builddir> <key>        [outdir]   one key
#   tools/o0_ab.sh <builddir> all          [outdir]   all twelve keys + gates
#   tools/o0_ab.sh <builddir> measurable   [outdir]   every key this host and
#                                                     build can actually measure
#
# Keys: x86_64 i386 arm arm64 riscv64
#       x86_64-win32 i386-win32 arm64-win32 arm-win32 arm-wince
#       x86_64-osx arm64-osx
#
# "all" is the re-bank spelling and demands every key: a missing cross compiler
# or sysroot is a failure, because a board with a hole in it is not the board.
# "measurable" is the ctest spelling: it drops the keys whose compiler or
# sysroot is absent, names every one it dropped, and refuses to report anything
# at all unless it measured at least O0_AB_MIN_KEYS of them.  That floor is the
# difference between "this build has no cross compilers" and "this check has no
# subject", which are otherwise the same green tick.
#
# Environment:
#   C2_NO_EXTRA=1   drop tests/diff/full_language.c, so a board is like-for-like
#                   with a c2_sweep.sh board taken the same way.
#   O0_AB_GATES=1   additionally force on, for both measurements, every optimizer
#                   knob that some -O level would turn on and -O0 leaves off:
#                   each MCC_OPTD_LEVEL(n) row of src/mccopt.h, passed as
#                   -f<name>.  Until a55c0a07 these were environment variables
#                   and the set was derived by regex for
#                   ast_env_gate("MCC_AST_*", o4 || s1->optimize >= 1) over
#                   src/*.c; that function and those variables are gone, and
#                   the flag table is now the definition rather than a second
#                   copy of it.  The names are re-derived per run, never
#                   remembered; deriving zero of them is fatal.  mcc ignores an
#                   unknown -f silently, so a derivation that still yields
#                   names but the wrong ones cannot be caught that way -- see
#                   the vacuity check in run_key, which requires the gated
#                   object board to differ from the ungated bank's.  It used to
#                   require the gated Replay_IR counters to differ instead; the
#                   only level knob that ever moved that census at -O0 was
#                   inline-functions-called-once, which changed which bodies
#                   exist rather than what they compile to, and which was
#                   deleted for rejecting valid programs at -O7 and above.
#   O0_AB_BANK=1    write the result into tests/ast/o0-baseline/ as well as
#                   into <outdir>.
#   O0_AB_CHECK=1   diff the result against tests/ast/o0-baseline/ and fail on
#                   any drift.  This is the gate every phase of the cut has to
#                   pass.
#   O0_AB_MIN_KEYS  floor on the number of keys "measurable" must have measured
#                   (default 1).  Set it to 12 where every cross compiler is
#                   expected, so that losing one is a failure rather than a
#                   quieter pass.
#   O0_AB_MIN_FILES floor on the corpus size, default 64.  A find(1) that
#                   matched nothing must not be bankable as a baseline of
#                   nothing.
#   O0_AB_MUTATE=1  the known-positive arm: take measurement A at -O1 instead
#                   of -O0.  Every banked object hash must then move, so a
#                   check run that still passes is comparing nothing.
#
# Regenerate the banked baseline (from the repo root, after a
#   cmake -S . -B b -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMCC_ENABLE_CROSS=ON
#   ninja -C b
# build).  Both boards are banked, and the ungated one has to be banked first
# because the gated run's vacuity check reads it:
#
#   C2_NO_EXTRA=1 O0_AB_BANK=1 tools/o0_ab.sh b all /tmp/o0ab
#   C2_NO_EXTRA=1 O0_AB_BANK=1 O0_AB_GATES=1 tools/o0_ab.sh b all /tmp/o0ab-g
#
# Check a tree against the bank the same way:
#
#   C2_NO_EXTRA=1 O0_AB_CHECK=1 tools/o0_ab.sh b all /tmp/o0ab
#
# Baseline layout, tests/ast/o0-baseline/:
#   <key>.obj.txt        one "<key>\t<path>\t<sha256>" line per file that
#                        compiled, sorted, paths relative to the repo root
#   <key>.rir.txt        key=value counters for the forced-O0 run
#   <key>.gated.rir.txt  same, with the level knobs forced on
#   board.txt            one summary line per key, written by "all"
#   board.gated.txt      same, with the level knobs forced on
#
# Only measurement B is banked twice.  Under O0_AB_GATES measurement A is also
# taken with the knobs on, but that board is never banked and never checked for
# drift: it exists only so the vacuity check has something that still moves.
#
# Paths are passed to mcc relative to the repo root on purpose: mcc embeds the
# source file name it was given into the object, so an absolute path would
# make every sha256 a function of where the tree happens to be checked out.
#
# Honesty rules, inherited from tools/c2_sweep.sh, which learned them the hard
# way:
#   * rir_report is an atexit handler, so a file that fails to compile still
#     prints [rir-total] and still contributes a partial body count.  Only
#     files that exited 0 are counted, and the population is printed.
#   * full_language.c needs -I <repo root> and -DCC_NAME=CC_gcc or it stops
#     early on every key.  It is also the one file whose two populations
#     differ: it compiles at plain -O0 and fails to compile the moment
#     MCC_REPLAY_IR=1 meets an armed recorder, so the board prints
#     extra=<in A>/<in B> rather than one flag.
#   * arm-win32 and arm-wince share a define set and must read identically.
#     The "all" driver checks that itself and fails loudly, because it is the
#     cheapest available proof that a run measured what it thinks it did.

set -e

BUILD=$1
KEY=$2
OUT=${3:-/tmp/o0ab}

if [ -z "$BUILD" ] || [ -z "$KEY" ]; then
	echo "usage: $0 <builddir> <key|all> [outdir]" >&2
	exit 2
fi

S=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$(cd "$BUILD" && pwd)
BANKDIR=$S/tests/ast/o0-baseline

LC_ALL=C
export LC_ALL
SOURCE_DATE_EPOCH=1000000000
export SOURCE_DATE_EPOCH

KEYS="x86_64 i386 arm arm64 riscv64 x86_64-win32 i386-win32 arm64-win32 arm-win32 arm-wince x86_64-osx arm64-osx"

if command -v sha256sum > /dev/null 2>&1; then
	sha256() { sha256sum "$1"; }
elif command -v shasum > /dev/null 2>&1; then
	sha256() { shasum -a 256 "$1"; }
elif command -v openssl > /dev/null 2>&1; then
	sha256() { openssl dgst -sha256 -r "$1"; }
else
	echo "o0_ab: no sha256 tool (sha256sum, shasum, openssl)" >&2
	exit 2
fi

mkdir -p "$OUT"

# MCC_OPTD_DEV rows are level rows too -- the class wraps the level rather than
# replacing it -- so they belong in the forced set, and the caller runs with
# MCC_DEV=1 or the driver refuses them. Dropping them silently would shrink the
# forced set to a subset of the banked one and the gated bank would read as a
# second copy of the ungated half.
derive_gates() {
	grep -Eoh 'MCC_OPT_ROW\([A-Z0-9_]+, *"[a-z0-9-]+", *(MCC_OPTD_DEV\()?MCC_OPTD_LEVEL\([0-9]+\)\)' \
		"$S"/src/mccopt.h \
		| sed -E 's/.*"([a-z0-9-]+)".*/-f\1/' | sort -u
}

AOPT=-O0
if [ -n "$O0_AB_MUTATE" ]; then
	AOPT=-O1
	echo "o0_ab: O0_AB_MUTATE -- measurement A taken at -O1, so every banked" \
		"object hash must move; a check that still passes compared nothing" >&2
fi

GATE_FLAGS=
SUF=
if [ -n "$O0_AB_GATES" ]; then
	SUF=.gated
	GATE_FLAGS=$(derive_gates | tr '\n' ' ')
	ngates=$(derive_gates | grep -c . || true)
	if [ "$ngates" -eq 0 ]; then
		echo "o0_ab: derived 0 level knobs from $S/src/mccopt.h -- the" \
			"MCC_OPT_ROW/MCC_OPTD_LEVEL spelling changed, so this run would" \
			"measure -O0 with every pass off and call it coverage" >&2
		exit 1
	fi
	if [ -n "$O0_AB_NOGATES" ]; then
		GATE_FLAGS=
		echo "o0_ab: O0_AB_NOGATES -- the known-positive arm: the $ngates knob(s)" \
			"are derived and then dropped, so measurement B is the ungated one" \
			"under a gated name and every gated bank row must read as vacuous" >&2
	else
		echo "o0_ab: O0_AB_GATES -- $ngates level knob(s) forced on:" \
			"$GATE_FLAGS" >&2
	fi
fi

key_banner() {
	case "$1" in
	x86_64)       echo '(x86_64 Linux)' ;;
	i386)         echo '(i386 Linux)' ;;
	arm64)        echo '(AArch64 Linux)' ;;
	riscv64)      echo '(riscv64 Linux)' ;;
	x86_64-osx)   echo '(x86_64 Darwin)' ;;
	arm64-osx)    echo '(AArch64 Darwin)' ;;
	x86_64-win32) echo '(x86_64 Windows)' ;;
	i386-win32)   echo '(i386 Windows)' ;;
	arm64-win32)  echo '(AArch64 Windows)' ;;
	*)            echo '' ;;
	esac
}

key_is_native() {
	b=$(key_banner "$1")
	[ -n "$b" ] || return 1
	[ -x "$BUILD/mcc" ] || return 1
	"$BUILD/mcc" -v 2>&1 | head -1 | grep -qF "$b"
}

key_flags() {
	SYSROOT=
	case "$1" in
	x86_64)
		MCC=$BUILD/mcc; FLAGS=""
		if ! key_is_native x86_64; then
			MCC=$BUILD/mcc-x86_64
			SYSROOT=$S/vendor/gentoo-stage3-x86_64-glibc
			FLAGS="-B $BUILD -I $S/runtime/include --sysroot=$SYSROOT -I$SYSROOT/usr/include"
		fi ;;
	*win32 | *wince)
		MCC=$BUILD/mcc-$1
		FLAGS="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include" ;;
	*osx)
		MCC=$BUILD/mcc-$1
		FLAGS="-B $S/runtime -I $S/runtime/include"
		if [ ! -x "$MCC" ] && key_is_native "$1"; then
			MCC=$BUILD/mcc; FLAGS="-B $BUILD"
		fi ;;
	arm)
		MCC=$BUILD/mcc-arm
		SYSROOT=$S/vendor/gentoo-stage3-arm-glibc
		FLAGS="-B $BUILD -mfloat-abi hard -I $S/runtime/include --sysroot=$SYSROOT -I$SYSROOT/usr/include" ;;
	*)
		MCC=$BUILD/mcc-$1
		SYSROOT=$S/vendor/gentoo-stage3-$1-glibc
		FLAGS="-B $BUILD -I $S/runtime/include --sysroot=$SYSROOT -I$SYSROOT/usr/include"
		if [ ! -x "$MCC" ] && key_is_native "$1"; then
			MCC=$BUILD/mcc; SYSROOT=; FLAGS=""
		fi ;;
	esac
}

corpus() {
	find tests/exec -name '*.c' | sort
	if [ -z "$C2_NO_EXTRA" ]; then
		echo tests/diff/full_language.c
	fi
}

key_available() {
	key_flags "$1"
	[ -x "$MCC" ] || return 1
	if [ -n "$SYSROOT" ] && [ ! -d "$SYSROOT/usr/include" ]; then
		return 1
	fi
	return 0
}

run_key() {
	k=$1
	key_flags "$k"
	if [ ! -x "$MCC" ]; then
		echo "$k: MISSING $MCC" >&2
		return 1
	fi
	if [ -n "$SYSROOT" ] && [ ! -d "$SYSROOT/usr/include" ]; then
		echo "$k: MISSING sysroot $SYSROOT -- with no system headers this key" \
			"still exits 0 on a fifth of the corpus and reports a plausible" \
			"board, so it is measured as unmeasurable rather than measured" >&2
		return 1
	fi

	objtxt=$OUT/$k$SUF.obj.txt
	rirtxt=$OUT/$k$SUF.rir.txt
	log=$OUT/$k$SUF.rir.log
	: > "$objtxt"
	: > "$log"

	nfile=0
	nok=0
	nobj=0
	extra=0
	rirextra=0

	cd "$S"
	for f in $(corpus); do
		nfile=$((nfile + 1))
		xflags=
		case "$f" in
		*/full_language.c) xflags="-I $S -DCC_NAME=CC_gcc" ;;
		esac

		if "$MCC" -w $AOPT $FLAGS $GATE_FLAGS $xflags -c -o "$OUT/o-$k.o" "$f" \
				> "$OUT/o-$k.err" 2>&1; then
			nobj=$((nobj + 1))
			h=$(sha256 "$OUT/o-$k.o" | cut -d' ' -f1)
			printf '%s\t%s\t%s\n' "$k" "$f" "$h" >> "$objtxt"
			case "$f" in */full_language.c) extra=1 ;; esac
		fi

		if env MCC_REPLAY_IR=1 MCC_FORCE_REPLAY=1 \
				"$MCC" -w -O0 $FLAGS $GATE_FLAGS $xflags -c -o "$OUT/r-$k.o" "$f" \
				> "$OUT/r-$k.err" 2>&1; then
			nok=$((nok + 1))
			case "$f" in */full_language.c) rirextra=1 ;; esac
			echo "### $f" >> "$log"
			grep -E '^\[rir-(total|verify)\]' "$OUT/r-$k.err" >> "$log" || true
		else
			echo "!!! rc!=0 $f" >> "$log"
		fi
	done

	if [ "$nfile" -lt "${O0_AB_MIN_FILES:-64}" ]; then
		echo "$k: corpus is $nfile file(s), floor is ${O0_AB_MIN_FILES:-64} --" \
			"find(1) matched (almost) nothing under tests/exec, so this run" \
			"would bank, or agree with, a baseline of nothing" >&2
		return 1
	fi
	if [ "$nobj" -eq 0 ]; then
		echo "$k: 0 of $nfile corpus files produced an object -- an empty" \
			"$objtxt diffs clean against an empty bank, which is the whole" \
			"failure mode this harness exists to catch" >&2
		return 1
	fi

	if ! grep -q '^\[rir-total\]' "$log"; then
		echo "$k: no [rir-total] in any of $nok successful compiles -- this" \
			"build has no MCC_REPLAY_IR, so a pass here would be vacuous" >&2
		return 1
	fi

	sort -o "$objtxt" "$objtxt"

	awk -v key="$k" -v nfile="$nfile" -v nok="$nok" -v nobj="$nobj" \
		-v extra="$extra" -v rirextra="$rirextra" '
	/^### / { next }
	/^\[rir-total\]/ {
		seen++
		for (i = 1; i <= NF; i++) {
			split($i, kv, "=")
			if (kv[1] in want) tot[kv[1]] += kv[2]
		}
		next
	}
	/^\[rir-verify\]/ {
		v = $2
		sub(/:.*$/, "", v)
		vd[v]++
		next
	}
	BEGIN {
		split("fn faithful unbal ovf fallbackfn", a, " ")
		for (i in a) want[a[i]] = 1
	}
	END {
		printf "key=%s\n", key
		printf "opt=-O0\n"
		printf "files=%d\n", nfile
		printf "objects=%d\n", nobj
		printf "rirok=%d\n", nok
		printf "rirfiles=%d\n", seen
		printf "extra=%d\n", extra
		printf "rirextra=%d\n", rirextra
		printf "fn=%d\n", tot["fn"]
		printf "faithful=%d\n", tot["faithful"]
		printf "empty=%d\n", vd["rempty"]
		printf "unfaithful=%d\n", vd["runfaithful"]
		printf "diverge=%d\n", vd["rdiverge"]
		printf "rewind=%d\n", vd["rrewind"]
		printf "error=%d\n", vd["rerror"]
		printf "unbal=%d\n", tot["unbal"]
		printf "ovf=%d\n", tot["ovf"]
		printf "accounted=%d\n", tot["faithful"] + vd["rempty"]
		printf "bar=%s\n", (tot["faithful"] + vd["rempty"] == tot["fn"]) ? "OK" : "FAIL"
	}
	' "$log" > "$rirtxt"

	if [ -n "$SUF" ]; then
		if [ ! -f "$BANKDIR/$k.obj.txt" ]; then
			echo "$k: no $BANKDIR/$k.obj.txt to compare against -- the gated run" \
				"cannot show that forcing the knobs on changed anything, and mcc" \
				"ignores an unknown -f silently, so bank the ungated board first" >&2
			return 1
		fi
		if cmp -s "$BANKDIR/$k.obj.txt" "$objtxt"; then
			echo "$k: the gated objects are identical to the ungated bank's." \
				"$ngates knob(s) were passed as -f and not one byte of one object" \
				"moved, which is what a silently-ignored flag name looks like." \
				"Either the derivation in derive_gates no longer names anything" \
				"mcc parses, or this half of the bank is a second copy of the" \
				"other half and should be retired rather than re-banked." >&2
			return 1
		fi
	fi

	if [ -n "$O0_AB_BANK" ]; then
		mkdir -p "$BANKDIR"
		if [ -z "$SUF" ]; then
			cp "$objtxt" "$BANKDIR/$k.obj.txt"
		fi
		cp "$rirtxt" "$BANKDIR/$k$SUF.rir.txt"
	fi

	awk -F= -v key="$k" -v cc="$MCC" '
	{ v[$1] = $2 }
	END {
		printf "%-14s -O0 cc=%s files=%s objects=%s rirok=%s extra=%s/%s rirfiles=%s fn=%s faithful=%s empty=%s unfaithful=%s diverge=%s rewind=%s error=%s unbal=%s ovf=%s %s\n",
			key, cc, v["files"], v["objects"], v["rirok"], v["extra"], v["rirextra"], v["rirfiles"],
			v["fn"], v["faithful"], v["empty"], v["unfaithful"], v["diverge"],
			v["rewind"], v["error"], v["unbal"], v["ovf"], v["bar"]
	}
	' "$rirtxt"

	if [ -n "$O0_AB_CHECK" ]; then
		drift=0
		if [ -z "$SUF" ] && ! diff -u "$BANKDIR/$k.obj.txt" "$objtxt" >&2; then
			echo "o0_ab: $k -- an -O0 object moved. The AST recorder does not" \
				"run at -O0, so nothing in the cut had any business touching" \
				"these bytes." >&2
			drift=1
		fi
		if ! diff -u "$BANKDIR/$k$SUF.rir.txt" "$rirtxt" >&2; then
			echo "o0_ab: $k -- forced-O0 Replay_IR coverage moved." >&2
			drift=1
		fi
		[ "$drift" -eq 0 ] || return 1
	fi
}

twin_check() {
	a=$OUT/arm-win32$SUF.rir.txt
	b=$OUT/arm-wince$SUF.rir.txt
	if [ ! -f "$a" ] || [ ! -f "$b" ]; then
		echo "o0_ab: arm-win32/arm-wince twin check SKIPPED (a row is missing)" >&2
		return 0
	fi
	if grep -v '^key=' "$a" > "$OUT/twin.a" \
			&& grep -v '^key=' "$b" > "$OUT/twin.b" \
			&& cmp -s "$OUT/twin.a" "$OUT/twin.b"; then
		:
	else
		echo "o0_ab: FAIL -- arm-win32 and arm-wince share a define set and must" \
			"read identically, but their forced-O0 counters differ:" >&2
		diff -u "$OUT/twin.a" "$OUT/twin.b" >&2 || true
		return 1
	fi
	cut -f2,3 "$OUT/arm-win32$SUF.obj.txt" > "$OUT/twin.oa"
	cut -f2,3 "$OUT/arm-wince$SUF.obj.txt" > "$OUT/twin.ob"
	if cmp -s "$OUT/twin.oa" "$OUT/twin.ob"; then
		echo "o0_ab: arm-win32 == arm-wince (counters and object sha256 both)"
	else
		echo "o0_ab: arm-win32 == arm-wince (counters); object sha256 differ on" \
			"$(diff "$OUT/twin.oa" "$OUT/twin.ob" | grep -c '^<' || true) file(s)"
	fi
}

RUNKEYS=$KEYS
SKIPPED=
case "$KEY" in
all)
	;;
measurable)
	RUNKEYS=
	for k in $KEYS; do
		if key_available "$k"; then
			RUNKEYS="$RUNKEYS $k"
		else
			SKIPPED="$SKIPPED $k"
		fi
	done ;;
*)
	run_key "$KEY"
	exit $? ;;
esac

nkeys=$(echo $KEYS | wc -w | tr -d ' ')
nrun=$(echo $RUNKEYS | wc -w | tr -d ' ')
minkeys=${O0_AB_MIN_KEYS:-1}
if [ -n "$O0_AB_BANK" ] && [ "$KEY" = "measurable" ]; then
	echo "o0_ab: FAIL -- refusing to bank from 'measurable'. The board is a" \
		"twelve-row artefact and 'all' is the only spelling that demands all" \
		"twelve; banking whichever rows this host happened to reach would" \
		"freeze a hole into the baseline. Use 'all'." >&2
	exit 1
fi
if [ "$KEY" = "measurable" ]; then
	echo "o0_ab: measurable -- $nrun/$nkeys key(s), floor $minkeys;" \
		"unmeasurable:${SKIPPED:- (none)}" >&2
	if [ "$nrun" -lt "$minkeys" ]; then
		echo "o0_ab: FAIL -- $nrun measurable key(s) is below the floor of" \
			"$minkeys. Every remaining key is missing its compiler or its" \
			"sysroot, so this run has no subject and must not read as a pass." >&2
		exit 1
	fi
fi

board=$OUT/board$SUF.txt
: > "$board"
rc=0
for k in $RUNKEYS; do
	if run_key "$k" >> "$board"; then
		:
	else
		rc=1
		grep -q "^$k " "$board" || echo "$k NOT MEASURED" >> "$board"
	fi
done
cat "$board"
if grep -q ' FAIL$' "$board"; then
	rc=1
fi
twin_check || rc=1
if [ -n "$O0_AB_BANK" ]; then
	mkdir -p "$BANKDIR"
	cp "$board" "$BANKDIR/board$SUF.txt"
fi
if [ -n "$O0_AB_CHECK" ] && [ "$KEY" = "all" ] \
		&& ! diff -u "$BANKDIR/board$SUF.txt" "$board" >&2; then
	echo "o0_ab: the twelve-key board moved." >&2
	rc=1
fi
exit $rc
