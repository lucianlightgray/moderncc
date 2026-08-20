#!/bin/sh
# T-mac-30074: a lone \r (classic-Mac line ending) must terminate a line, so a
# #define and a // comment after it are recognized. Generated at runtime because
# .gitattributes normalizes stored \r to \n.
set -e
MCC="$1"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

printf 'int glob = 1;\r#define X 41\r// comment to end of line\rint main(void){ return (X == 41 && glob == 1) ? 0 : 1; }\n' > "$tmp/cr.c"
"$MCC" "$tmp/cr.c" -o "$tmp/cr"
"$tmp/cr"

# CRLF and LF endings must be unaffected (no double-counted lines / miscompiles).
printf 'int f(void){return 0;}\r\n#define Y 7\r\nint g(void){return Y;}\r\n' > "$tmp/crlf.c"
"$MCC" -c "$tmp/crlf.c" -o "$tmp/crlf.o"
printf 'int h(void){return 0;}\n#define Z 9\nint k(void){return Z;}\n' > "$tmp/lf.c"
"$MCC" -c "$tmp/lf.c" -o "$tmp/lf.o"
echo OK
