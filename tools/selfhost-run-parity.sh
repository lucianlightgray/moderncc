#!/bin/sh
# In-memory `-run` JIT-parity gate: MCC_JIT=0 (interpret/AOT-in-memory) must
# produce byte-identical output to MCC_JIT=1 (JIT) for a target-arch mcc, and
# both must match the known-good reference value.
#
#   tools/selfhost-run-parity.sh <arch> [KNOB=VAL ...]     arch: riscv64|arm|arm64
#
# This is the `-run` peer of tools/selfhost-cross-native.sh (which gates the
# 3-stage AOT self-host fixpoint). Where that script proves the mcc-built
# cross mcc reaches an object-file fixpoint, this one proves mcc's in-process
# `-run` engine is stable across the MCC_JIT=0/1 code paths -- a property that
# is easy to silently regress because the two paths share a front end but split
# at codegen/execution.
#
# Per arch it:
#   1. Builds a target-arch mcc from the amalgamation (src/mcc.c) with
#      -DMCC_CONFIG_STATIC so `-run` resolves libc/libm from mcc's baked
#      mcc_syms table (self-contained in-memory run). riscv64/arm use the
#      cross-gcc; arm64 uses the native host cc (aarch64 host only).
#   2. Stages cmake-dist-macos/<arch>-libmccrt.a as plain `libmccrt.a` in a
#      scratch -B dir and builds runmain.o there with that same mcc.
#   3. Compiles+runs two programs in-process (hi.c -> sum=10; hot.c -> an
#      arch-specific accumulator) once under MCC_JIT=0 and once under MCC_JIT=1.
#   4. Asserts each run succeeds, matches the reference value, AND that the
#      JIT=0 and JIT=1 outputs are identical. Any mismatch/crash -> exit 1.
#
# Extra KNOB=VAL args are exported into every mcc invocation so gate flags can
# be soaked, e.g.:  tools/selfhost-run-parity.sh riscv64 MCC_AST_IVSR_PTR=1
#
# Exit 77 (ctest SKIP) when a prerequisite is missing (cross-gcc / qemu /
# sysroot / libmccrt.a), so CI cells without them do not fail.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
arch=${1:-riscv64}
[ $# -gt 0 ] && shift
KNOBS="$*"

# ---- per-arch configuration -------------------------------------------------
# CROSSCC   compiler that emits the target arch
# QEMU      user-mode emulator ("" => run mcc natively, no emulation)
# LOADER    explicit dynamic loader to invoke mcc through ("" => none/static)
# SR        sysroot ("" => none; use host libs)
# BUILDLIBS libs appended AFTER src/mcc.c (order matters for static -lm)
# SYSINC    sysroot include dir passed on the `-run`/compile line
# EXTRALINK extra -L passed on the `-run` line
# HOT       expected hot.c accumulator for this arch (long width differs)
case "$arch" in
  riscv64)
    CROSSCC=riscv64-linux-gnu-gcc; TDEF=MCC_TARGET_RISCV64; ADIR=riscv64
    QEMU=qemu-riscv64
    SR="$root/vendor/gentoo-stage3-riscv64-glibc"
    LOADER="$SR/usr/lib64/ld-linux-riscv64-lp64d.so.1"
    RTANAME=riscv64-libmccrt.a
    BUILDFLAGS="-O0 -DMCC_CONFIG_STATIC"     # dynamic mcc, run via LOADER
    BUILDLIBS="-lm"
    SYSINC="-I$SR/usr/include"
    EXTRALINK=""
    HOT=998508278240
    ;;
  arm)
    CROSSCC=arm-linux-gnueabihf-gcc; TDEF=MCC_TARGET_ARM; ADIR=arm
    QEMU=qemu-arm
    SR="/usr/arm-linux-gnueabihf"
    LOADER=""                                # static mcc
    RTANAME=arm-libmccrt.a
    BUILDFLAGS="-O0 -static -DMCC_CONFIG_STATIC"
    BUILDLIBS="-lm"
    SYSINC="-I$SR/include"
    EXTRALINK=""
    HOT=2075865568                           # 32-bit long
    ;;
  arm64)
    CROSSCC="${CC:-cc}"; TDEF=MCC_TARGET_ARM64; ADIR=arm64
    QEMU=""                                   # native, no qemu
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

# ---- prerequisite checks (missing => SKIP 77, never FAIL) -------------------
command -v "$CROSSCC" >/dev/null 2>&1 || { echo "SKIP: no compiler '$CROSSCC'"; exit 77; }
if [ -n "$QEMU" ]; then
  command -v "$QEMU" >/dev/null 2>&1 || { echo "SKIP: $QEMU not available"; exit 77; }
else
  # native (arm64) leg only makes sense on an aarch64 host
  case "$(uname -m)" in
    aarch64|arm64) : ;;
    *) echo "SKIP: $arch is native-only; host $(uname -m) is not aarch64"; exit 77 ;;
  esac
  # ...and only on an ELF host: this leg builds the mcc with TDEF alone (an
  # ELF target, no MCC_TARGET_MACHO), so on a Mach-O host it is a CROSS
  # compiler and `-run` is refused (MCC_TARGET_IS_HOST needs the object format
  # to match the host -- see mcc.h). macOS arm64 `-run`/JIT parity is covered
  # separately by the selfhost-jit ctest, which builds a real Mach-O JIT mcc.
  [ "$(uname -s)" = "Darwin" ] && { echo "SKIP: native $arch -run leg builds an ELF mcc; cross on this Mach-O host (macOS -run parity is covered by selfhost-jit)"; exit 77; }
fi
if [ -n "$SR" ]; then
  [ -d "$SR" ] || { echo "SKIP: no $arch sysroot at $SR"; exit 77; }
fi
[ -n "$LOADER" ] && { [ -f "$LOADER" ] || { echo "SKIP: no dynamic loader $LOADER"; exit 77; }; }

# runtime archive: prefer the macOS dist dir, fall back to the cross build dir
RTA=""
for d in "$root/cmake-dist-macos" "$root/cmake-cross" "$root/cmake-dist"; do
  if [ -f "$d/$RTANAME" ]; then RTA="$d/$RTANAME"; break; fi
done
[ -n "$RTA" ] || { echo "SKIP: no $RTANAME (build the $arch runtime)"; exit 77; }

# ---- staging ----------------------------------------------------------------
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
B="$work/stage"
mkdir -p "$B"
cp "$RTA" "$B/libmccrt.a"

DEF="-DMCC_CONFIG_OPTIMIZER=1 -D$TDEF"
INC="-I$root/src -I$root/include -I$root/src/formats -I$root/src/objfmt -I$root/src/arch/$ADIR"
MCC="$work/mcc"

# how to invoke mcc (native, or under qemu +/- explicit loader)
if [ -n "$QEMU" ]; then
  RUN="$QEMU"
  [ -n "$SR" ] && RUN="$RUN -L $SR"
  [ -n "$LOADER" ] && RUN="$RUN $LOADER"
else
  RUN=""
fi

# common mcc args for compiling runmain.o and for -run
BASEARGS="-B$B -I$root/runtime/include"
[ -n "$SR" ] && BASEARGS="$BASEARGS --sysroot=$SR"
RUNARGS="$BASEARGS $SYSINC $EXTRALINK"

# ---- build the target-arch mcc ---------------------------------------------
echo "[$arch] building mcc from src/mcc.c with $CROSSCC ($BUILDFLAGS)"
$CROSSCC -w $DEF $INC $BUILDFLAGS -o "$MCC" "$root/src/mcc.c" $BUILDLIBS
[ -s "$MCC" ] || { echo "FAIL: $arch mcc did not build"; exit 1; }

# ---- build runmain.o with that same mcc (required by -run) ------------------
echo "[$arch] building runmain.o"
env $KNOBS $RUN "$MCC" $BASEARGS -c "$root/runtime/lib/runmain.c" -o "$B/runmain.o"
[ -s "$B/runmain.o" ] || { echo "FAIL: $arch runmain.o did not build"; exit 1; }

# ---- test programs ----------------------------------------------------------
cat > "$work/hi.c" <<'EOF'
extern int printf(const char*,...);
int main(void){int s=0;for(int i=0;i<5;i++)s+=i;printf("sum=%d\n",s);return 0;}
EOF
cat > "$work/hot.c" <<'EOF'
extern int printf(const char*,...);
static long work(long a,long b){long s=0;for(long i=0;i<a;i++)s+=(i*b)^(i+a);return s;}
int main(void){long acc=0;for(int k=0;k<2000;k++)acc+=work(1000,k);printf("acc=%ld\n",acc);return 0;}
EOF

# run one program in-process at a given MCC_JIT setting; echo its stdout
run_one() { # $1=MCC_JIT  $2=source
  env $KNOBS MCC_JIT="$1" $RUN "$MCC" $RUNARGS -run "$2"
}

# assert: JIT=0 ok, JIT=1 ok, both == expected, and JIT=0 == JIT=1
check() { # $1=name  $2=source  $3=expected
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
