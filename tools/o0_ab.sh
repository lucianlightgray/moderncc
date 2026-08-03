#!/bin/sh
#
# o0_ab.sh -- the -O0 A/B harness for the Cut to Replay_IR plan.
#
# Two independent measurements per target key, both at -O0, both banked:
#
#   A. Object identity.  Plain -O0 with no MCC_* in the environment.  The AST
#      recorder does not run at -O0 (ast_replay_env needs optimize >= 1,
#      src/mccast.c:2035), so no phase of the cut may move a single one of
#      these bytes.  Recorded as a sorted, diffable per-file sha256.
#
#   B. Forced Replay_IR coverage.  MCC_REPLAY_IR=1 MCC_FORCE_REPLAY=1 at -O0
#      arms Replay_IR over every body independently of the recorder.  The bar
#      is faithful + empty == fn.
#
# Usage:
#   tools/o0_ab.sh <builddir> <key> [outdir]      one key
#   tools/o0_ab.sh <builddir> all   [outdir]      all twelve keys + gates
#
# Keys: x86_64 i386 arm arm64 riscv64
#       x86_64-win32 i386-win32 arm64-win32 arm-win32 arm-wince
#       x86_64-osx arm64-osx
#
# Environment:
#   C2_NO_EXTRA=1   drop tests/diff/full_language.c, so a board is like-for-like
#                   with a c2_sweep.sh board taken the same way.
#   O0_AB_GATES=1   additionally force the 38 "o4 || s1->optimize >= 1" AST
#                   gates on for measurement B, the way
#                   tests/ast/rir_parity.cmake FORCE does.  The gate names are
#                   derived by regex over src/*.c per run, never remembered;
#                   deriving zero of them is fatal.
#   O0_AB_BANK=1    write the result into tests/ast/o0-baseline/ as well as
#                   into <outdir>.
#   O0_AB_CHECK=1   diff the result against tests/ast/o0-baseline/ and fail on
#                   any drift.  This is the gate every phase of the cut has to
#                   pass.
#
# Regenerate the banked baseline (from the repo root, after a
#   cmake -S . -B b -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMCC_ENABLE_CROSS=ON
#   ninja -C b
# build).  Both boards are banked; the second is what
# tests/ast/rir_parity.cmake's FORCE mode measures:
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
#   <key>.gated.rir.txt  same, with the 38 gates forced on
#   board.txt            one summary line per key, written by "all"
#   board.gated.txt      same, with the 38 gates forced on
#
# Measurement A does not depend on O0_AB_GATES -- it runs with an empty
# MCC_* environment either way -- so only measurement B is banked twice.
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

derive_gates() {
	grep -Eoh 'ast_env_gate\("MCC_AST_[A-Z0-9_]+", *o4 \|\| s1->optimize >= 1\)' \
		"$S"/src/*.c \
		| sed -E 's/.*"(MCC_AST_[A-Z0-9_]+)".*/\1/' | sort -u
}

GATE_ENV=
SUF=
if [ -n "$O0_AB_GATES" ]; then
	SUF=.gated
	gates=$(derive_gates)
	ngates=$(echo "$gates" | grep -c . || true)
	if [ "$ngates" -eq 0 ]; then
		echo "o0_ab: derived 0 optimize>=1 gates from $S/src -- the" \
			"ast_env_gate spelling changed, so this run would measure -O0 with" \
			"every pass off and call it coverage" >&2
		exit 1
	fi
	for g in $gates; do
		GATE_ENV="$GATE_ENV $g=1"
	done
	echo "o0_ab: O0_AB_GATES -- $ngates optimize>=1 gate(s) forced on" >&2
fi

key_flags() {
	SYSROOT=
	case "$1" in
	x86_64) MCC=$BUILD/mcc; FLAGS="" ;;
	*win32 | *wince)
		MCC=$BUILD/mcc-$1
		FLAGS="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include" ;;
	*osx)
		MCC=$BUILD/mcc-$1
		FLAGS="-B $S/runtime -I $S/runtime/include" ;;
	arm)
		MCC=$BUILD/mcc-arm
		SYSROOT=$S/vendor/gentoo-stage3-arm-glibc
		FLAGS="-B $BUILD -mfloat-abi hard -I $S/runtime/include --sysroot=$SYSROOT -I$SYSROOT/usr/include" ;;
	*)
		MCC=$BUILD/mcc-$1
		SYSROOT=$S/vendor/gentoo-stage3-$1-glibc
		FLAGS="-B $BUILD -I $S/runtime/include --sysroot=$SYSROOT -I$SYSROOT/usr/include" ;;
	esac
}

corpus() {
	find tests/exec -name '*.c' | sort
	if [ -z "$C2_NO_EXTRA" ]; then
		echo tests/diff/full_language.c
	fi
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

	objtxt=$OUT/$k.obj.txt
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

		if "$MCC" -w -O0 $FLAGS $xflags -c -o "$OUT/o-$k.o" "$f" \
				> "$OUT/o-$k.err" 2>&1; then
			nobj=$((nobj + 1))
			h=$(sha256 "$OUT/o-$k.o" | cut -d' ' -f1)
			printf '%s\t%s\t%s\n' "$k" "$f" "$h" >> "$objtxt"
			case "$f" in */full_language.c) extra=1 ;; esac
		fi

		if env MCC_REPLAY_IR=1 MCC_FORCE_REPLAY=1 $GATE_ENV \
				"$MCC" -w -O0 $FLAGS $xflags -c -o "$OUT/r-$k.o" "$f" \
				> "$OUT/r-$k.err" 2>&1; then
			nok=$((nok + 1))
			case "$f" in */full_language.c) rirextra=1 ;; esac
			echo "### $f" >> "$log"
			grep -E '^\[rir-(total|verify)\]' "$OUT/r-$k.err" >> "$log" || true
		else
			echo "!!! rc!=0 $f" >> "$log"
		fi
	done

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

	if [ -n "$O0_AB_BANK" ]; then
		mkdir -p "$BANKDIR"
		if [ -z "$SUF" ]; then
			cp "$objtxt" "$BANKDIR/$k.obj.txt"
		fi
		cp "$rirtxt" "$BANKDIR/$k$SUF.rir.txt"
	fi

	awk -F= -v key="$k" '
	{ v[$1] = $2 }
	END {
		printf "%-14s -O0 files=%s objects=%s rirok=%s extra=%s/%s rirfiles=%s fn=%s faithful=%s empty=%s unfaithful=%s diverge=%s rewind=%s error=%s unbal=%s ovf=%s %s\n",
			key, v["files"], v["objects"], v["rirok"], v["extra"], v["rirextra"], v["rirfiles"],
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
	cut -f2,3 "$OUT/arm-win32.obj.txt" > "$OUT/twin.oa"
	cut -f2,3 "$OUT/arm-wince.obj.txt" > "$OUT/twin.ob"
	if cmp -s "$OUT/twin.oa" "$OUT/twin.ob"; then
		echo "o0_ab: arm-win32 == arm-wince (counters and object sha256 both)"
	else
		echo "o0_ab: arm-win32 == arm-wince (counters); object sha256 differ on" \
			"$(diff "$OUT/twin.oa" "$OUT/twin.ob" | grep -c '^<' || true) file(s)"
	fi
}

if [ "$KEY" != "all" ]; then
	run_key "$KEY"
	exit $?
fi

board=$OUT/board$SUF.txt
: > "$board"
rc=0
for k in $KEYS; do
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
exit $rc
