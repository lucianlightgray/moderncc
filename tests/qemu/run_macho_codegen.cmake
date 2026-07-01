











cmake_minimum_required(VERSION 3.20)


if(NOT DEFINED SRC OR NOT DEFINED XB OR NOT DEFINED WORK)
    message(FATAL_ERROR "run_macho_codegen.cmake: SRC, XB and WORK are required")
endif()
if(NOT DEFINED ARCH OR ARCH STREQUAL "")
    set(ARCH "x86_64")
endif()
if(NOT DEFINED SYSROOT)
    set(SYSROOT "")
endif()
set(CONF "${SRC}/tests/qemu/conformance")


find_program(GCC NAMES gcc)
if(NOT GCC)
    message("SKIP: no gcc to build the ELF harness")
    cmake_language(EXIT 77)
endif()


if(ARCH STREQUAL "x86_64")
    set(MCC "${XB}/x86_64-osx-mcc")
    set(OSXRT "${XB}/lib-x86_64-osx")
    execute_process(COMMAND uname -m OUTPUT_VARIABLE _uname_m
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _uname_m STREQUAL "x86_64")
        message("SKIP: host is not x86_64")
        cmake_language(EXIT 77)
    endif()
    if(NOT EXISTS "${MCC}")
        message("SKIP: no x86_64-osx-mcc")
        cmake_language(EXIT 77)
    endif()
    if(NOT EXISTS "${OSXRT}/atomic.o")
        message("SKIP: no x86_64-osx runtime objects")
        cmake_language(EXIT 77)
    endif()
    
    set(RUNTIME "${OSXRT}/atomic.o" "${OSXRT}/stdatomic.o"
                "${OSXRT}/va_list.o" "${OSXRT}/builtin.o")
    
    
    set(SKIP_PROGS tls tls_aggr)
    set(BR "jmp")
    set(PLT "@PLT")
elseif(ARCH STREQUAL "arm64")
    set(MCC "${XB}/arm64-osx-mcc")
    set(OSXRT "${XB}/lib-arm64-osx")
    if(NOT EXISTS "${MCC}")
        message("SKIP: no arm64-osx-mcc")
        cmake_language(EXIT 77)
    endif()
    if(NOT EXISTS "${OSXRT}/atomic.o")
        message("SKIP: no arm64-osx runtime objects")
        cmake_language(EXIT 77)
    endif()
    find_program(CLANG NAMES clang)
    if(NOT CLANG)
        message("SKIP: no clang for the aarch64 link")
        cmake_language(EXIT 77)
    endif()
    execute_process(COMMAND "${CLANG}" -print-targets
                    OUTPUT_VARIABLE _clang_targets
                    ERROR_QUIET)
    if(NOT _clang_targets MATCHES "(^|[^A-Za-z0-9_])aarch64([^A-Za-z0-9_]|$)")
        message("SKIP: clang lacks the aarch64 target")
        cmake_language(EXIT 77)
    endif()
    find_program(QEMU NAMES qemu-aarch64)
    if(NOT QEMU)
        message("SKIP: no qemu-aarch64")
        cmake_language(EXIT 77)
    endif()
    if(SYSROOT STREQUAL "" OR NOT IS_DIRECTORY "${SYSROOT}")
        message("SKIP: no arm64 glibc sysroot (${SYSROOT})")
        cmake_language(EXIT 77)
    endif()
    set(RUNTIME "${OSXRT}/atomic.o" "${OSXRT}/stdatomic.o"
                "${OSXRT}/builtin.o" "${OSXRT}/lib-arm64.o")
    
    
    
    
    set(SKIP_PROGS tls tls_aggr control_libc floats_libc libc libc_struct varargs_fp)
    set(BR "b")
    set(PLT "")
else()
    message("SKIP: unknown arch '${ARCH}'")
    cmake_language(EXIT 77)
endif()


if(EXISTS "${OSXRT}/complex.o")
    list(APPEND RUNTIME "${OSXRT}/complex.o")
endif()

file(MAKE_DIRECTORY "${WORK}")


file(WRITE "${WORK}/harness.c"
"extern int osx_main(void) __asm__(\"_main\");\nint main(void){ return osx_main(); }\n")


set(_shim "")
string(APPEND _shim ".text\n")
string(APPEND _shim ".macro tramp dar, nat\n")
string(APPEND _shim ".globl \\dar\n")
string(APPEND _shim "\\dar: ${BR} \\nat${PLT}\n")
string(APPEND _shim ".endm\n")
set(_libcnames
    memset memcpy memmove memcmp malloc calloc realloc free printf snprintf
    strcmp strncmp strcpy strlen abort qsort strtod strtold div ldiv lldiv)
foreach(p IN LISTS _libcnames)
    string(APPEND _shim "tramp _${p}, ${p}\n")
endforeach()
string(APPEND _shim "tramp __setjmp, _setjmp\n")
string(APPEND _shim ".section .note.GNU-stack,\"\",@progbits\n")
file(WRITE "${WORK}/shim.S" "${_shim}")


file(GLOB _progs "${CONF}/*.c")
list(SORT _progs)

set(_status 0)
foreach(f IN LISTS _progs)
    get_filename_component(n "${f}" NAME_WE)
    if(n IN_LIST SKIP_PROGS)
        message("SKIP osx-${ARCH}/${n} (libc-variadic/TLS not ELF-linkable; covered on x86_64-osx)")
        continue()
    endif()

    
    execute_process(
        COMMAND "${MCC}" "-I${SRC}/runtime/include" -c "${f}" -o "${WORK}/o.o"
        RESULT_VARIABLE _rc ERROR_VARIABLE _err OUTPUT_QUIET)
    if(NOT _rc EQUAL 0)
        string(REGEX MATCH "^[^\n]*" _firstline "${_err}")
        message("FAIL osx-${ARCH}/${n} (compile): ${_firstline}")
        set(_status 1)
        continue()
    endif()

    
    if(ARCH STREQUAL "x86_64")
        execute_process(
            COMMAND "${GCC}" "${WORK}/harness.c" "${WORK}/o.o" "${WORK}/shim.S"
                    ${RUNTIME} -o "${WORK}/run"
            RESULT_VARIABLE _rc ERROR_VARIABLE _err OUTPUT_QUIET)
    else()
        execute_process(
            COMMAND "${CLANG}" --target=aarch64-linux-gnu "--sysroot=${SYSROOT}"
                    -fuse-ld=lld -isystem "${SYSROOT}/usr/include"
                    "${WORK}/harness.c" "${WORK}/shim.S" "${WORK}/o.o" ${RUNTIME}
                    "-L${SYSROOT}/usr/lib" "-L${SYSROOT}/lib"
                    "-L${SYSROOT}/usr/lib64" "-L${SYSROOT}/lib64"
                    -o "${WORK}/run"
            RESULT_VARIABLE _rc ERROR_VARIABLE _err OUTPUT_QUIET)
    endif()
    if(NOT _rc EQUAL 0)
        
        set(_msg "")
        string(REGEX MATCHALL "[^\n]+" _lines "${_err}")
        foreach(_line IN LISTS _lines)
            string(TOLOWER "${_line}" _low)
            if(_low MATCHES "undefined reference|undefined symbol|error:")
                set(_msg "${_line}")
                break()
            endif()
        endforeach()
        message("FAIL osx-${ARCH}/${n} (link): ${_msg}")
        set(_status 1)
        continue()
    endif()

    
    if(ARCH STREQUAL "x86_64")
        execute_process(COMMAND "${WORK}/run"
                        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    else()
        execute_process(COMMAND "${QEMU}" -L "${SYSROOT}" "${WORK}/run"
                        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    endif()
    if(_rc EQUAL 0)
        message("PASS osx-${ARCH}/${n} (${ARCH}-osx codegen executed)")
    else()
        message("FAIL osx-${ARCH}/${n} (run, rc=${_rc})")
        set(_status 1)
    endif()
    file(REMOVE "${WORK}/run")
endforeach()

if(NOT _status EQUAL 0)
    cmake_language(EXIT 1)
endif()
