#!/bin/sh
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
arch=${1:-riscv64}
[ $# -gt 0 ] && shift
KNOBS="$*"

case "$arch" in
  riscv64)
    CROSSCC=riscv64-linux-gnu-gcc; TDEF=MCC_TARGET_RISCV64; ADIR=riscv64
    QEMU=qemu-riscv64
    SR="$root/vendor/gentoo-stage3-riscv64-glibc"
    LOADER="$SR/usr/lib64/ld-linux-riscv64-lp64d.so.1"
    RTANAME=riscv64-libmccrt.a
    BUILDFLAGS="-O0 -DMCC_CONFIG_STATIC"
    BUILDLIBS="-lm"
    SYSINC="-I$SR/usr/include"
    EXTRALINK=""
    HOT=998508278240
    ;;
  arm)
    CROSSCC=arm-linux-gnueabihf-gcc; TDEF=MCC_TARGET_ARM; ADIR=arm
    QEMU=qemu-arm
    SR="/usr/arm-linux-gnueabihf"
    LOADER=""
    RTANAME=arm-libmccrt.a
    BUILDFLAGS="-O0 -static -DMCC_CONFIG_STATIC"
    BUILDLIBS="-lm"
    SYSINC="-I$SR/include"
    EXTRALINK=""
    HOT=2075865568
    ;;
  arm64)
    CROSSCC="${CC:-cc}"; TDEF=MCC_TARGET_ARM64; ADIR=arm64
    QEMU=""
    SR=""
    LOADER=""
    RTANAME=arm64-libmccrt.a
    BUILDFLAGS="-O0"
    BUILDLIBS=""
    SYSINC=""
    EXTRALINK="-L/usr/lib/aarch64-linux-gnu"
    HOT=998508278240
    ;;
  *)
    echo "unsupported arch '$arch' (riscv64|arm|arm64)"; exit 77 ;;
esac

need_fallback=0
case "$arch" in
  arm|riscv64)
    command -v "$CROSSCC" >/dev/null 2>&1 || need_fallback=1
    [ -n "$SR" ] && [ ! -d "$SR" ] && need_fallback=1
    ;;
  arm64)
    case "$(uname -m)" in aarch64|arm64) : ;; *) need_fallback=1 ;; esac
    ;;
esac

if [ "$need_fallback" = 1 ]; then
  VSR="$root/vendor/gentoo-stage3-$arch-glibc"
  VMCC="$root/cmake-cross/mcc-$arch"
  [ -x "$VMCC" ] || { echo "SKIP: no '$CROSSCC' and no $VMCC to bootstrap with"; exit 77; }
  [ -d "$VSR" ] || { echo "SKIP: no '$CROSSCC' and no vendored sysroot at $VSR"; exit 77; }
  echo "[$arch] no '$CROSSCC'; bootstrapping with $VMCC against $VSR"
  CROSSCC="$VMCC"
  SR="$VSR"
  LOADER=""
  SYSINC="-I$SR/usr/include"
  EXTRALINK=""
  for d in usr/lib64 lib64 usr/lib lib; do
    [ -d "$SR/$d" ] && EXTRALINK="$EXTRALINK -L$SR/$d"
  done
  BUILDFLAGS="-O1 --sysroot=$SR $SYSINC $EXTRALINK"
  BUILDLIBS="-lm"
  case "$arch" in
    arm64) QEMU=qemu-aarch64 ;;
  esac
fi

command -v "$CROSSCC" >/dev/null 2>&1 || [ -x "$CROSSCC" ] || { echo "SKIP: no compiler '$CROSSCC'"; exit 77; }
if [ -n "$QEMU" ]; then
  command -v "$QEMU" >/dev/null 2>&1 || { echo "SKIP: $QEMU not available"; exit 77; }
else
  case "$(uname -m)" in
    aarch64|arm64) : ;;
    *) echo "SKIP: $arch is native-only; host $(uname -m) is not aarch64"; exit 77 ;;
  esac
  [ "$(uname -s)" = "Darwin" ] && { echo "SKIP: native $arch -run leg builds an ELF mcc; cross on this Mach-O host (macOS -run parity is covered by selfhost-jit)"; exit 77; }
fi
if [ -n "$SR" ]; then
  [ -d "$SR" ] || { echo "SKIP: no $arch sysroot at $SR"; exit 77; }
fi
[ -n "$LOADER" ] && { [ -f "$LOADER" ] || { echo "SKIP: no dynamic loader $LOADER"; exit 77; }; }

RTA=""
for d in "$root/cmake-dist-macos" "$root/cmake-cross" "$root/cmake-dist"; do
  if [ -f "$d/$RTANAME" ]; then RTA="$d/$RTANAME"; break; fi
done
[ -n "$RTA" ] || { echo "SKIP: no $RTANAME (build the $arch runtime)"; exit 77; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
B="$work/stage"
mkdir -p "$B"
cp "$RTA" "$B/libmccrt.a"

DEF="-DMCC_CONFIG_OPTIMIZER=1 -D$TDEF"
INC="-I$root/src -I$root/include -I$root/src/formats -I$root/src/objfmt -I$root/src/arch/$ADIR"
MCC="$work/mcc"

if [ -n "$QEMU" ]; then
  RUN="$QEMU"
  [ -n "$SR" ] && RUN="$RUN -L $SR"
  [ -n "$LOADER" ] && RUN="$RUN $LOADER"
else
  RUN=""
fi

BASEARGS="-B$B -I$root/runtime/include"
[ -n "$SR" ] && BASEARGS="$BASEARGS --sysroot=$SR"
RUNARGS="$BASEARGS $SYSINC $EXTRALINK"

echo "[$arch] building mcc from src/mcc.c with $CROSSCC ($BUILDFLAGS)"
$CROSSCC -w $DEF $INC $BUILDFLAGS -o "$MCC" "$root/src/mcc.c" $BUILDLIBS
[ -s "$MCC" ] || { echo "FAIL: $arch mcc did not build"; exit 1; }

echo "[$arch] building runmain.o"
env $KNOBS $RUN "$MCC" $BASEARGS -c "$root/runtime/lib/runmain.c" -o "$B/runmain.o"
[ -s "$B/runmain.o" ] || { echo "FAIL: $arch runmain.o did not build"; exit 1; }

cat > "$work/hi.c" <<'EOF'
extern int printf(const char*,...);
int main(void){int s=0;for(int i=0;i<5;i++)s+=i;printf("sum=%d\n",s);return 0;}
EOF
cat > "$work/hot.c" <<'EOF'
extern int printf(const char*,...);
static long work(long a,long b){long s=0;for(long i=0;i<a;i++)s+=(i*b)^(i+a);return s;}
int main(void){long acc=0;for(int k=0;k<2000;k++)acc+=work(1000,k);printf("acc=%ld\n",acc);return 0;}
EOF

run_one() {
  env $KNOBS MCC_JIT="$1" $RUN "$MCC" $RUNARGS -run "$2"
}

check() {
  n=$1; src=$2; exp=$3
  o0=$(run_one 0 "$src") || { echo "FAIL: [$arch] $n MCC_JIT=0 crashed"; exit 1; }
  o1=$(run_one 1 "$src") || { echo "FAIL: [$arch] $n MCC_JIT=1 crashed"; exit 1; }
  [ "$o0" = "$exp" ] || { echo "FAIL: [$arch] $n MCC_JIT=0 got [$o0] want [$exp]"; exit 1; }
  [ "$o1" = "$exp" ] || { echo "FAIL: [$arch] $n MCC_JIT=1 got [$o1] want [$exp]"; exit 1; }
  [ "$o0" = "$o1" ] || { echo "FAIL: [$arch] $n JIT parity broken: JIT=0 [$o0] != JIT=1 [$o1]"; exit 1; }
  echo "[$arch] $n: OK (MCC_JIT=0 == MCC_JIT=1 == '$exp')"
}

check hi  "$work/hi.c"  "sum=10"
check hot "$work/hot.c" "acc=$HOT"

echo "[$arch] -run JIT parity: OK"
