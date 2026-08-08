#!/bin/sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
img=debian:bookworm-slim
name=mcc-selfhost-rv64-$$

SR="$root/vendor/gentoo-stage3-riscv64-glibc"
RTA="$root/cmake-cross/riscv64-libmccrt.a"
if command -v qemu-riscv64 >/dev/null 2>&1 && command -v cc >/dev/null 2>&1 \
   && [ -d "$SR" ] && [ -f "$RTA" ]; then
    exec "$(dirname "$0")/selfhost-cross-native.sh" riscv64 "$@"
fi

. "$(dirname "$0")/dockergate.sh"
dg_need_docker
dg_need_platform "" "$img"
dg_need_mount "$root"

cleanup() { docker rm -f "$name" >/dev/null 2>&1 || true; }
trap cleanup EXIT

docker run -d --name "$name" -v "$root":/work -w /work "$img" sleep 3600 >/dev/null
docker exec "$name" sh -c \
  'apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq gcc gcc-riscv64-linux-gnu qemu-user >/dev/null 2>&1' \
  || { echo "cross toolchain install failed"; exit 77; }

docker exec -e KNOBS="$*" "$name" sh -c '
set -e
cd /tmp
Q="qemu-riscv64 -L /usr/riscv64-linux-gnu"
INC="-I/work/src -I/work/src/formats -I/work/src/objfmt -I/work/src/arch/riscv64 -I/work/include"
SYS="-I/usr/riscv64-linux-gnu/include -I/usr/lib/gcc-cross/riscv64-linux-gnu/12/include"
DEF="-DMCC_CONFIG_OPTIMIZER=1 -DMCC_TARGET_RISCV64"
ARGS="-B/work -I/work/runtime/include $INC $SYS $DEF"

need() { [ -s "$1" ] || { echo "FAIL: $2 produced no output"; exit 1; }; }

gcc -w $DEF $INC -O0 -o /tmp/mcc0 /work/src/mcc.c
need /tmp/mcc0 "stage0"
echo "stage0: host tool emitting riscv64"

rm -f /tmp/o1.o /tmp/o2.o /tmp/o3.o /tmp/mcc1 /tmp/mcc2

echo "stage1: mcc0 compiles src/mcc.c"
env $KNOBS /tmp/mcc0 $ARGS -c -O2 /work/src/mcc.c -o /tmp/o1.o
need /tmp/o1.o "stage1"
riscv64-linux-gnu-gcc -static /tmp/o1.o -o /tmp/mcc1 2>/dev/null
need /tmp/mcc1 "stage1 link"

echo "stage2: riscv64 mcc1 compiles src/mcc.c under qemu"
env $KNOBS $Q /tmp/mcc1 $ARGS -c -O2 /work/src/mcc.c -o /tmp/o2.o
need /tmp/o2.o "stage2"
riscv64-linux-gnu-gcc -static /tmp/o2.o -o /tmp/mcc2 2>/dev/null
need /tmp/mcc2 "stage2 link"

echo "stage3: riscv64 mcc2 compiles src/mcc.c under qemu"
env $KNOBS $Q /tmp/mcc2 $ARGS -c -O2 /work/src/mcc.c -o /tmp/o3.o
need /tmp/o3.o "stage3"

echo "sizes o1=$(stat -c%s /tmp/o1.o) o2=$(stat -c%s /tmp/o2.o) o3=$(stat -c%s /tmp/o3.o)"
cmp /tmp/o1.o /tmp/o2.o || { echo "FAIL: o1 != o2"; exit 1; }
cmp /tmp/o2.o /tmp/o3.o || { echo "FAIL: o2 != o3"; exit 1; }
echo "riscv64 selfhost: OK (o1 == o2 == o3, byte-identical)"

cat > /tmp/sanity.c <<EOF
#include <stdio.h>
#include <string.h>
struct P { double x, y; int n; };
static double norm(struct P *p) { return -(p->x * p->x + p->y * p->y); }
int main(void) {
  struct P p; char buf[64]; int i; long acc = 0;
  p.x = 3.0; p.y = 4.0; p.n = 7;
  for (i = 0; i < 10; i++) acc += i * p.n;
  snprintf(buf, sizeof buf, "%.3f/%ld/%d", norm(&p), acc, (int)strlen("abcd"));
  printf("%s\n", buf);
  return 0;
}
EOF
riscv64-linux-gnu-gcc -w -O2 -static /tmp/sanity.c -o /tmp/ref
ref=$($Q /tmp/ref)
for O in -O0 -O1 -O2; do
  rm -f /tmp/s.o /tmp/s
  env $KNOBS $Q /tmp/mcc1 -B/work -I/work/runtime/include $SYS -c $O /tmp/sanity.c -o /tmp/s.o
  need /tmp/s.o "sanity $O"
  riscv64-linux-gnu-gcc -static /tmp/s.o -o /tmp/s 2>/dev/null
  got=$($Q /tmp/s)
  [ "$got" = "$ref" ] || { echo "FAIL: sanity $O got [$got] want [$ref]"; exit 1; }
done
echo "riscv64 sanity: OK (mcc1 output matches gcc at -O0/-O1/-O2)"
'
