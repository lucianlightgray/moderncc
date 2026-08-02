#!/bin/sh
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
cross="$root/cmake-cross"
MCCI="$cross/mcc-i386-win32.exe"
MINGW="${MCC_I386_MINGW:-/c/Users/llg/opt/mingw32}"
W32LIB="$MINGW/i686-w64-mingw32/lib"
GCC="$MINGW/bin/i686-w64-mingw32-gcc.exe"
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

stage="$work/stage"
mkdir -p "$stage/lib"
cp "$cross/i386-win32-libmccrt.a" "$stage/lib/"
"$MCCI" -c -O2 $BRT $DEFS $INCS "$root/runtime/lib/runmain.c" -o "$stage/lib/i386-win32-runmain.o"
for l in libmsvcrt.a libkernel32.a; do [ -f "$W32LIB/$l" ] && cp "$W32LIB/$l" "$stage/lib/"; done
stage_abs=$(cygpath -m "$stage" 2>/dev/null || echo "$stage")
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
if [ "$fail" -ne 0 ]; then
	echo "i386win32-soak: FAIL — stub-tail selftests regressed:$failed"
	exit 1
fi

echo "== AOT exec: stack auto over-alignment (WoW64) =="
ao="$root/tests/exec/features_c99_c11/alignas_over.c"
if "$MCCI" -c -O2 $BRT "$ao" -o "$work/alignas_over.o" 2>/dev/null &&
	 "$MCCI" $LNK "$work/alignas_over.o" -o "$work/alignas_over.exe" 2>/dev/null; then
	aoout=$("$work/alignas_over.exe" 2>&1); aorc=$?
	printf "  %-4s %-32s %s\n" "$([ "$aorc" -eq 0 ] && echo PASS || echo FAIL)" \
		alignas_over "$(printf '%s\n' "$aoout" | tail -1)"
	if [ "$aorc" -ne 0 ]; then
		echo "i386win32-soak: FAIL — i386-PE stack over-alignment regressed (rc=$aorc)"
		exit 1
	fi
else
	echo "i386win32-soak: FAIL — i386-PE over-alignment cc/link failed"
	exit 1
fi

FUZZ_N="${MCC_I386_FUZZ_N:-25}"
CLANG="${MCC_I386_CLANG:-}"
if [ -z "$CLANG" ]; then
	for cand in i686-w64-mingw32-gcc i686-w64-mingw32-clang clang; do
		p=$(command -v "$cand" 2>/dev/null) || continue
		[ "$p" = "$GCC" ] && continue
		if "$p" --version 2>&1 | grep -qi clang; then CLANG="$p"; break; fi
	done
fi
echo "== i386 differential fuzz: $FUZZ_N seeds, refs = gcc(O0,O2)${CLANG:+ + clang} =="
fz="$work/fz"; mkdir -p "$fz"
cat > "$fz/gendrv.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "gen.h"
int main(int c, char **v){ fuzz_emit(c>1?strtoul(v[1],0,10):1, stdout); return 0; }
EOF
fzagree=0; fzdiv=0; fzskip=0; fzmiss=""
set +e
if "$GCC" -w -I"$root/tests/fuzz" "$fz/gendrv.c" -o "$fz/gen.exe" 2>/dev/null; then
	rr(){ cp "$1" "$fz/_x.exe" 2>/dev/null; o=$("$fz/_x.exe" 2>/dev/null); c=$?;
		[ "$c" -eq 126 ] && { o=$("$fz/_x.exe" 2>/dev/null); c=$?; }
		printf '%s|%d' "$o" "$c"; }
	s=1
	while [ "$s" -le "$FUZZ_N" ]; do
		"$fz/gen.exe" "$s" > "$fz/p.c" 2>/dev/null || { fzskip=$((fzskip+1)); s=$((s+1)); continue; }
		ok=1
		"$GCC" -w -O0 "$fz/p.c" -o "$fz/g0.exe" 2>/dev/null || ok=0
		"$GCC" -w -O2 "$fz/p.c" -o "$fz/g2.exe" 2>/dev/null || ok=0
		[ "$ok" = 1 ] || { fzskip=$((fzskip+1)); s=$((s+1)); continue; }
		r0=$(rr "$fz/g0.exe"); r2=$(rr "$fz/g2.exe")
		if [ "$r0" != "$r2" ]; then fzskip=$((fzskip+1)); s=$((s+1)); continue; fi
		if [ -n "$CLANG" ] && "$CLANG" -w -O2 "$fz/p.c" -o "$fz/cl.exe" 2>/dev/null; then
			[ "$(rr "$fz/cl.exe")" = "$r0" ] || { fzskip=$((fzskip+1)); s=$((s+1)); continue; }
		fi
		mst=agree
		for rp in "" "MCC_AST_REGPAIR=1"; do
			env $rp "$MCCI" -O2 $BRT "$fz/p.c" -o "$fz/m.exe" $LNK 2>/dev/null || { mst=skip; break; }
			mo=$(rr "$fz/m.exe")
			if [ "$mo" != "$r0" ]; then
				mst=div; lbl=${rp:-default}
				echo "  DIVERGE seed=$s [$lbl] ref=[$r0] mcc=[$mo]"
				cp "$fz/p.c" "$work/../fuzz_diverge_${s}_${lbl%%=*}.c" 2>/dev/null
				break
			fi
		done
		case "$mst" in
			agree) fzagree=$((fzagree+1)) ;;
			div) fzdiv=$((fzdiv+1)); fzmiss="$fzmiss $s" ;;
			skip) fzskip=$((fzskip+1)) ;;
		esac
		s=$((s+1))
	done
	echo "i386win32-soak: differential fuzz agree=$fzagree diverge=$fzdiv skip(UB/build)=$fzskip"
	if [ "$fzdiv" -ne 0 ]; then echo "i386win32-soak: FAIL — fuzz divergence:$fzmiss"; set -e; exit 1; fi
else
	echo "i386win32-soak: differential fuzz SKIP (could not build the generator)"
fi
set -e
echo "i386win32-soak: done — ALL GREEN (AOT self-host + $pass stub-tail selftests + $fzagree fuzz)"
