#!/bin/sh
set -e
repro=$1
preset=${2:-debug}
[ -n "$repro" ] && [ -f "$repro" ] || {
	echo "git-bisect predicate for a tests/fuzz repro (0=good,1=bad,125=skip)"
	echo "usage: git bisect run $0 <repro.c> [preset]"
	exit 125
}

root=$(git rev-parse --show-toplevel)
bdir="$root/cmake-$preset"
cmake --build --preset "$preset" -j >/dev/null 2>&1 || exit 125

gcc=$(command -v gcc || true)
clang=$(command -v clang || true)
[ -n "$gcc" ] && [ -n "$clang" ] || exit 125

work=$(mktemp -d)
one="$work/corpus"
mkdir -p "$one"
cp "$repro" "$one/"
trap 'rm -rf "$work"' EXIT
"$bdir/fuzz_runner" "$bdir/mcc" "$bdir" "$root/runtime/include" \
    "$work" "$gcc" "$clang" --corpus "$one" --replay
