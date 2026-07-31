#!/bin/sh
# Darwin thread-local variables across a link boundary.
#
# mcc has always been able to compile its own _Thread_local; what this covers is
# linking TLS that arrives INSIDE an object built by the system compiler, which
# needs the whole TLV path: the imported __thread_vars descriptor section, the
# ARM64_RELOC_TLVP_LOAD_PAGE21/PAGEOFF12 relocations, MH_HAS_TLV_DESCRIPTORS, and
# the descriptor's third word being an offset into the thread-local image rather
# than an absolute address or a chained-fixup word.
#
# Three shapes, because they fail independently and the middle one is the case a
# partial implementation passes:
#   1. mcc's own TLS only        -- the path that already worked; a guard against
#                                   regressing it while changing the shared code
#   2. imported TLS only         -- the descriptors come from the clang object
#   3. BOTH in one link          -- mcc must append its descriptors to the
#                                   imported section instead of minting a second
#                                   one; splitting them leaves half unrebased and
#                                   dyld rejects the image
#
# Each is compared against the same program built entirely by the system
# compiler, so the assertion is behavioural rather than a golden string.
#
# Usage: tlv.sh <mcc> <srcdir> <workdir> [-B<prefix>]
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: tlv.sh <mcc> <srcdir> <workdir> [-B<prefix>]" >&2
	exit 2
}

[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: needs a Darwin host"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
CC=${CC:-clang}
command -v "$CC" >/dev/null 2>&1 || { echo "SKIP: no system compiler to build the TLS object"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

# The imported half: TLS defined in an object mcc did not compile.
cat > "$WORK/lib.c" <<'EOF'
_Thread_local int tv = 42;
int get_tv(void) { return tv; }
void set_tv(int x) { tv = x; }
EOF

# 1: mcc's own TLS only.
cat > "$WORK/own.c" <<'EOF'
extern int printf(const char *, ...);
_Thread_local int a = 5;
_Thread_local int b;
int main(void) { b = a * 3; printf("a=%d b=%d\n", a, b); return 0; }
EOF

# 2: imported TLS only.
cat > "$WORK/imported.c" <<'EOF'
extern int printf(const char *, ...);
int get_tv(void);
void set_tv(int);
int main(void) { printf("tv=%d\n", get_tv()); set_tv(7); printf("tv=%d\n", get_tv()); return 0; }
EOF

# 3: both in one link.
cat > "$WORK/mixed.c" <<'EOF'
extern int printf(const char *, ...);
int get_tv(void);
void set_tv(int);
_Thread_local int mine = 11;
int main(void) { set_tv(4); printf("mine=%d tv=%d\n", mine, get_tv()); return 0; }
EOF

"$CC" -c -O1 "$WORK/lib.c" -o "$WORK/lib.o"

fail=0
for case in own imported mixed; do
	objs=""
	[ "$case" = own ] || objs="$WORK/lib.o"

	if ! "$MCC" $BFLAG -O1 "$WORK/$case.c" $objs -o "$WORK/$case.mcc" 2>"$WORK/$case.err"; then
		echo "FAIL[$case]: mcc could not link:"; sed 's/^/    /' "$WORK/$case.err"
		fail=1; continue
	fi
	"$CC" -O1 "$WORK/$case.c" $objs -o "$WORK/$case.ref"

	# An image whose TLV descriptors are malformed is killed by dyld before main,
	# so a clean exit is itself an assertion; the output compare is the rest.
	# Capture the status before testing it: inside `if ! cmd`, $? is the negation's
	# result, not the command's, and the diagnostic would always read rc=0.
	rc=0
	"$WORK/$case.mcc" > "$WORK/$case.out" 2>"$WORK/$case.rt" || rc=$?
	if [ "$rc" != 0 ]; then
		echo "FAIL[$case]: mcc-linked image did not run (rc=$rc):"; sed 's/^/    /' "$WORK/$case.rt"
		fail=1; continue
	fi
	"$WORK/$case.ref" > "$WORK/$case.refout"
	if ! cmp -s "$WORK/$case.out" "$WORK/$case.refout"; then
		echo "FAIL[$case]: output differs from the $CC-built reference"
		diff "$WORK/$case.refout" "$WORK/$case.out" | sed 's/^/    /' || true
		fail=1; continue
	fi
	echo "  $case: OK ($(tr '\n' ' ' < "$WORK/$case.out"))"
done

[ "$fail" = 0 ] || exit 1
echo "macho-tlv: OK"
