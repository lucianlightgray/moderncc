#!/bin/sh
# Runtime-verify mcc's Mach-O (Darwin) *code generation* without a macOS host.
# A full Mach-O executable can't be loaded on Linux (needs macOS/darling), but
# mcc's osx `-c` emits the Darwin-target machine code in an ELF object, and the
# Darwin/Linux base ABI is identical per arch (SysV on x86_64, AAPCS64 on arm64).
# So we link each self-checking conformance program (compiled for <arch>-osx)
# into a Linux ELF via:
#   * a trampoline shim mapping Darwin _-mangled libc names to the host libc, and
#   * mcc's own osx runtime objects (the Darwin-mangled __atomic_*/__va_arg), and
# then run it. Each program self-checks and must exit 0 — this executes the exact
# machine code mcc generates for Darwin. (The Mach-O *container* is validated
# separately by validate_macho.sh.)
#
# Two execution modes, selected by <arch>:
#   x86_64  the host is x86_64, so the Darwin object runs natively (default).
#   arm64   the object is aarch64; build/link with clang --target=aarch64 + lld
#           against an arm64 glibc sysroot and run under qemu-aarch64. This is
#           the darwin/macho row of the qemu matrix.
#
# Usage: run_macho_codegen.sh <src-dir> <cross-build-dir> <work-dir> [arch] [sysroot]
# Self-skips (exit 77, ctest SKIP_RETURN_CODE) when the toolchain for the
# requested arch is unavailable.
set -eu

SRC=$1; XB=$2; WORK=$3; ARCH=${4:-x86_64}; SYSROOT=${5:-}
CONF="$SRC/tests/qemu/conformance"

command -v gcc >/dev/null 2>&1 || { echo "SKIP: no gcc to build the ELF harness"; exit 77; }

case "$ARCH" in
x86_64)
    MCC="$XB/x86_64-osx-mcc"; OSXRT="$XB/lib-x86_64-osx"
    [ "$(uname -m)" = x86_64 ] || { echo "SKIP: host is not x86_64"; exit 77; }
    [ -x "$MCC" ] || { echo "SKIP: no x86_64-osx-mcc"; exit 77; }
    [ -f "$OSXRT/atomic.o" ] || { echo "SKIP: no x86_64-osx runtime objects"; exit 77; }
    # native link + run; SysV varargs match Darwin's, so the full set runs.
    RUNTIME="$OSXRT/atomic.o $OSXRT/stdatomic.o $OSXRT/va_list.o $OSXRT/builtin.o"
    # tls/tls_aggr now use native Mach-O TLV descriptors (a thunk call into
    # libSystem's __tlv_bootstrap), which the Linux ELF shortcut can't provide —
    # so they segfault here even though the codegen is correct. (Previously the
    # Darwin local-exec %fs relocation happened to also work on the Linux host.)
    # Skipped on this path like arm64; covered on real Darwin (arm64 native +
    # x86_64-osx via Rosetta) per the §6.7.1 TLS work.
    SKIP_PROGS="tls tls_aggr"
    BR=jmp; PLT='@PLT'
    ;;
arm64)
    MCC="$XB/arm64-osx-mcc"; OSXRT="$XB/lib-arm64-osx"
    [ -x "$MCC" ] || { echo "SKIP: no arm64-osx-mcc"; exit 77; }
    [ -f "$OSXRT/atomic.o" ] || { echo "SKIP: no arm64-osx runtime objects"; exit 77; }
    command -v clang >/dev/null 2>&1 || { echo "SKIP: no clang for the aarch64 link"; exit 77; }
    clang -print-targets 2>/dev/null | grep -qw aarch64 || { echo "SKIP: clang lacks the aarch64 target"; exit 77; }
    command -v qemu-aarch64 >/dev/null 2>&1 || { echo "SKIP: no qemu-aarch64"; exit 77; }
    [ -n "$SYSROOT" ] && [ -d "$SYSROOT" ] || { echo "SKIP: no arm64 glibc sysroot ($SYSROOT)"; exit 77; }
    RUNTIME="$OSXRT/atomic.o $OSXRT/stdatomic.o $OSXRT/builtin.o $OSXRT/lib-arm64.o"
    # This row exercises arm64-osx *codegen* under qemu. Skipped here (NOT mcc
    # codegen bugs; covered on the x86_64-osx row where target==host ABI/headers):
    #  - tls/tls_aggr: emit a Darwin TLS relocation the ELF linker can't process.
    #  - the host-libc programs (control_libc/floats_libc/libc/libc_struct/
    #    varargs_fp): they exercise glibc *through* Darwin-ABI codegen, which the
    #    ELF/glibc shortcut can't bridge portably — Darwin passes varargs on the
    #    stack (glibc/AAPCS64 reads registers) and mcc's Darwin va_list isn't
    #    glibc-binary-compatible (printf/snprintf), and they pull the host's
    #    <setjmp.h>/<stdlib.h>, whose layout/macros vary across distros (the
    #    Gentoo vs Debian-container split). On real macOS these bind to libSystem.
    SKIP_PROGS="tls tls_aggr control_libc floats_libc libc libc_struct varargs_fp"
    BR=b; PLT=''
    ;;
*) echo "SKIP: unknown arch '$ARCH'"; exit 77 ;;
esac
# _Complex multiply/divide helpers (__mcc_cmul/cdiv) — link when present.
[ -f "$OSXRT/complex.o" ] && RUNTIME="$RUNTIME $OSXRT/complex.o"

mkdir -p "$WORK"
# harness: real entry calls the Darwin-mangled _main of the osx-compiled program
cat > "$WORK/harness.c" <<'EOF'
extern int osx_main(void) __asm__("_main");
int main(void){ return osx_main(); }
EOF
# trampolines: Darwin _name -> host libc name (tail call/branch through the PLT)
{
  printf '.text\n.macro tramp dar, nat\n.globl \\dar\n\\dar: %s \\nat%s\n.endm\n' "$BR" "$PLT"
  for p in memset memcpy memmove memcmp malloc calloc realloc free printf snprintf \
           strcmp strncmp strcpy strlen abort qsort strtod strtold div ldiv lldiv; do
      echo "tramp _$p, $p"
  done
  echo 'tramp __setjmp, _setjmp'
  echo '.section .note.GNU-stack,"",@progbits'
} > "$WORK/shim.S"

status=0
for f in "$CONF"/*.c; do
    n=$(basename "$f" .c)
    case " $SKIP_PROGS " in *" $n "*) echo "SKIP osx-$ARCH/$n (libc-variadic/TLS not ELF-linkable; covered on x86_64-osx)"; continue;; esac
    # libc.c is included: the trampoline shim forwards the Darwin _-mangled libc
    # names to the *real host glibc*, so this exercises the osx-target codegen
    # against an actual C library, not a synthetic one.
    if ! "$MCC" -I"$SRC/runtime/include" -c "$f" -o "$WORK/o.o" 2>"$WORK/err.txt"; then
        echo "FAIL osx-$ARCH/$n (compile): $(head -1 "$WORK/err.txt")"; status=1; continue
    fi
    if [ "$ARCH" = x86_64 ]; then
        link() { gcc "$WORK/harness.c" "$WORK/o.o" "$WORK/shim.S" $RUNTIME -o "$WORK/run" 2>"$WORK/err.txt"; }
        run()  { "$WORK/run" >/dev/null 2>&1; }
    else
        link() { clang --target=aarch64-linux-gnu --sysroot="$SYSROOT" -fuse-ld=lld \
                     -isystem "$SYSROOT/usr/include" "$WORK/harness.c" "$WORK/shim.S" "$WORK/o.o" $RUNTIME \
                     -L"$SYSROOT/usr/lib" -L"$SYSROOT/lib" -L"$SYSROOT/usr/lib64" -L"$SYSROOT/lib64" \
                     -o "$WORK/run" 2>"$WORK/err.txt"; }
        run()  { qemu-aarch64 -L "$SYSROOT" "$WORK/run" >/dev/null 2>&1; }
    fi
    if ! link; then
        echo "FAIL osx-$ARCH/$n (link): $(grep -iE 'undefined reference|undefined symbol|error:' "$WORK/err.txt" | head -1)"; status=1; continue
    fi
    if run; then
        echo "PASS osx-$ARCH/$n ($ARCH-osx codegen executed)"
    else
        echo "FAIL osx-$ARCH/$n (run, rc=$?)"; status=1
    fi
    rm -f "$WORK/run"
done
exit $status
