#!/bin/sh
MCC=$1
BDIR=$2
W=$BDIR/perfn-cache-test
rm -rf "$W"
mkdir -p "$W" || exit 1

cat > "$W/a.c" <<'EOF'
int f(int x) { int i, s = 0; for (i = 0; i < x; i++) s += i * 3; return s; }
int g(int x) { int i, s = 1; for (i = 0; i < x; i++) s += i * 5; return s; }
int main(void) { return f(10) == 135 ? 0 : 1; }
EOF

run() {
	env MCC_AST_PERFN=1 XDG_CACHE_HOME="$W/cache" HOME="$W" \
		"$MCC" "-B$BDIR" -O4 -v -c -o "$W/a.o" "$W/a.c" 2>/dev/null
}

cached() {
	echo "$1" | sed -n 's/.*superopt-perfn: [0-9]* functions (\([0-9]*\) cached).*/\1/p'
}

out1=$(run) || { echo "run1 failed"; exit 1; }
[ "$(cached "$out1")" = "0" ] || {
	echo "expected 0 cached on the cold run, got '$(cached "$out1")'"; exit 1; }

out2=$(run) || { echo "run2 failed"; exit 1; }
[ "$(cached "$out2")" = "3" ] || {
	echo "expected all 3 functions cached on the warm run, got '$(cached "$out2")'"; exit 1; }

sed 's/i \* 5/i * 7/' "$W/a.c" > "$W/a2.c" && mv "$W/a2.c" "$W/a.c"

out3=$(run) || { echo "run3 failed"; exit 1; }
[ "$(cached "$out3")" = "2" ] || {
	echo "expected 2 cached after editing one function, got '$(cached "$out3")'"; exit 1; }

echo OK
