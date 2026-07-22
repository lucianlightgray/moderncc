#!/usr/bin/env bash
# DWARF debug-info correctness guard: does gdb, driven by mcc's -gdwarf output,
# report correct source lines for EVERY frame in a backtrace (not just the
# innermost)? mcc used to emit the .debug_line end_sequence at the last
# line-change address instead of the code end, so the last function got a
# zero-length final line range and gdb showed "main ()" with no "at file:line"
# in a backtrace. The extlink guard only checks distinct DW_AT_low_pc; nothing
# exercised the line table through an external consumer (gdb) until this.
#
# gdb needs real ptrace, which qemu-user does not fully support, so this runs on
# the HOST-NATIVE platform (arm64 on Apple Silicon, amd64 on CI x86). mcc's line
# table is emitted by arch-independent mccdbg.c, so validating one native arch
# covers the fix for all.
#
# Usage:  tools/dwarfgdb-docker.sh [workdir]
# Exit:   0 pass · 1 a failure · 77 skipped (no docker / gdb / platform)
set -eu

REPO="$(cd "$(dirname "$0")/.." && pwd)"
HP="$(cd "$REPO" && (pwd -W 2>/dev/null || pwd))"
WORK="${1:-./w-dwarfgdb}"
rm -rf "$WORK"; mkdir -p "$WORK"
WORK_ABS="$(cd "$WORK" && pwd)"
WP="$(cd "$WORK_ABS" && (pwd -W 2>/dev/null || pwd))"

HOSTM="$(uname -m)"
case "$HOSTM" in
	aarch64|arm64) IMAGE="arm64v8/debian:bookworm-slim"; MDEF="-DMCC_TARGET_ARM64=1" ;;
	*)             IMAGE="debian:bookworm-slim";          MDEF="-DMCC_TARGET_X86_64=1" ;;
esac

if ! command -v docker >/dev/null 2>&1; then echo "SKIP: docker not available"; exit 77; fi
if ! docker info >/dev/null 2>&1; then echo "SKIP: docker daemon not available"; exit 77; fi

MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm -e MDEF="$MDEF" \
  -v "$HP":/repo:ro -v "$WP":/w -w /w "$IMAGE" bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update >/dev/null 2>&1 || { echo "SKIP: apt update failed (no network?)"; exit 77; }
apt-get install -y --no-install-recommends gcc libc6-dev gdb binutils ca-certificates >/dev/null 2>&1 \
  || { echo "SKIP: apt install failed"; exit 77; }
command -v gdb >/dev/null 2>&1 || { echo "SKIP: gdb unavailable"; exit 77; }

mkdir -p /b; cp -a /repo/src /repo/include /repo/runtime /b/
INC="-I src -I src/arch/i386 -I src/arch/x86_64 -I src/arch/arm -I src/arch/arm64 -I src/arch/riscv64 -I src/objfmt -I src/formats -I include"
cd /b
echo "== build mcc ($MDEF) =="
gcc -O1 -w -DMCC_CONFIG_OPTIMIZER=1 $MDEF $INC src/mcc.c -o /w/mcc

cat > /w/t.c <<EOF
extern int printf(const char*,...);
int compute(int x, int y){
  int sum = x + y;
  int prod = x * y;
  return sum + prod;
}
int main(void){ int r = compute(3, 4); printf("%d\n", r); return 0; }
EOF
cat > /w/c.gdb <<EOF
set pagination off
break compute
run
bt
quit
EOF

/w/mcc -gdwarf-4 -O0 -I /b/runtime/include -c /w/t.c -o /w/t.o
gcc -g /w/t.o -o /w/t

echo "== gdb backtrace on mcc -gdwarf output =="
BT=$(gdb -q -batch -x /w/c.gdb /w/t 2>&1 | grep -E "^#[0-9]")
echo "$BT" | sed "s/^/   /"

# The innermost frame (#0 compute) and the caller frame (#1 main) must BOTH
# report a source location. Before the fix, main had no line-table coverage so
# its frame printed "main ()" with no "at t.c:N".
n0=$(echo "$BT" | grep -c "compute .* at .*t.c:")
n1=$(echo "$BT" | grep -c "main () at .*t.c:")
echo "== decoded line table (last-function coverage) =="
objdump --dwarf=decodedline /w/t 2>/dev/null | grep "t.c *[0-9]" | tail -4 | sed "s/^/   /"

if [ "$n0" -ge 1 ] && [ "$n1" -ge 1 ]; then
  echo "PASS: every backtrace frame has a source line (compute + main)"
else
  echo "FAIL: a backtrace frame is missing its source line (n0=$n0 n1=$n1) -- .debug_line last-function coverage regressed"
  exit 1
fi
'
