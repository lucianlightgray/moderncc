#!/bin/sh
# run-tier.sh drives tests/run with no -O flag at all, so the whole -run corpus
# only ever ran at -O0 -- and at -O0 the AST replay path is off entirely, which
# left every arena consumer reached only by -run untested. A segfault compiling
# an operand-carrying inline asm with `-O2 -run` passed a full 8600-cell ctest
# twice because of it. This cell sweeps the same corpus over every -O level and
# both JIT settings on the host compiler, where it costs seconds.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
mcc=${1:-$root/cmake-release/mcc}
bdir=${2:-$root/cmake-release}
corpus="$root/tests/run"

[ -x "$mcc" ] || { echo "SKIP: no mcc at $mcc"; exit 77; }
[ -d "$corpus" ] || { echo "SKIP: no corpus directory at $corpus"; exit 77; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# <program>:<opt>:<jit> cells whose failure is a documented defect that predates
# this cell, XFAILed instead of failing it. tls_threads at -O1 and above with
# MCC_JIT=1 reports child_init=0 and child_aux=0 where the thread should see 42
# and 11: a TLS block created for a new thread does not get its initialiser on
# the JIT path once replay is on. Verified identical on the parent commit, so it
# is not this work. A listed cell that starts passing fails the run, so this
# cannot rot into lost coverage.
KNOWN_RED="tls_threads:-O1:1 tls_threads:-O2:1 tls_threads:-O3:1"

is_known_red() {
	for kr in $KNOWN_RED; do
		[ "$kr" = "$1:$2:$3" ] && return 0
	done
	return 1
}

pass=0
bad=0
xfail=0
stale=0

for src in "$corpus"/*.c; do
	case "$src" in *.aux.c) continue ;; esac
	name=$(basename "$src" .c)
	exp="$corpus/$name.expected"
	[ -f "$exp" ] || { echo "FAIL: $name has no .expected"; bad=$((bad + 1)); continue; }
	aux=""
	[ -f "$corpus/$name.aux.c" ] && aux="$corpus/$name.aux.c"
	args=""
	[ -f "$corpus/$name.args" ] && args=$(cat "$corpus/$name.args")
	want=$(tr -d '\r' <"$exp")

	for opt in -O0 -O1 -O2 -O3; do
		for jit in 0 1; do
			set +e
			MCC_JIT=$jit "$mcc" -w "-B$bdir" "-L$bdir" \
				-I"$root/runtime/include" $opt $aux -run "$src" $args \
				>"$work/out" 2>"$work/err"
			rc=$?
			set -e
			got=$(tr -d '\r' <"$work/out")
			ok=1
			[ "$rc" = 0 ] || ok=0
			[ "$got" = "$want" ] || ok=0
			if is_known_red "$name" "$opt" "$jit"; then
				if [ "$ok" = 1 ]; then
					echo "FAIL: $name $opt MCC_JIT=$jit is listed KNOWN_RED but passed -- delist it"
					stale=$((stale + 1))
				else
					echo "XFAIL: $name $opt MCC_JIT=$jit (known defect)"
					xfail=$((xfail + 1))
				fi
				continue
			fi
			if [ "$rc" != 0 ]; then
				tail -20 "$work/err" | sed -e "s/^/  $name $opt JIT=$jit stderr: /"
				echo "FAIL: $name $opt MCC_JIT=$jit exited $rc"
				bad=$((bad + 1))
				continue
			fi
			if [ "$got" != "$want" ]; then
				echo "FAIL: $name $opt MCC_JIT=$jit output differs"
				printf '%s\n' "$want" | sed -e "s/^/  want: /"
				printf '%s\n' "$got" | sed -e "s/^/  got : /"
				bad=$((bad + 1))
				continue
			fi
			pass=$((pass + 1))
		done
	done
done

echo "run-opt: $pass ok, $bad bad, $xfail xfail, $stale stale"
[ "$bad" = 0 ] && [ "$stale" = 0 ] || exit 1
exit 0
