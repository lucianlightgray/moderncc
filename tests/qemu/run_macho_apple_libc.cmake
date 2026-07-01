



























if(NOT DEFINED SRC OR NOT DEFINED XB OR NOT DEFINED WORK)
    message(FATAL_ERROR "usage: cmake -DSRC=<src> -DXB=<cross-build> -DWORK=<work> -P run_macho_apple_libc.cmake")
endif()

set(AL    "${SRC}/tests/qemu/apple-libc")
set(MCC   "${XB}/mcc-x86_64-osx")
set(OSXRT "${XB}/lib-x86_64-osx")        




cmake_host_system_information(RESULT _hostarch QUERY OS_PLATFORM)
if(NOT _hostarch STREQUAL "x86_64")
    message("SKIP: host is not x86_64")
    cmake_language(EXIT 77)
endif()
if(NOT EXISTS "${MCC}")
    message("SKIP: no mcc-x86_64-osx")
    cmake_language(EXIT 77)
endif()
find_program(GCC NAMES gcc)
if(NOT GCC)
    message("SKIP: no gcc for the loader")
    cmake_language(EXIT 77)
endif()
if(NOT EXISTS "${AL}/src/strcspn.c")
    message("SKIP: vendored Apple sources absent")
    cmake_language(EXIT 77)
endif()

file(MAKE_DIRECTORY "${WORK}")


execute_process(
    COMMAND "${GCC}" -O2 "${SRC}/tests/qemu/macho/loader.c" -o "${WORK}/machoload"
    RESULT_VARIABLE _lrc
    OUTPUT_QUIET
    ERROR_VARIABLE  _lerr)
if(NOT _lrc EQUAL 0)
    string(REGEX REPLACE "\n.*" "" _lfirst "${_lerr}")
    message("SKIP: cannot build Mach-O loader (no seccomp?): ${_lfirst}")
    cmake_language(EXIT 77)
endif()








file(WRITE "${WORK}/wrap.c" "typedef unsigned long size_t;
int cmain(void);
static void osx_exit(int c){ __asm__ volatile(\"movl %0,%%edi; movl $0x2000001,%%eax; syscall\"
                            :: \"r\"(c):\"eax\",\"edi\",\"rcx\",\"r11\"); }
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
")

set(CFLAGS -nostdlib "-I${AL}/shim-include")

set(RTOBJS "")
foreach(o va_list builtin)
    if(EXISTS "${OSXRT}/${o}.o")
        list(APPEND RTOBJS "${OSXRT}/${o}.o")
    endif()
endforeach()

set(status 0)




set(OBJS "")
foreach(dir "${AL}/src" "${AL}/src-libplatform" "${AL}/src-simple")
    file(GLOB _cs "${dir}/*.c")
    list(SORT _cs)
    foreach(f ${_cs})
        get_filename_component(n "${f}" NAME_WE)
        execute_process(
            COMMAND "${MCC}" ${CFLAGS} -c "${f}" -o "${WORK}/o_${n}.o"
            RESULT_VARIABLE _crc
            OUTPUT_QUIET
            ERROR_VARIABLE  _cerr)
        if(NOT _crc EQUAL 0)
            string(REGEX REPLACE "\n.*" "" _cfirst "${_cerr}")
            message("FAIL apple-libc/${n} (compile): ${_cfirst}")
            set(status 1)
            break()
        endif()
        list(APPEND OBJS "${WORK}/o_${n}.o")
    endforeach()
endforeach()

if(NOT status EQUAL 0)
    cmake_language(EXIT 1)
endif()

execute_process(
    COMMAND "${MCC}" ${CFLAGS} -c "${WORK}/wrap.c" -o "${WORK}/wrap.o"
    RESULT_VARIABLE _wrc
    OUTPUT_QUIET
    ERROR_VARIABLE  _werr)
if(NOT _wrc EQUAL 0)
    string(REGEX REPLACE "\n.*" "" _wfirst "${_werr}")
    message("FAIL apple-libc (wrap compile): ${_wfirst}")
    cmake_language(EXIT 1)
endif()




function(run_image src label)
    execute_process(
        COMMAND "${MCC}" ${CFLAGS} -Dmain=cmain -c "${src}" -o "${WORK}/test.o"
        RESULT_VARIABLE _trc
        OUTPUT_QUIET
        ERROR_VARIABLE  _terr)
    if(NOT _trc EQUAL 0)
        string(REGEX REPLACE "\n.*" "" _tfirst "${_terr}")
        message("FAIL ${label} (test compile): ${_tfirst}")
        set(status 1 PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND "${MCC}" -nostdlib "${WORK}/test.o" ${OBJS} "${WORK}/wrap.o" ${RTOBJS}
                -o "${WORK}/${label}.macho"
        RESULT_VARIABLE _krc
        OUTPUT_QUIET
        ERROR_VARIABLE  _kerr)
    if(NOT _krc EQUAL 0)
        
        string(REPLACE "\n" ";" _klines "${_kerr}")
        set(_kfirst "")
        foreach(_line ${_klines})
            string(TOLOWER "${_line}" _lc)
            if(NOT _lc MATCHES "stack" AND NOT _lc MATCHES "deprecat")
                set(_kfirst "${_line}")
                break()
            endif()
        endforeach()
        message("FAIL ${label} (link): ${_kfirst}")
        set(status 1 PARENT_SCOPE)
        return()
    endif()

    
    file(READ "${WORK}/${label}.macho" _magic LIMIT 4 HEX)
    set(_machomagics cffaedfe cefaedfe feedfacf feedface cafebabe bebafeca)
    if(NOT _magic IN_LIST _machomagics)
        message("FAIL ${label}: not a Mach-O image")
        set(status 1 PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND "${WORK}/machoload" "${WORK}/${label}.macho"
        RESULT_VARIABLE _rrc
        OUTPUT_QUIET ERROR_QUIET)
    if(_rrc EQUAL 0)
        message("PASS ${label} (Apple's genuine libc executed as a Mach-O image)")
    else()
        message("FAIL ${label} (run, rc=${_rrc} -- maps to the test's return code)")
        set(status 1 PARENT_SCOPE)
    endif()

    file(REMOVE "${WORK}/${label}.macho")
endfunction()

run_image("${AL}/apple_string_conf.c"      apple-libc-freebsd)
run_image("${AL}/apple_libplatform_conf.c" apple-libc-libplatform)
run_image("${AL}/apple_simple_conf.c"      apple-libc-simple-printf)

if(NOT status EQUAL 0)
    cmake_language(EXIT 1)
endif()
cmake_language(EXIT 0)
