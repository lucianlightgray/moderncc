#!/bin/sh
set -u

MCC="$1"
WORK="$2"

mkdir -p "$WORK" 2>/dev/null || exit 2
cd "$WORK" || exit 2
[ -x "$MCC" ] || exit 77

fail() { echo "FAIL: $1" >&2; exit 1; }

cat > bad.c <<'EOF'
void mylog(const char *fmt, ...) __attribute__((format(printf,1,2)));
int main(void) { mylog("%d", 3.0); return 0; }
EOF

out=$("$MCC" -Wformat -c bad.c -o bad.o 2>&1)
echo "$out" | grep -q "format '%d' expects an integer argument" \
	|| fail "attributed user fn with mismatched arg should warn under -Wformat; got: $out"

out=$("$MCC" -c bad.c -o bad.o 2>&1)
echo "$out" | grep -q "expects an integer argument" \
	&& fail "without -Wformat the format check must stay silent; got: $out"

cat > good.c <<'EOF'
void mylog(const char *fmt, ...) __attribute__((format(printf,1,2)));
int main(void) { mylog("%d", 3); return 0; }
EOF

out=$("$MCC" -Wformat -c good.c -o good.o 2>&1)
echo "$out" | grep -q "expects an integer argument" \
	&& fail "correct usage must not warn; got: $out"

cat > offset.c <<'EOF'
int myfprintf(void *stream, const char *fmt, ...) __attribute__((format(printf,2,3)));
int main(void) { myfprintf(0, "%d", 3.0); return 0; }
EOF

out=$("$MCC" -Wformat -c offset.c -o offset.o 2>&1)
echo "$out" | grep -q "format '%d' expects an integer argument" \
	|| fail "offset format(printf,2,3) with a leading non-format arg should warn; got: $out"

out=$("$MCC" -Wformat -c offset.c -o offset.o 2>&1)
echo "$out" | grep -q "too many arguments" \
	&& fail "offset index handling miscounted the format-string position; got: $out"

cat > positional.c <<'EOF'
int printf(const char *, ...);
int main(void) { printf("%1$d\n", 5); return 0; }
EOF

out=$("$MCC" -Wformat -c positional.c -o positional.o 2>&1)
echo "$out" | grep -q "too many arguments" \
	&& fail "positional %1\$d must not trigger a too-many-arguments warning; got: $out"

echo "PASS: __attribute__((format)) drives -Wformat on user fns; positional %N\$ accepted"
exit 0
