#!/bin/sh
# External-linker interop for a 64-bit ELF mcc cross target: compile a
# multi-function TU with mcc, then link the resulting object with the
# container's *native* toolchain (GNU ld via gcc) rather than mcc's own
# linker, and require it to (a) link without error and (b) run correct.
# Also compile a -gdwarf variant and require the linked binary's per-function
# DW_AT_low_pc values to be distinct.
#
# This is the regression guard for the RELA addend-0 bugs: mcc emitted FDE
# initial_location (.eh_frame) and DWARF DW_AT_low_pc/DW_OP_addr relocations
# with a hardcoded addend of 0 and the real value only in-place. mcc's own
# relocator adds the in-place value, so self-host worked; standard RELA
# linkers (GNU ld/lld) use S+A and ignored it, so .eh_frame FDEs collapsed
# (overlapping-FDE link failure) and every subprogram low_pc collapsed onto
# .text+0. i386/arm are REL and were never affected; only x86_64/arm64/riscv64
# (RELA) are, and nothing else in the suite links an mcc 64-bit object with an
# external linker. The program is freestanding (exit code = self-check result)
# so mcc needs no libc headers on the host.
#
# Usage:  tools/extlink-docker.sh <mcc> <docker-platform> [workdir]
#           docker-platform: linux/amd64 | linux/arm64
# Exit:   0 pass · 1 a failure · 77 skipped (no docker / mcc / platform)

set -eu

MCC="${1:-}"
PLAT="${2:-}"
WORK="${3:-./w-extlink}"

case "$PLAT" in
	linux/amd64) IMAGE="${MCC_EXTLINK_AMD64_IMAGE:-debian:bookworm-slim}" ;;
	linux/arm64) IMAGE="${MCC_EXTLINK_ARM64_IMAGE:-arm64v8/debian:bookworm-slim}" ;;
	*) echo "SKIP: unsupported platform '${PLAT:-<unset>}'"; exit 77 ;;
esac

if [ -z "$MCC" ] || [ ! -x "$MCC" ]; then echo "SKIP: mcc not found at '${MCC:-<unset>}'"; exit 77; fi
if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! docker run --rm --platform "$PLAT" "$IMAGE" true >/dev/null 2>&1; then
	echo "SKIP: cannot run $PLAT containers ($IMAGE)"; exit 77
fi

rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS=$(cd "$WORK" && pwd)

cat > "$WORK_ABS/m.c" <<'EOF'
unsigned ud(unsigned x){ return x / 7u; }
int sd(int x){ return x / 7; }
long long ll(long long x){ return x / 7; }
unsigned long long ull(unsigned long long x){ return x / 7ull; }
int sel(int a, int b){ return a < b ? a : b; }
int main(void){
	unsigned bad = 0; long i;
	for (i = -100000; i <= 100000; i += 37) {
		if (ud((unsigned)i) != (unsigned)i / 7u) bad++;
		if (sd((int)i) != (int)i / 7) bad++;
		if (ll(i) != i / 7) bad++;
		if (ull((unsigned long long)i) != (unsigned long long)i / 7ull) bad++;
		if (sel((int)i, (int)i + 3) != ((int)i < (int)i + 3 ? (int)i : (int)i + 3)) bad++;
	}
	return (int)bad;
}
EOF

echo "== host: mcc -> ELF objects (-O0, -O2, -gdwarf-4) =="
"$MCC" -O0 -c "$WORK_ABS/m.c" -o "$WORK_ABS/m0.o"
"$MCC" -O2 -c "$WORK_ABS/m.c" -o "$WORK_ABS/m2.o"
"$MCC" -gdwarf-4 -O0 -c "$WORK_ABS/m.c" -o "$WORK_ABS/mg.o"

echo "== docker $PLAT: link mcc objects with GNU ld (gcc), run, check DWARF =="
docker run --rm --platform "$PLAT" -v "$WORK_ABS":/w -w /w "$IMAGE" sh -c '
	command -v gcc >/dev/null 2>&1 || { apt-get update >/dev/null 2>&1; apt-get install -y gcc binutils >/dev/null 2>&1; }
	fail=0
	for o in 0 2; do
		if ! gcc m${o}.o -o m${o} 2>gcc_err; then
			echo "FAIL  -O$o external link:"; sed "s/^/    /" gcc_err; fail=1; continue
		fi
		rc=0; ./m${o} || rc=$?
		if [ "$rc" = 0 ]; then echo "OK    -O$o link+run (exit 0)"; else echo "FAIL  -O$o run exit=$rc (should be 0)"; fail=1; fi
	done
	# -gdwarf: link and require distinct per-function DW_AT_low_pc
	if ! gcc -g mg.o -o mg 2>gcc_err; then
		echo "FAIL  -gdwarf external link:"; sed "s/^/    /" gcc_err; fail=1
	else
		n=$(readelf --debug-dump=info mg 2>/dev/null | awk "/DW_TAG_subprogram/{p=1} p&&/DW_AT_low_pc/{print \$NF; p=0}" | sort -u | wc -l)
		if [ "$n" -ge 5 ]; then echo "OK    -gdwarf $n distinct subprogram low_pc"; else echo "FAIL  -gdwarf only $n distinct low_pc (collapsed onto .text+0)"; fail=1; fi
	fi
	exit $fail
'
