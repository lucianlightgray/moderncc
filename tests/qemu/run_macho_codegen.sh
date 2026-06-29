#!/bin/sh
# Runtime-verify mcc's Mach-O (Darwin) *code generation* without a macOS host.
# A full Mach-O executable can't be loaded on Linux (needs macOS/darling), but
# mcc's osx `-c` emits the Darwin-target machine code in an ELF object, and the
# x86_64 SysV ABI is identical between Darwin and Linux. So on an x86_64 host we
# link each self-checking conformance program (compiled for x86_64-osx) into a
# Linux ELF via:
#   * a trampoline shim mapping Darwin _-mangled libc names to the host libc, and
#   * mcc's own osx runtime objects (the Darwin-mangled __atomic_*/__va_arg), and
# then run it. Each program self-checks and must exit 0 — this executes the exact
# machine code mcc generates for Darwin. (The Mach-O *container* is validated
# separately by validate_macho.sh.)
#
# Usage: run_macho_codegen.sh <src-dir> <cross-build-dir> <work-dir>
# Skips (exit 0) unless the host is x86_64 with the x86_64-osx cross compiler.
set -eu

SRC=$1; XB=$2; WORK=$3
CONF="$SRC/tests/qemu/conformance"
MCC="$XB/x86_64-osx-mcc"

[ "$(uname -m)" = x86_64 ] || { echo "SKIP: host is not x86_64"; exit 0; }
[ -x "$MCC" ] || { echo "SKIP: no x86_64-osx-mcc"; exit 0; }
command -v gcc >/dev/null 2>&1 || { echo "SKIP: no gcc to build the ELF harness"; exit 0; }
OSXRT="$XB/lib-x86_64-osx"
[ -f "$OSXRT/atomic.o" ] || { echo "SKIP: no x86_64-osx runtime objects"; exit 0; }

mkdir -p "$WORK"
# harness: real entry calls the Darwin-mangled _main of the osx-compiled program
cat > "$WORK/harness.c" <<'EOF'
extern int osx_main(void) __asm__("_main");
int main(void){ return osx_main(); }
EOF
# trampolines: Darwin _name -> host libc name (tail call through the PLT)
cat > "$WORK/shim.S" <<'EOF'
.macro tramp dar, nat
.globl \dar
\dar: jmp \nat@PLT
.endm
.text
tramp _memset, memset
tramp _memcpy, memcpy
tramp _memmove, memmove
tramp _memcmp, memcmp
tramp _malloc, malloc
tramp _calloc, calloc
tramp _realloc, realloc
tramp _free, free
tramp _printf, printf
tramp _snprintf, snprintf
tramp _strcmp, strcmp
tramp _strncmp, strncmp
tramp _strcpy, strcpy
tramp _strlen, strlen
tramp _abort, abort
.section .note.GNU-stack,"",@progbits
EOF

status=0
for f in "$CONF"/*.c; do
    n=$(basename "$f" .c)
    # libc.c is included: the trampoline shim forwards the Darwin _-mangled libc
    # names to the *real host glibc* (malloc/snprintf/str*/mem* are genuine
    # glibc), so this exercises the osx-target codegen against an actual C
    # library, not a synthetic one.
    if ! "$MCC" -I"$SRC/runtime/include" -c "$f" -o "$WORK/o.o" 2>"$WORK/err.txt"; then
        echo "FAIL osx/$n (compile): $(head -1 "$WORK/err.txt")"; status=1; continue
    fi
    if ! gcc "$WORK/harness.c" "$WORK/o.o" "$WORK/shim.S" \
             "$OSXRT/atomic.o" "$OSXRT/stdatomic.o" "$OSXRT/va_list.o" "$OSXRT/builtin.o" \
             -o "$WORK/run" 2>"$WORK/err.txt"; then
        echo "FAIL osx/$n (link): $(grep -vi 'stack\|deprecat' "$WORK/err.txt" | head -1)"; status=1; continue
    fi
    if "$WORK/run" >/dev/null 2>&1; then
        echo "PASS osx/$n (x86_64-osx codegen executed)"
    else
        echo "FAIL osx/$n (run, rc=$?)"; status=1
    fi
    rm -f "$WORK/run"
done
exit $status
