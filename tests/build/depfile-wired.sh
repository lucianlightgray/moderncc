#!/bin/sh
# T-lin-10068: in an mcc-profile (self-host / stage-2) build, CMake must wire mcc's
# gcc-style depfile into the Ninja C compile rule -- both emitted (-MF $DEP_FILE) and
# consumed (deps = gcc) -- or the object never rebuilds when an included header changes
# (a silent-stale build; every measurement taken from it is suspect). This asserts the
# wiring is present in the build that invoked the test. REF docs/DETAILS.md#t-lin-10068.
set -eu

bdir="${1:?usage: depfile-wired.sh <build-dir>}"

found=0
for f in "$bdir/CMakeFiles/rules.ninja" "$bdir/build.ninja"; do
	[ -f "$f" ] || continue
	if grep -qE 'deps = gcc' "$f" && grep -qE -- '-MF \$DEP_FILE' "$f"; then
		found=1
		break
	fi
done

if [ "$found" -eq 1 ]; then
	echo "OK: mcc build wires the depfile (deps=gcc + -MF \$DEP_FILE); objects rebuild on header change"
	exit 0
fi

echo "FAIL: no depfile-wired C compile rule in $bdir"
echo "  T-lin-10068 regressed: the mcc/TinyCC depfile format+flags are not set, so a"
echo "  stage-2 build silently does not rebuild when a header changes."
exit 1
