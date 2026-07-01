












if(NOT DEFINED SRC OR NOT DEFINED XB OR NOT DEFINED WORK)
    message(FATAL_ERROR "usage: -DSRC= -DXB= -DWORK= -P run_macho_image.cmake")
endif()

set(CONF  "${SRC}/tests/qemu/conformance")
set(MCC   "${XB}/mcc-x86_64-osx")
set(OSXRT "${XB}/lib-x86_64-osx")


cmake_host_system_information(RESULT _arch QUERY OS_PLATFORM)
if(NOT _arch STREQUAL "x86_64")
    message("SKIP: host is not x86_64")
    cmake_language(EXIT 77)
endif()
if(NOT EXISTS "${MCC}")
    message("SKIP: no mcc-x86_64-osx")
    cmake_language(EXIT 77)
endif()
find_program(GCC gcc)
if(NOT GCC)
    message("SKIP: no gcc for the loader")
    cmake_language(EXIT 77)
endif()
if(NOT EXISTS "${OSXRT}/atomic.o")
    message("SKIP: no x86_64-osx runtime objects")
    cmake_language(EXIT 77)
endif()

file(MAKE_DIRECTORY "${WORK}")


execute_process(
    COMMAND "${GCC}" -O2 "${SRC}/tests/qemu/macho/loader.c" -o "${WORK}/machoload"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE  _err)
if(NOT _rc EQUAL 0)
    string(REGEX REPLACE "\r?\n.*" "" _firstline "${_err}")
    message("SKIP: cannot build Mach-O loader (no seccomp?): ${_firstline}")
    cmake_language(EXIT 77)
endif()





file(WRITE "${WORK}/wrap.c" [==[
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
]==])


execute_process(
    COMMAND "${MCC}" -nostdlib -c "${WORK}/wrap.c" -o "${WORK}/wrap.o"
    OUTPUT_QUIET ERROR_QUIET)

set(_status 0)





foreach(t atomics control integers floats lexical aggregates varargs libc)
    
    execute_process(
        COMMAND "${MCC}" -nostdlib -Dmain=cmain "-I${SRC}/runtime/include"
                -c "${CONF}/${t}.c" -o "${WORK}/c.o"
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message("FAIL osx/${t} (compile)")
        set(_status 1)
        continue()
    endif()

    
    execute_process(
        COMMAND "${MCC}" -nostdlib "${WORK}/c.o" "${WORK}/wrap.o"
                "${OSXRT}/atomic.o" "${OSXRT}/stdatomic.o"
                "${OSXRT}/va_list.o" "${OSXRT}/builtin.o"
                -o "${WORK}/${t}.macho"
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        
        set(_msg "")
        string(REPLACE "\r" "" _err "${_err}")
        string(REPLACE "\n" ";" _errlines "${_err}")
        foreach(_line IN LISTS _errlines)
            string(TOLOWER "${_line}" _low)
            if(NOT _low MATCHES "stack" AND NOT _low MATCHES "deprecat")
                set(_msg "${_line}")
                break()
            endif()
        endforeach()
        message("FAIL osx/${t} (link): ${_msg}")
        set(_status 1)
        continue()
    endif()

    
    execute_process(
        COMMAND file -b "${WORK}/${t}.macho"
        RESULT_VARIABLE _frc OUTPUT_VARIABLE _ftype ERROR_QUIET)
    string(STRIP "${_ftype}" _ftype)
    if(NOT _ftype MATCHES "^Mach-O")
        message("FAIL osx/${t}: not a Mach-O")
        set(_status 1)
        continue()
    endif()

    
    execute_process(
        COMMAND "${WORK}/machoload" "${WORK}/${t}.macho"
        RESULT_VARIABLE _rrc OUTPUT_QUIET ERROR_QUIET)
    if(_rrc EQUAL 0)
        message("PASS osx/${t} (Mach-O image loaded + executed)")
    else()
        message("FAIL osx/${t} (run, rc=${_rrc})")
        set(_status 1)
    endif()

    file(REMOVE "${WORK}/${t}.macho")
endforeach()

if(NOT _status EQUAL 0)
    cmake_language(EXIT 1)
endif()
cmake_language(EXIT 0)
