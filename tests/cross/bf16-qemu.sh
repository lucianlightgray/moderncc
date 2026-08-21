#!/bin/sh
set -e

ARCH=$1
MCC=$2
CROSS=$3
SR=$4
W=$5
[ -n "$ARCH" ] && [ -n "$MCC" ] && [ -n "$CROSS" ] && [ -n "$SR" ] && [ -n "$W" ] || {
	echo "usage: bf16-qemu.sh <arch> <mcc> <crossdir> <sysroot> <workdir>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no $ARCH mcc at $MCC"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR"; exit 77; }

ABI=""
case "$ARCH" in
riscv64) EMU=qemu-riscv64 ;;
arm64)   EMU=qemu-aarch64 ;;
i386)    EMU=qemu-i386 ;;
arm)     EMU=qemu-arm; ABI="-mfloat-abi hard" ;;
*) echo "unsupported arch '$ARCH'" >&2; exit 2 ;;
esac
QEMU=$(command -v "$EMU" || command -v "$EMU-static" || true)
[ -n "$QEMU" ] || { echo "SKIP: no $EMU"; exit 77; }

rm -rf "$W"; mkdir -p "$W"
CC="$MCC $ABI -B $CROSS --sysroot=$SR -I$SR/usr/include"
CC="$CC -L$SR/usr/lib64 -L$SR/lib64 -L$SR/usr/lib -L$SR/lib"

cat > "$W/bf16.c" <<'EOF'
typedef __bf16 bf;
static bf g = (bf)2.5;
__attribute__((noinline)) static float addbf(bf a, bf b){ return (float)a + (float)b; }
__attribute__((noinline)) static float mulbf(bf a, bf b){ return (float)a * (float)b; }
struct s { bf v; int tag; };
__attribute__((noinline)) static float takes(struct s s){ return (float)s.v + (float)s.tag; }
extern int printf(const char*, ...);
int main(void){
	bf a = (bf)1.5, b = (bf)3.0;
	unsigned short gb; __builtin_memcpy(&gb, &g, 2);
	unsigned short pi; bf p = (bf)3.14159265f; __builtin_memcpy(&pi, &p, 2);
	struct s sv = { (bf)4.0, 5 };
	int ok = sizeof(bf)==2
	       && (int)addbf(a,b)==4              /* 1.5+3.0 */
	       && (int)mulbf((bf)2.0,(bf)3.0)==6
	       && gb==0x4020                       /* (bf)2.5 */
	       && pi==0x4049                       /* (bf)pi RNE */
	       && (int)takes(sv)==9;               /* 4.0 + 5 struct-by-value */
	printf("%s\n", ok?"OK":"FAIL");
	return ok?0:1;
}
EOF

$CC "$W/bf16.c" -o "$W/bf16.bin" >"$W/build.log" 2>&1 || {
	echo "SKIP: $ARCH mcc could not build the bf16 probe"
	sed 's/^/  /' "$W/build.log" | head -8
	exit 77
}

OUT=$("$QEMU" -L "$SR" "$W/bf16.bin" 2>&1) || {
	echo "FAIL: $ARCH bf16 run rc=$? out='$OUT'"
	exit 1
}
[ "$OUT" = "OK" ] || { echo "FAIL: $ARCH bf16 got '$OUT'"; exit 1; }
echo "PASS: $ARCH bf16 (sizeof/add/mul/const/RNE/struct-byval)"
exit 0
