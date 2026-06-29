#!/bin/sh
# Load and RUN mcc-produced Mach-O *images* on a Linux/x86_64 host (no macOS,
# no darling). Builds the minimal Mach-O loader (tests/qemu/macho/loader.c:
# maps segments, seccomp-traps macOS syscalls, jumps to LC_MAIN), links each
# self-checking conformance program for x86_64-osx as a freestanding Mach-O
# (custom entry that exits via the macOS exit syscall), and executes it through
# the loader. Each program self-checks and must exit 0 — this loads and runs the
# actual Mach-O binary mcc emits, exercising C11 atomics/arithmetic/etc.
#
# Usage: run_macho_image.sh <src-dir> <cross-build-dir> <work-dir>
# Skips (exit 0) unless host is x86_64 with the x86_64-osx cross compiler + gcc.
set -eu

SRC=$1; XB=$2; WORK=$3
CONF="$SRC/tests/qemu/conformance"
MCC="$XB/x86_64-osx-mcc"
OSXRT="$XB/lib-x86_64-osx"

[ "$(uname -m)" = x86_64 ] || { echo "SKIP: host is not x86_64"; exit 0; }
[ -x "$MCC" ] || { echo "SKIP: no x86_64-osx-mcc"; exit 0; }
command -v gcc >/dev/null 2>&1 || { echo "SKIP: no gcc for the loader"; exit 0; }
[ -f "$OSXRT/atomic.o" ] || { echo "SKIP: no x86_64-osx runtime objects"; exit 0; }

mkdir -p "$WORK"
if ! gcc -O2 "$SRC/tests/qemu/macho/loader.c" -o "$WORK/machoload" 2>"$WORK/err.txt"; then
    echo "SKIP: cannot build Mach-O loader (no seccomp?): $(head -1 "$WORK/err.txt")"; exit 0
fi

# Freestanding Mach-O entry: call the (renamed) conformance main, exit via the
# macOS x86_64 exit syscall (BSD class 0x2000000 | 1).
cat > "$WORK/wrap.c" <<'EOF'
int cmain(void);
static void osx_exit(int c){ __asm__ volatile("movl %0,%%edi; movl $0x2000001,%%eax; syscall"
                              :: "r"(c):"eax","edi","rcx","r11"); }
int main(void){ osx_exit(cmain()); for(;;); return 0; }
void abort(void){ osx_exit(99); }
/* tiny freestanding libc the codegen/runtime may emit (aggregate init -> memset,
   struct copy -> memmove, va_arg -> memcpy, etc.). Enough to run the codegen
   conformance programs as Mach-O images; libc.c (which tests the *target* C
   library itself) genuinely needs macOS libSystem and is excluded. */
void *memset(void *d, int c, unsigned long n){ unsigned char *p=d; while(n--)*p++=(unsigned char)c; return d; }
void *memcpy(void *d, const void *s, unsigned long n){ unsigned char *a=d; const unsigned char *b=s; while(n--)*a++=*b++; return d; }
void *memmove(void *d, const void *s, unsigned long n){ unsigned char *a=d; const unsigned char *b=s;
    if(a<b){ while(n--)*a++=*b++; } else { a+=n; b+=n; while(n--)*--a=*--b; } return d; }
int memcmp(const void *x, const void *y, unsigned long n){ const unsigned char *a=x,*b=y;
    while(n--){ if(*a!=*b) return *a-*b; a++; b++; } return 0; }
unsigned long strlen(const char *s){ const char *p=s; while(*p)p++; return (unsigned long)(p-s); }
int strcmp(const char *a, const char *b){ while(*a&&*a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b; }
char *strcpy(char *d, const char *s){ char *r=d; while((*d++=*s++)); return r; }
/* bump allocator (free is a no-op) -- enough for the conformance programs */
static char heap[1<<16]; static unsigned long hp;
void *malloc(unsigned long n){ n=(n+15)&~15UL; if(hp+n>sizeof heap) return 0; void *p=heap+hp; hp+=n; return p; }
void free(void *p){ (void)p; }
/* minimal snprintf (%d %u %x %s %c %%) -- exercises mcc's Mach-O varargs codegen */
static void emit_(char *b,unsigned long n,unsigned long *i,char c){ if(*i+1<n)b[*i]=c; (*i)++; }
static void emitu_(char *b,unsigned long n,unsigned long *i,unsigned long v,int base){
    char t[24]; int k=0; if(!v)t[k++]='0'; while(v){t[k++]="0123456789abcdef"[v%base]; v/=base;}
    while(k--) emit_(b,n,i,t[k]); }
int snprintf(char *b, unsigned long n, const char *f, ...){
    __builtin_va_list ap; __builtin_va_start(ap,f); unsigned long i=0;
    for(; *f; f++){
        if(*f!='%'){ emit_(b,n,&i,*f); continue; }
        f++;
        if(*f=='d'){ int v=__builtin_va_arg(ap,int); if(v<0){emit_(b,n,&i,'-'); v=-v;} emitu_(b,n,&i,(unsigned long)v,10); }
        else if(*f=='u'){ emitu_(b,n,&i,(unsigned long)__builtin_va_arg(ap,unsigned),10); }
        else if(*f=='x'){ emitu_(b,n,&i,(unsigned long)__builtin_va_arg(ap,unsigned),16); }
        else if(*f=='s'){ const char *s=__builtin_va_arg(ap,char*); while(*s)emit_(b,n,&i,*s++); }
        else if(*f=='c'){ emit_(b,n,&i,(char)__builtin_va_arg(ap,int)); }
        else if(*f=='%'){ emit_(b,n,&i,'%'); }
    }
    if(n) b[i<n?i:n-1]=0;
    __builtin_va_end(ap);
    return (int)i;
}
EOF
"$MCC" -nostdlib -c "$WORK/wrap.c" -o "$WORK/wrap.o" 2>/dev/null

status=0
# Every conformance program runs as a Mach-O image. The libc impl is a
# freestanding shim (the real macOS libSystem needs a macOS/darling host); what
# this verifies is mcc's *codegen* for the full call surface on Mach-O -- struct
# ABI (aggregates), varargs (varargs, snprintf), and the libc-call ABI (libc).
for t in atomics control integers floats lexical aggregates varargs libc; do
    "$MCC" -nostdlib -Dmain=cmain -I"$SRC/runtime/include" -c "$CONF/$t.c" -o "$WORK/c.o" \
        2>"$WORK/err.txt" || { echo "FAIL osx/$t (compile)"; status=1; continue; }
    "$MCC" -nostdlib "$WORK/c.o" "$WORK/wrap.o" "$OSXRT/atomic.o" "$OSXRT/stdatomic.o" \
        "$OSXRT/va_list.o" "$OSXRT/builtin.o" \
        -o "$WORK/$t.macho" 2>"$WORK/err.txt" \
        || { echo "FAIL osx/$t (link): $(grep -vi 'stack\|deprecat' "$WORK/err.txt"|head -1)"; status=1; continue; }
    case "$(file -b "$WORK/$t.macho")" in
        Mach-O*) ;;
        *) echo "FAIL osx/$t: not a Mach-O"; status=1; continue;;
    esac
    if "$WORK/machoload" "$WORK/$t.macho" >/dev/null 2>&1; then
        echo "PASS osx/$t (Mach-O image loaded + executed)"
    else
        echo "FAIL osx/$t (run, rc=$?)"; status=1
    fi
    rm -f "$WORK/$t.macho"
done
exit $status
