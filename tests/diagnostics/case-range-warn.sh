#!/bin/sh
set -u

MCC="$1"
WORK="$2"

mkdir -p "$WORK" 2>/dev/null || exit 2
cd "$WORK" || exit 2
[ -x "$MCC" ] || exit 77

echo 'int f(char x){switch(x){case 256: return 1; default: return 0;}}' > narrow.c
echo 'int g(unsigned char y){switch(y){case -1: return 1; default: return 0;}}' > uneg.c
echo 'int h(int x){switch(x){case 256: return 1; default: return 0;}}' > wide.c
echo 'int i(char x){switch(x){case 127: return 1; default: return 0;}}' > inrange.c

out=$("$MCC" -c narrow.c -o narrow.o 2>&1)
case "$out" in
	*"out of range"*) : ;;
	*) echo "FAIL: char case 256 did not warn out-of-range: $out"; exit 1 ;;
esac

out=$("$MCC" -c uneg.c -o uneg.o 2>&1)
case "$out" in
	*"out of range"*) : ;;
	*) echo "FAIL: unsigned char case -1 did not warn out-of-range: $out"; exit 1 ;;
esac

out=$("$MCC" -c wide.c -o wide.o 2>&1)
case "$out" in
	*"out of range"*) echo "FAIL: int case 256 wrongly warned out-of-range: $out"; exit 1 ;;
esac

out=$("$MCC" -c inrange.c -o inrange.o 2>&1)
case "$out" in
	*"out of range"*) echo "FAIL: char case 127 (in range) wrongly warned: $out"; exit 1 ;;
esac

out=$("$MCC" -w -c narrow.c -o narrow.o 2>&1)
case "$out" in
	*"out of range"*) echo "FAIL: -w did not suppress the warning: $out"; exit 1 ;;
esac

echo "PASS: narrow-type out-of-range case warns (char/short, signed+unsigned); int-width and in-range do not; -w suppresses"
exit 0
