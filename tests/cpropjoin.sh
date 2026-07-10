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
int ext(int x);
int cse_join(int x, int f) {
	int t = x * 3 + 1, u, v;
	if (f) u = x * 3 + 1; else u = x * 3 + 1;
	v = x * 3 + 1;
	return t + u + v + ext(x * 3 + 1);
}
int cse_killed(int x, int f) {
	int t = x * 3 + 1, u;
	if (f) x = 9; else x = 10;
	u = x * 3 + 1;
	return t + u;
}
int cse_loop(int x, int n) {
	int b = x * 5, s = 0, i;
	for (i = 0; i < n; i++) s += x * 5;
	return s + b;
}
int ext(int x) { return x / 2; }
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
	acc = acc * 31 + cse_join(2, 1);
	acc = acc * 31 + cse_join(2, 0);
	acc = acc * 31 + cse_killed(5, 1);
	acc = acc * 31 + cse_killed(5, 0);
	acc = acc * 31 + cse_loop(3, 4);
	return ((unsigned)acc % 251u);
}
EOF

build() {
	env MCC_AST_CPROP_JOIN="$1" MCC_AST_CSE_JOIN="$2" \
		"$MCC" "-B$BDIR" "$3" -o "$4" "$W/j.c" 2>/dev/null
}

build 0 0 -O0 "$W/j-o0" || { echo "O0 build failed"; exit 1; }
"$W/j-o0"
ref=$?

for lvl in -O1 -O2 -O3; do
	for gc in 0 1; do
		for ge in 0 1; do
			build "$gc" "$ge" "$lvl" "$W/j-t" || {
				echo "$lvl cprop=$gc cse=$ge build failed"; exit 1; }
			"$W/j-t"
			rc=$?
			[ "$rc" = "$ref" ] || {
				echo "$lvl cprop=$gc cse=$ge rc=$rc, expected $ref"; exit 1; }
		done
	done
done

echo OK
