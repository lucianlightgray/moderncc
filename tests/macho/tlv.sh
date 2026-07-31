#!/bin/sh
# Darwin thread-local variables across a link boundary.
#
# mcc has always been able to compile its own _Thread_local; what this covers is
# linking TLS that arrives INSIDE an object built by the system compiler, which
# needs the whole TLV path: the imported __thread_vars descriptor section, the
# TLVP relocations (ARM64_RELOC_TLVP_LOAD_PAGE21/PAGEOFF12 on arm64,
# X86_64_RELOC_TLV on x86_64), MH_HAS_TLV_DESCRIPTORS, and the descriptor's third
# word being an offset into the thread-local image rather than an absolute
# address or a chained-fixup word.
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
# <arch> selects the Mach-O architecture; it must match what <mcc> targets. A
# non-host arch is fine as long as the host can execute those images (Rosetta for
# x86_64 on arm64) -- these are AOT binaries, not the dynamically-generated code
# Rosetta chokes on -- and the probe below skips the cell when it cannot.
#
# Usage: tlv.sh <mcc> <srcdir> <workdir> [-B<prefix>] [arch]
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
ARCH=$5
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: tlv.sh <mcc> <srcdir> <workdir> [-B<prefix>] [arch]" >&2
	exit 2
}

[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: needs a Darwin host"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
[ -n "$ARCH" ] || ARCH=$(uname -m)
# Pinned to clang rather than honouring $CC: the subject is Apple's TLV ABI, and
# clang is the only Darwin compiler that emits it. Homebrew GCC falls back to
# emutls (___emutls_v.tv plus a call to ___emutls_get_address, which lives in
# libgcc), so a gcc-built lib.o carries no __thread_vars descriptors to import --
# the case under test never runs, and the link fails on a libgcc symbol instead.
CC=clang
command -v "$CC" >/dev/null 2>&1 || { echo "SKIP: no clang to build the TLS object"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

# A cross-targeted mcc gets no SDK library path: mcc_add_macos_sdkpath is behind
# MCC_TARGET_IS_HOST, so only a native build finds libSystem by itself. The SDK's
# .tbd files cover every arch, so handing the same path over works for both.
SDKL=""
sdk=$(xcrun --show-sdk-path 2>/dev/null || true)
[ -n "$sdk" ] && [ -d "$sdk/usr/lib" ] && SDKL="-L$sdk/usr/lib"

echo 'int main(void) { return 0; }' > "$WORK/probe.c"
"$CC" -arch "$ARCH" "$WORK/probe.c" -o "$WORK/probe" 2>/dev/null ||
	{ echo "SKIP: this clang cannot target $ARCH"; exit 77; }
"$WORK/probe" || { echo "SKIP: this host cannot execute $ARCH images"; exit 77; }

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

"$CC" -arch "$ARCH" -c -O1 "$WORK/lib.c" -o "$WORK/lib.o"

# A gcc-built or otherwise emutls object would carry no descriptors to import,
# and cases 2 and 3 would then assert nothing.
otool -l "$WORK/lib.o" | grep -q __thread_vars || {
	echo "FAIL: $CC -arch $ARCH emitted no __thread_vars section; cases 2 and 3"
	echo "  would be vacuous -- check the compiler above"
	exit 1
}

fail=0
for case in own imported mixed; do
	objs=""
	[ "$case" = own ] || objs="$WORK/lib.o"

	if ! "$MCC" $BFLAG $SDKL -O1 "$WORK/$case.c" $objs -o "$WORK/$case.mcc" 2>"$WORK/$case.err"; then
		echo "FAIL[$case]: mcc could not link:"; sed 's/^/    /' "$WORK/$case.err"
		fail=1; continue
	fi
	"$CC" -arch "$ARCH" -O1 "$WORK/$case.c" $objs -o "$WORK/$case.ref"

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
echo "macho-tlv ($ARCH): OK"
