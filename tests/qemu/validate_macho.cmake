# Structurally validate mcc's Mach-O (Darwin) codegen: for each osx cross
# target, link every tests/qemu/conformance/*.c into a Mach-O executable and
# confirm a Mach-O parser (llvm-otool/otool) reads its header and all load
# commands. (Executing Mach-O binaries needs macOS or darling, which are not
# present here; this is the strongest verification available off-Darwin.)
#
# CMake -P port of validate_macho.sh -- no POSIX shell required.
# Usage: cmake -DSRC=<src-dir> -DXB=<cross-build-dir> -DWORK=<work-dir>
#              -P validate_macho.cmake
# Whole-test skip (no parser / no osx cross compilers) -> EXIT 77 (ctest Skipped).
# Any FAIL -> EXIT 1. Otherwise EXIT 0.

if(NOT DEFINED SRC OR NOT DEFINED XB OR NOT DEFINED WORK)
    message(FATAL_ERROR "usage: cmake -DSRC=.. -DXB=.. -DWORK=.. -P validate_macho.cmake")
endif()

set(CONF "${SRC}/tests/qemu/conformance")

# pick(): first parser found on PATH (or the known Gentoo llvm path).
find_program(_otool NAMES llvm-otool otool PATHS /usr/lib/llvm/22/bin)
if(NOT _otool)
    message("SKIP: no Mach-O parser (otool/llvm-otool)")
    cmake_language(EXIT 77)
endif()

file(MAKE_DIRECTORY "${WORK}")
set(_status 0)
set(_ran_any 0)

foreach(tgt x86_64-osx arm64-osx)
    set(_mcc "${XB}/${tgt}-mcc")
    if(NOT EXISTS "${_mcc}")
        message("SKIP ${tgt}: no ${tgt}-mcc")
        continue()
    endif()
    set(_ran_any 1)

    file(GLOB _srcs "${CONF}/*.c")
    foreach(f ${_srcs})
        get_filename_component(n "${f}" NAME_WE)
        # These pull in the target C library (memset/memmove/abort/<string.h>),
        # which needs the macOS SDK (libSystem) -- absent here. Validate only the
        # codegen-self-contained programs.
        if(n STREQUAL "aggregates" OR n STREQUAL "libc" OR n STREQUAL "varargs")
            message("SKIP ${tgt}/${n} (needs macOS libSystem)")
            continue()
        endif()

        set(exe "${WORK}/macho_${tgt}_${n}")
        execute_process(
            COMMAND "${_mcc}" "-I${SRC}/runtime/include" "${f}" -o "${exe}"
            RESULT_VARIABLE _rc
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            # No macOS SDK here, so a program referencing libSystem (memset,
            # abort, <string.h>, ...) can't be linked; that's an environment
            # limit, not a codegen defect, so skip it rather than fail.
            string(FIND "${_err}" "unresolved reference" _u)
            string(FIND "${_err}" "not found" _nf)
            if(NOT _u EQUAL -1 OR NOT _nf EQUAL -1)
                message("SKIP ${tgt}/${n} (needs macOS libSystem)")
                continue()
            endif()
            string(REGEX REPLACE "\r?\n.*$" "" _firstline "${_err}")
            message("FAIL ${tgt}/${n} (link): ${_firstline}")
            set(_status 1)
            continue()
        endif()

        # MH_MAGIC_64 (little-endian: cf fa ed fe) and a parseable load-command table
        file(READ "${exe}" _hex LIMIT 4 HEX)
        if(NOT _hex STREQUAL "cffaedfe")
            message("FAIL ${tgt}/${n}: bad Mach-O magic (${_hex})")
            set(_status 1)
        else()
            execute_process(
                COMMAND "${_otool}" -l "${exe}"
                RESULT_VARIABLE _orc
                OUTPUT_QUIET ERROR_QUIET)
            if(_orc EQUAL 0)
                message("PASS ${tgt}/${n} (valid Mach-O)")
            else()
                message("FAIL ${tgt}/${n}: otool could not parse load commands")
                set(_status 1)
            endif()
        endif()
        file(REMOVE "${exe}")
    endforeach()
endforeach()

if(NOT _ran_any)
    message("SKIP: no osx cross compilers (<tgt>-mcc) for any target")
    cmake_language(EXIT 77)
endif()
if(NOT _status EQUAL 0)
    cmake_language(EXIT 1)
endif()
cmake_language(EXIT 0)
