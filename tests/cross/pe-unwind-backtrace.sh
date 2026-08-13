#!/bin/sh
set -eu

MCC="$1"
GCC="$2"
SRC="$3"
WORK="$4"
OBJDUMP="${5:-}"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then
	echo "pe-unwind-backtrace: no mcc: $MCC" >&2
	exit 77
fi
if [ ! -f "$GCC" ]; then
	echo "pe-unwind-backtrace: mingw gcc not found: $GCC (skipping)" >&2
	exit 77
fi

rm -rf "$WORK"
mkdir -p "$WORK"

fail=0

# Structural W4 check: each function must get its OWN UNWIND_INFO record in
# .xdata (per-function), not a single shared blob. The subject has 5 functions,
# so .pdata is 5 RUNTIME_FUNCTIONs (12 bytes each) and .xdata is >= 5 records.
if [ -n "$OBJDUMP" ] && [ -f "$OBJDUMP" ]; then
	if "$MCC" -c -Wl,-oformat=coff "$SRC" -o "$WORK/uw.o" 2>"$WORK/objbuild.log"; then
		psz=$("$OBJDUMP" -h "$WORK/uw.o" 2>/dev/null | awk '$2==".pdata"{print strtonum("0x"$3)}')
		xsz=$("$OBJDUMP" -h "$WORK/uw.o" 2>/dev/null | awk '$2==".xdata"{print strtonum("0x"$3)}')
		nfn=$(( ${psz:-0} / 12 ))
		echo "structural: .pdata=${psz:-0}B (${nfn} funcs) .xdata=${xsz:-0}B"
		if [ "${nfn}" -lt 2 ] || [ "${xsz:-0}" -lt $(( nfn * 8 )) ]; then
			echo "pe-unwind-backtrace: .xdata (${xsz:-0}B) is smaller than one UNWIND_INFO per function (${nfn}); regressed to a shared blob" >&2
			fail=1
		fi
	fi
fi

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
echo "pe-unwind-backtrace: OK (per-function UNWIND_INFO; RtlCaptureStackBackTrace reaches every mcc frame incl. __chkstk, -O0/-O2/-O3)"
exit 0
