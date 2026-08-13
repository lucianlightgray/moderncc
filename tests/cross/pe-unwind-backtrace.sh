#!/bin/sh
set -eu

MCC="$1"
GCC="$2"
SRC="$3"
WORK="$4"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then
	echo "pe-unwind-backtrace: mcc not found: $MCC" >&2
	exit 77
fi
if [ ! -f "$GCC" ]; then
	echo "pe-unwind-backtrace: mingw gcc not found: $GCC (skipping)" >&2
	exit 77
fi

rm -rf "$WORK"
mkdir -p "$WORK"

fail=0

"$GCC" "$SRC" -o "$WORK/bt_g.exe"
gout=$("$WORK/bt_g.exe") || true
echo "mingw: $gout"
case "$gout" in
	*"ok=1"*) : ;;
	*) echo "pe-unwind-backtrace: mingw reference did not reach all frames" >&2; fail=1 ;;
esac

for opt in -O0 -O2 -O3; do
	if ! "$MCC" $opt -lntdll "$SRC" -o "$WORK/bt_m$opt.exe" 2>"$WORK/build$opt.log"; then
		echo "pe-unwind-backtrace: mcc $opt build failed" >&2
		cat "$WORK/build$opt.log" >&2
		fail=1
		continue
	fi
	mout=$("$WORK/bt_m$opt.exe") || true
	echo "mcc $opt: $mout"
	case "$mout" in
		*"ok=1"*) : ;;
		*) echo "pe-unwind-backtrace: mcc $opt did not reach all frames through RtlVirtualUnwind" >&2; fail=1 ;;
	esac
done

if [ "$fail" != "0" ]; then
	exit 1
fi
echo "pe-unwind-backtrace: OK (RtlCaptureStackBackTrace reaches every mcc frame incl. __chkstk, -O0/-O2/-O3)"
exit 0
