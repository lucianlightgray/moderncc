#!/bin/sh
set -eu

mcc=${1:?usage: vector-abi-layout.sh <mcc> <ref-cc> <srcdir> <workdir>}
ref=${2:?}
srcdir=${3:?}
work=${4:?}

src="$srcdir/tests/cross/vector-abi-layout.c"
mkdir -p "$work"

# VECABI_MUTATE=1 is the known-positive arm: it packs the probe's structs on the
# mcc side only, so the two compilers must disagree, and the cell then requires
# the disagreement it would otherwise report as a failure. It perturbs the
# subject rather than the verdict, so it exercises both compile arms, both runs,
# the both-rows guard and the diff -- a green mutate arm means the OK path is
# comparing something.
mutate=${VECABI_MUTATE:-0}
mccdefs=""
if [ "$mutate" != "0" ]; then
	mccdefs="-DVECABI_MUTATE"
fi

# Compare LAYOUT, not a cross-link. Linking an mcc object against a reference
# object is infeasible on Darwin -- mcc emits ELF there and the host linker
# wants Mach-O, so a link-based cell would skip on exactly the target whose
# verdict is "this one needed no change". Each compiler builds the whole probe
# and prints its own offsetof/sizeof, which are compile-time constants baked by
# whichever compiler built it, so the diff is a pure ABI-layout comparison.
if ! "$mcc" -O0 $mccdefs -o "$work/layout-mcc" "$src" 2>"$work/mcc.err"; then
	echo "vector-abi-layout: mcc failed to build the probe" >&2
	cat "$work/mcc.err" >&2
	exit 1
fi
if ! "$ref" -O0 -o "$work/layout-ref" "$src" 2>"$work/ref.err"; then
	echo "vector-abi-layout: reference cc failed to build the probe" >&2
	cat "$work/ref.err" >&2
	exit 1
fi

"$work/layout-mcc" > "$work/mcc.out"
"$work/layout-ref" > "$work/ref.out"

# A comparison of two empty outputs is not a passing comparison. Both probes
# must have produced both rows, or the cell reports agreement it never saw.
for f in "$work/mcc.out" "$work/ref.out"; do
	if [ "$(grep -c '^v[0-9]* [0-9]* [0-9]*$' "$f")" -ne 2 ]; then
		echo "vector-abi-layout: $f does not carry both measurement rows;" >&2
		echo "vector-abi-layout: an empty subject cannot report agreement" >&2
		cat "$f" >&2
		exit 1
	fi
done

echo "vector-abi-layout: mcc  $(tr '\n' ' ' < "$work/mcc.out")"
echo "vector-abi-layout: ref  $(tr '\n' ' ' < "$work/ref.out")  ($ref)"

if diff -u "$work/ref.out" "$work/mcc.out" > "$work/diff.txt"; then
	agree=1
else
	agree=0
fi

if [ "$mutate" != "0" ]; then
	if [ "$agree" = "1" ]; then
		echo "vector-abi-layout: MUTATE -- the packed probe still agrees with the" >&2
		echo "vector-abi-layout: reference, so this cell cannot tell layouts apart" >&2
		echo "vector-abi-layout: and its green arm proves nothing" >&2
		exit 1
	fi
	echo "vector-abi-layout: MUTATE OK -- the packed probe was caught"
	exit 0
fi

if [ "$agree" = "0" ]; then
	echo "vector-abi-layout: mcc and the reference disagree on vector struct layout" >&2
	echo "vector-abi-layout: this is a cross-TU ABI incompatibility, not a" >&2
	echo "vector-abi-layout: reported-alignment difference -- offsetof and sizeof" >&2
	echo "vector-abi-layout: are what an object file commits to" >&2
	cat "$work/diff.txt" >&2
	exit 1
fi

echo "vector-abi-layout: OK -- mcc and $ref lay 32- and 64-byte vector members identically"
exit 0
