#!/bin/sh
# Run Apple's *genuine* libc sources through mcc's Mach-O codegen on a
# Linux/x86_64 host. Two vendored, verbatim source sets (see
# tests/qemu/apple-libc/PROVENANCE.md):
#   * src/             -- Apple Libc string/FreeBSD/*.c (strcspn/strpbrk/strsep/
#                         memmem/strchrnul/strnstr)
#   * src-libplatform/ -- Apple libplatform src/string/generic/*.c, the core
#                         string/memory routines (strlen/strcmp/strncmp/strcpy/
#                         strncpy/strlcpy/strlcat/strchr/strstr/strnlen/memmove/
#                         memcpy/memcmp/memchr/memccpy/bzero/memset/memset_pattern)
#                         -- the very functions that ship as commpage assembly on
#                         macOS; the asm is only an optimization variant, Apple
#                         ships this portable C as the functional equivalent.
# Both sets are compiled by mcc for x86_64 Darwin and linked TOGETHER into a
# Mach-O image with only a freestanding entry (no hand-written libc): the
# FreeBSD funcs resolve strlen/memcmp/memchr/strncmp against Apple's real
# libplatform code, so EVERY string/memory function under test is genuine Apple
# source. Each self-checking image must exit 0, executed via the in-repo loader.
#
# This is the closest achievable approximation off-macOS to testing Mach-O
# against its real target libc. The remainder of libSystem that is genuinely
# kernel-fused (Mach-VM malloc, Darwin stdio/FILE, dyld) needs a macOS/darling
# host; PROVENANCE.md documents that boundary.
#
# Usage: run_macho_apple_libc.sh <src-dir> <cross-build-dir> <work-dir>
# Self-skips (exit 77, ctest SKIP_RETURN_CODE) unless host is x86_64 with the
# x86_64-osx cross compiler + gcc.
set -eu

SRC=$1; XB=$2; WORK=$3
AL="$SRC/tests/qemu/apple-libc"
MCC="$XB/x86_64-osx-mcc"
OSXRT="$XB/lib-x86_64-osx"        # mcc's own Darwin runtime (__va_arg etc.)

[ "$(uname -m)" = x86_64 ] || { echo "SKIP: host is not x86_64"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no x86_64-osx-mcc"; exit 77; }
command -v gcc >/dev/null 2>&1 || { echo "SKIP: no gcc for the loader"; exit 77; }
[ -f "$AL/src/strcspn.c" ] || { echo "SKIP: vendored Apple sources absent"; exit 77; }

mkdir -p "$WORK"
if ! gcc -O2 "$SRC/tests/qemu/macho/loader.c" -o "$WORK/machoload" 2>"$WORK/err.txt"; then
    echo "SKIP: cannot build Mach-O loader (no seccomp?): $(head -1 "$WORK/err.txt")"; exit 77
fi

# Freestanding entry only -- NO hand-written libc. Every str*/mem*/format symbol
# is satisfied by Apple's vendored sources. The only extra definitions are stubs
# for the Darwin kernel vm/io primitives that string_io.c references on its
# growing-string paths (vm_allocate/vm_deallocate/mach_task_self/write/errno) --
# the fixed-buffer _simple_vsnprintf under test never calls them; they exist
# only to satisfy the linker. This is the kernel's role (page supply / byte
# sink), which on a non-Darwin host is legitimately not provided.
cat > "$WORK/wrap.c" <<'EOF'
typedef unsigned long size_t;
int cmain(void);
static void osx_exit(int c){ __asm__ volatile("movl %0,%%edi; movl $0x2000001,%%eax; syscall"
                            :: "r"(c):"eax","edi","rcx","r11"); }
int main(void){ osx_exit(cmain()); for(;;); return 0; }
void abort(void){ osx_exit(99); }
/* unused Darwin kernel primitives (never reached on the _simple_vsnprintf path) */
int errno;
long write(int fd, const void *p, size_t n){ (void)fd;(void)p;(void)n; return -1; }
int  vm_allocate(unsigned int t, unsigned long *a, unsigned long s, int f){
    (void)t;(void)a;(void)s;(void)f; return -1; }
int  vm_deallocate(unsigned int t, unsigned long a, unsigned long s){
    (void)t;(void)a;(void)s; return -1; }
unsigned int mach_task_self(void){ return 0; }
EOF

CFLAGS="-nostdlib -I$AL/shim-include"
# mcc's own osx runtime: __va_arg (varargs) is needed by string_io.c's printf.
RTOBJS=""
for o in va_list builtin; do
    [ -f "$OSXRT/$o.o" ] && RTOBJS="$RTOBJS $OSXRT/$o.o"
done
status=0

compile_set() {  # $1 = glob dir ; appends objects to $OBJS
    for f in "$1"/*.c; do
        n=$(basename "$f" .c)
        if ! "$MCC" $CFLAGS -c "$f" -o "$WORK/o_$n.o" 2>"$WORK/err.txt"; then
            echo "FAIL apple-libc/$n (compile): $(head -1 "$WORK/err.txt")"; status=1; return 1
        fi
        OBJS="$OBJS $WORK/o_$n.o"
    done
    return 0
}

OBJS=""
compile_set "$AL/src" || true
compile_set "$AL/src-libplatform" || true
compile_set "$AL/src-simple" || true
[ $status -eq 0 ] || exit 1
if ! "$MCC" $CFLAGS -c "$WORK/wrap.c" -o "$WORK/wrap.o" 2>"$WORK/err.txt"; then
    echo "FAIL apple-libc (wrap compile): $(head -1 "$WORK/err.txt")"; exit 1
fi

run_image() {  # $1 = test source (its main -> cmain) ; $2 = label
    src=$1; label=$2
    if ! "$MCC" $CFLAGS -Dmain=cmain -c "$src" -o "$WORK/test.o" 2>"$WORK/err.txt"; then
        echo "FAIL $label (test compile): $(head -1 "$WORK/err.txt")"; status=1; return
    fi
    if ! "$MCC" -nostdlib "$WORK/test.o" $OBJS "$WORK/wrap.o" $RTOBJS -o "$WORK/$label.macho" 2>"$WORK/err.txt"; then
        echo "FAIL $label (link): $(grep -vi 'stack\|deprecat' "$WORK/err.txt"|head -1)"; status=1; return
    fi
    case "$(file -b "$WORK/$label.macho")" in
        Mach-O*) ;;
        *) echo "FAIL $label: not a Mach-O image"; status=1; return;;
    esac
    if "$WORK/machoload" "$WORK/$label.macho" >/dev/null 2>&1; then
        echo "PASS $label (Apple's genuine libc executed as a Mach-O image)"
    else
        rc=$?
        echo "FAIL $label (run, rc=$rc -- maps to the test's return code)"; status=1
    fi
    rm -f "$WORK/$label.macho"
}

run_image "$AL/apple_string_conf.c"      apple-libc-freebsd
run_image "$AL/apple_libplatform_conf.c" apple-libc-libplatform
run_image "$AL/apple_simple_conf.c"      apple-libc-simple-printf

exit $status
