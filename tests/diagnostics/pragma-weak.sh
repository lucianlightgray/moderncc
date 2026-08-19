#!/bin/sh
set -u

MCC="$1"
WORK="$2"

mkdir -p "$WORK" 2>/dev/null || exit 2
cd "$WORK" || exit 2
[ -x "$MCC" ] || exit 77
command -v nm >/dev/null 2>&1 || exit 77

cat > w.c <<'EOF'
#pragma weak wsym
int wsym(void){ return 1; }
int strongsym(void){ return 2; }
EOF

"$MCC" -c w.c -o w.o || { echo "FAIL: compile error"; exit 1; }

syms=$(nm w.o 2>/dev/null)

case "$syms" in
	*[wW]" "*wsym*) : ;;
	*) echo "FAIL: #pragma weak did not mark wsym weak:"; echo "$syms"; exit 1 ;;
esac

echo "$syms" | grep -i 'strongsym' | grep -qi ' T ' || {
	echo "FAIL: strongsym is not a strong (T) symbol:"; echo "$syms"; exit 1; }

echo "$syms" | grep -i 'wsym' | grep -qi ' T ' && {
	echo "FAIL: wsym is still strong (T), #pragma weak ignored:"; echo "$syms"; exit 1; }

cat > wov.c <<'EOF'
#pragma weak val
int val(void){ return 1; }
extern int printf(const char *, ...);
int main(void){ printf("%d\n", val()); return 0; }
EOF
cat > strong.c <<'EOF'
int val(void){ return 99; }
EOF
"$MCC" -c strong.c -o strong.o || { echo "FAIL: strong.c compile"; exit 1; }
"$MCC" wov.c strong.o -o wov || { echo "FAIL: link with weak+strong"; exit 1; }
got=$(./wov 2>/dev/null)
[ "$got" = "99" ] || { echo "FAIL: strong override did not win over weak (got '$got', want 99)"; exit 1; }

echo "PASS: #pragma weak marks the symbol weak; strong definition overrides the weak one"
exit 0
