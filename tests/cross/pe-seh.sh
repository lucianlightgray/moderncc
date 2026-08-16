#!/bin/sh
# pe/seh (W6): __try/__except structured exception handling on x86_64 Windows.
# mcc must catch a null-store access violation and an integer divide-by-zero via
# a constant __except(1) filter, matching MSVC cl, on both the full-link and the
# COFF-object (mingw-linked) paths.
#
# usage: pe-seh.sh <mcc> <gcc> <src> <work>
set -u
MCC="$1"; GCC="$2"; SRC="$3"; WORK="$4"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then echo "pe-seh: no mcc: $MCC" >&2; exit 77; fi

rm -rf "$WORK"; mkdir -p "$WORK"
want="av=7 ok=100 x=42 div=9 divok=5 nc=55 ns=77 lo=88 lns=99 fin=5 finr=1 fu=66 fur=1 ret=32 retr=1 brk=303 brkr=4 cont=30 contr=5 gt=2 gtr=1 nst=8 nstr=12"
fail=0

run_expect() {
	tag="$1"; exe="$2"
	got=$("$exe" 2>/dev/null); rc=$?
	if [ "$got" = "$want" ] && [ "$rc" = "0" ]; then
		echo "$tag: '$got' (ok)"
	else
		echo "pe-seh: $tag produced '$got' rc=$rc, expected '$want' rc=0" >&2
		fail=1
	fi
}

# 1) mcc full internal link
if "$MCC" "$SRC" -o "$WORK/seh_m.exe" 2>"$WORK/m.log"; then
	run_expect "mcc-fulllink" "$WORK/seh_m.exe"
else
	echo "pe-seh: mcc full-link build failed" >&2; cat "$WORK/m.log" >&2; fail=1
fi

# 2) mcc COFF object -> mingw link (interop path)
if [ -f "$GCC" ]; then
	if "$MCC" -c -Wl,-oformat=coff "$SRC" -o "$WORK/seh_c.o" 2>"$WORK/c.log" &&
			"$GCC" "$WORK/seh_c.o" -o "$WORK/seh_c.exe" 2>>"$WORK/c.log"; then
		run_expect "mcc-coff+mingw" "$WORK/seh_c.exe"
	else
		echo "pe-seh: mcc COFF -> mingw link failed" >&2; cat "$WORK/c.log" >&2; fail=1
	fi
fi

# 3) MSVC cl cross-check, if cl is on PATH (e.g. a vcvars environment)
if command -v cl >/dev/null 2>&1; then
	if cl -nologo -Fe:"$WORK/seh_cl.exe" -Fo:"$WORK/seh_cl.obj" "$SRC" >/dev/null 2>&1; then
		run_expect "msvc-cl" "$WORK/seh_cl.exe"
	fi
fi

if [ "$fail" != "0" ]; then exit 1; fi
echo "pe-seh: OK (__try/__except catches AV + int div-by-zero, matches MSVC)"
exit 0
