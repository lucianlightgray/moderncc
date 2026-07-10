#!/bin/sh
MCC=$1
BDIR=$2
W=$BDIR/cprop-join-test
rm -rf "$W"
mkdir -p "$W" || exit 1

cat > "$W/j.c" <<'EOF'
int same_join(int f) {
	int a = 10, b;
	if (f) b = a + 1; else b = a + 1;
	return b + a;
}
int diff_join(int f) {
	int b;
	if (f) b = 3; else b = 4;
	return b;
}
int one_arm(int f) {
	int b = 5;
	if (f) b = 5;
	return b;
}
int loop_inv(int n) {
	int c = 7, s = 0, i;
	for (i = 0; i < n; i++) s += c;
	return s;
}
int loop_written(int n) {
	int c = 1, i;
	for (i = 0; i < n; i++) c = c * 2;
	return c;
}
int nested(int f, int n) {
	int k = 9, s = 0, i;
	for (i = 0; i < n; i++) {
		if (f) s += k; else s += k + 1;
	}
	return s;
}
int with_break(int n) {
	int c = 6, s = 0, i;
	for (i = 0; i < n; i++) {
		if (i == 2) break;
		s += c;
	}
	return s + c;
}
int escaped(int f) {
	int a = 8, b;
	int *p = &a;
	*p = f ? 20 : 30;
	if (f) b = a; else b = a;
	return b;
}
int do_loop(int n) {
	int c = 11, s = 0, i = 0;
	do { s += c; i++; } while (i < n);
	return s;
}
int main(void) {
	int acc = 0;
	acc = acc * 31 + same_join(1);
	acc = acc * 31 + same_join(0);
	acc = acc * 31 + diff_join(1);
	acc = acc * 31 + diff_join(0);
	acc = acc * 31 + one_arm(0);
	acc = acc * 31 + one_arm(1);
	acc = acc * 31 + loop_inv(0);
	acc = acc * 31 + loop_inv(4);
	acc = acc * 31 + loop_written(5);
	acc = acc * 31 + nested(1, 3);
	acc = acc * 31 + nested(0, 3);
	acc = acc * 31 + with_break(9);
	acc = acc * 31 + escaped(1);
	acc = acc * 31 + escaped(0);
	acc = acc * 31 + do_loop(3);
	return ((unsigned)acc % 251u);
}
EOF

build() {
	env MCC_AST_CPROP_JOIN="$1" "$MCC" "-B$BDIR" "$2" -o "$3" "$W/j.c" \
		2>/dev/null
}

build 0 -O0 "$W/j-o0" || { echo "O0 build failed"; exit 1; }
"$W/j-o0"
ref=$?

for lvl in -O1 -O2 -O3; do
	for g in 0 1; do
		build "$g" "$lvl" "$W/j-t" || { echo "$lvl gate=$g build failed"; exit 1; }
		"$W/j-t"
		rc=$?
		[ "$rc" = "$ref" ] || {
			echo "$lvl gate=$g rc=$rc, expected $ref"; exit 1; }
	done
done

echo OK
