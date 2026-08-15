#!/bin/sh
set -eu

mcc=${1:?usage: headers-parse.sh <mcc> <srcdir> --min-files <n>}
srcdir=${2:?}
shift 2

minfiles=0
while [ $# -gt 0 ]; do
	case $1 in
	--min-files)
		minfiles=${2:?--min-files needs a value}
		shift 2
		;;
	--min-files=*)
		minfiles=${1#--min-files=}
		shift
		;;
	*)
		echo "headers-parse: unknown argument $1" >&2
		exit 2
		;;
	esac
done

if [ "$minfiles" -lt 1 ]; then
	echo "headers-parse: --min-files must be at least 1; a floor of $minfiles" >&2
	echo "headers-parse: cannot reject an empty subject" >&2
	exit 2
fi

osxinc="$srcdir/runtime/osx/include"
freeinc="$srcdir/runtime/include"

if [ ! -d "$osxinc" ]; then
	echo "headers-parse: $osxinc absent" >&2
	exit 1
fi

covered='stdio|string|stdlib|math|assert|errno|ctype'
ok=0
bad=0
missing=0
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

for f in $(find "$srcdir/tests/exec" -name '*.c' | sort); do
	grep -qE "#include <($covered)\.h>" "$f" || continue
	grep -qE '#include <(threads|fenv|signal|unistd|windows|sys/)' "$f" && continue
	grep -qE '#include "' "$f" && continue
	if "$mcc" -c -nostdinc -I"$osxinc" -I"$freeinc" -o /dev/null "$f" 2>"$tmp"; then
		ok=$((ok + 1))
	elif grep -q "include file .* not found" "$tmp"; then
		missing=$((missing + 1))
		echo "headers-parse: MISSING HEADER for $f"
		grep "include file .* not found" "$tmp" | head -1
	else
		bad=$((bad + 1))
	fi
done

echo "headers-parse: $ok compiled, $missing blocked on a missing header, $bad failed for other reasons"

if [ "$missing" -gt 0 ]; then
	echo "headers-parse: a covered corpus file wants a header runtime/osx/include does not ship" >&2
	exit 1
fi

if [ "$ok" -lt "$minfiles" ]; then
	echo "headers-parse: only $ok file(s) compiled and at least $minfiles were expected;" >&2
	echo "headers-parse: a shrinking subject reads as coverage it does not have" >&2
	exit 1
fi

exit 0
