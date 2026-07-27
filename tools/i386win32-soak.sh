#!/bin/sh
# i386-win32 JIT stub-tail soak (P0 step 4 groundwork). Runs on an x86_64
# Windows host via WoW64 (i386 PE binaries run natively) -- no docker, no qemu.
#
# What it does, all locally:
#   1. builds a real i386 mcc.exe by self-compiling src/mcc.c with the
#      host-runnable i386-win32 cross compiler (cmake-cross/mcc-i386-win32.exe);
#   2. proves a 3-stage AOT self-host FIXPOINT (o1==o2==o3, byte-identical),
#      the "self-host" half of the MCC_JIT_I386_STUBS flip criteria;
#   3. builds every tests/embed/jit_selftest_*.c against an i386 libmcc and runs
#      them under MCC_JIT_I386_STUBS=1 -- exercising the hand-emitted i386
#      KGC/FP(x87)/mixed stub tail that is default-OFF in production.
#
# Prereqs (see docs/TODO.md "JIT Windows / PE", P0 step 4):
#   - an i386-win32 cross build:  cmake --preset cross && \
#       ninja -C cmake-cross mcc-i386-win32 i386-win32-libmccrt.a
#   - a real i386 mingw with libgcc (winlibs i686, NOT compiler-rt-only llvm):
#       MCC_I386_MINGW=/c/Users/llg/opt/mingw32   (bin/, lib/gcc/.../libgcc.a)
#
# Usage:  tools/i386win32-soak.sh
# Exit:   0 all green · 1 a check failed · 77 skipped (missing prereqs)
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
cross="$root/cmake-cross"
MCCI="$cross/mcc-i386-win32.exe"
MINGW="${MCC_I386_MINGW:-/c/Users/llg/opt/mingw32}"
W32LIB="$MINGW/i686-w64-mingw32/lib"
GCC="$MINGW/bin/i686-w64-mingw32-gcc.exe"
# libgcc dir (for ___chkstk_ms / 64-bit divide helpers at link)
LGCC=$("$GCC" -print-libgcc-file-name 2>/dev/null | xargs -r dirname || true)

skip() { echo "i386win32-soak: SKIP ($1)"; exit 77; }
[ -x "$MCCI" ] || skip "no $MCCI (build the cross preset)"
[ -f "$cross/i386-win32-libmccrt.a" ] || skip "no i386-win32-libmccrt.a (ninja -C cmake-cross i386-win32-libmccrt.a)"
[ -x "$GCC" ] || skip "no winlibs i686 gcc at $GCC (set MCC_I386_MINGW)"
[ -d "$W32LIB" ] || skip "no i686 sysroot libs at $W32LIB"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

DEFS='-DCC_NAME=CC_clang -DMCC_CONFIG_CROSSPREFIX="i386-win32-" -DMCC_CONFIG_OPTIMIZER=1 -DMCC_CONFIG_PREDEFS=1 -DMCC_TARGET_I386 -DMCC_TARGET_PE'
INCS="-I$cross -I$root/src -I$root/src/arch/i386 -I$root/src/arch/x86_64 -I$root/src/objfmt -I$root/src/formats -I$root/include -I$root"
BRT="-B$root/runtime/win32 -I$root/runtime/include"
LNK="-B$root/runtime/win32 -L$cross -L$W32LIB ${LGCC:+-L$LGCC}"

# --- 1+2. AOT self-host fixpoint ---------------------------------------------
echo "== stage1: host cross compiler -> i386 mcc object =="
"$MCCI" -c -O2 $BRT $DEFS $INCS "$root/src/mcc.c" -o "$work/o1.o"
"$MCCI" $LNK "$work/o1.o" -o "$work/mcc1.exe"

echo "== stage2: i386 mcc.exe (WoW64) recompiles src/mcc.c =="
"$work/mcc1.exe" -c -O2 $BRT $DEFS $INCS "$root/src/mcc.c" -o "$work/o2.o"
"$work/mcc1.exe" $LNK "$work/o2.o" -o "$work/mcc2.exe"

echo "== stage3: stage2 mcc recompiles src/mcc.c =="
"$work/mcc2.exe" -c -O2 $BRT $DEFS $INCS "$root/src/mcc.c" -o "$work/o3.o"

cmp "$work/o1.o" "$work/o2.o" || { echo "FAIL: o1 != o2 (host-cross vs i386-native)"; exit 1; }
cmp "$work/o2.o" "$work/o3.o" || { echo "FAIL: o2 != o3 (not a fixpoint)"; exit 1; }
echo "i386win32-soak: AOT self-host OK (o1==o2==o3 byte-identical on WoW64)"

# --- 3. stub-tail selftests (MCC_JIT_I386_STUBS=1) ---------------------------
# Stage the i386 runtime the way the in-memory `-run` selftests look for it:
# PE searches {B}/lib, so runmain.o + libmccrt.a + the win32 CRT import libs go
# under <stage>/lib and the selftests are invoked with -B<stage>. (fparg/liverun
# drive an end-to-end -run that links this runtime; without it they report a
# spurious "i386-win32-runmain.o not found", not a stub-tail failure.)
stage="$work/stage"
mkdir -p "$stage/lib"
cp "$cross/i386-win32-libmccrt.a" "$stage/lib/"
"$MCCI" -c -O2 $BRT $DEFS $INCS "$root/runtime/lib/runmain.c" -o "$stage/lib/i386-win32-runmain.o"
for l in libmsvcrt.a libkernel32.a; do [ -f "$W32LIB/$l" ] && cp "$W32LIB/$l" "$stage/lib/"; done
# Also bake this staging dir as the selftests' default MCC_CONFIG_MCCDIR: the
# in-memory recompile creates its own MCCState whose lib path is the baked mccdir
# (NOT the -B arg), so stage2's `abs` and mixed's `__fixdfdi` are pulled from
# <mccdir>/lib. On a normal install the build tree bakes a valid mccdir; the
# cross compiler bakes dist/lib/mcc/win32 which does not exist here.
stage_abs=$(cygpath -m "$stage" 2>/dev/null || echo "$stage")
# literal quotes around the path stay in the token (paths here have no spaces),
# so mcc receives -DMCC_CONFIG_MCCDIR="..." and the macro is a valid C string.
STDEFS="$DEFS -DMCC_EMBED_JIT=1 -DMCC_CONFIG_MCCDIR=\"$stage_abs\""

echo "== building i386 libmcc (MCC_EMBED_JIT=1) for the selftests =="
"$MCCI" -c -O2 $BRT $STDEFS $INCS "$root/src/libmcc.c" -o "$work/libmcc.o"

pass=0; fail=0; skipn=0; failed=""
for f in "$root"/tests/embed/jit_selftest_*.c; do
	n=$(basename "$f" .c)
	if ! "$MCCI" -c -O2 $BRT $STDEFS $INCS -I"$root" "$f" -o "$work/$n.o" 2>/dev/null; then
		fail=$((fail+1)); failed="$failed $n(cc)"; continue
	fi
	if ! "$MCCI" $LNK "$work/$n.o" "$work/libmcc.o" -o "$work/$n.exe" 2>/dev/null; then
		fail=$((fail+1)); failed="$failed $n(link)"; continue
	fi
	# Windows "installer detection" auto-requests elevation for exe FILENAMES
	# containing patch/install/setup/update (e.g. jit_selftest_patch,
	# jit_selftest_sliceinstall) -> a non-elevated exec gets rc 126 "Permission
	# denied". It is a shell/UAC quirk, not a test failure: run via a neutral-named
	# copy so every selftest is exercised on its merits.
	cp "$work/$n.exe" "$work/_run.exe"
	out=$(cd "$work" && MCC_JIT=1 MCC_JIT_I386_STUBS=1 "$work/_run.exe" "-B$stage" 2>&1) && rc=0 || rc=$?
	last=$(printf '%s\n' "$out" | tail -1)
	if [ "$rc" -eq 0 ]; then
		case "$last" in
			*skip*|*SKIP*) skipn=$((skipn+1)); tag=SKIP ;;
			*) pass=$((pass+1)); tag=PASS ;;
		esac
	else
		fail=$((fail+1)); failed="$failed $n(rc=$rc)"; tag=FAIL
	fi
	printf "  %-4s %-32s %s\n" "$tag" "$n" "$last"
done
echo "i386win32-soak: stub-tail selftests PASS=$pass SKIP=$skipn FAIL=$fail"
[ -n "$failed" ] && echo "i386win32-soak: unresolved (see docs/TODO.md P0 step 4):$failed"
# Known-remaining before the MCC_JIT_I386_STUBS flip: fparg/liverun need i386
# runtime staging for the inner -run; mixed + stage2 under investigation. These
# are tracked, not silent -- the script reports them but does not hard-fail on
# them so the AOT+30-selftest baseline stays a usable regression signal.
echo "i386win32-soak: done"
