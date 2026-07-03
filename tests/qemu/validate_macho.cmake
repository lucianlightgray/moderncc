











if(NOT DEFINED SRC OR NOT DEFINED XB OR NOT DEFINED WORK)
    message(FATAL_ERROR "usage: cmake -DSRC=.. -DXB=.. -DWORK=.. -P validate_macho.cmake")
endif()

set(CONF "${SRC}/tests/qemu/conformance")


find_program(_otool NAMES llvm-otool otool PATHS /usr/lib/llvm/22/bin)
if(NOT _otool)
    message("SKIP: no Mach-O parser (otool/llvm-otool)")
    cmake_language(EXIT 77)
endif()

file(MAKE_DIRECTORY "${WORK}")
set(_status 0)
set(_ran_any 0)

foreach(tgt x86_64-osx arm64-osx)
    set(_mcc "${XB}/mcc-${tgt}")
    if(NOT EXISTS "${_mcc}")
        message("SKIP ${tgt}: no mcc-${tgt}")
        continue()
    endif()
    set(_ran_any 1)

    file(GLOB _srcs "${CONF}/*.c")
    foreach(f ${_srcs})
        get_filename_component(n "${f}" NAME_WE)
        
        
        
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

    # -mmacosx-version-min must land in LC_BUILD_VERSION (default is 10.6)
    set(_vsrc "${WORK}/macho_${tgt}_versionmin.c")
    set(_vexe "${WORK}/macho_${tgt}_versionmin")
    file(WRITE "${_vsrc}" "int main(void){return 0;}\n")
    execute_process(
        COMMAND "${_mcc}" "-I${SRC}/runtime/include"
                -mmacosx-version-min=12.3.1 "${_vsrc}" -o "${_vexe}"
        RESULT_VARIABLE _rc ERROR_VARIABLE _err OUTPUT_QUIET)
    if(NOT _rc EQUAL 0)
        message("FAIL ${tgt}/versionmin (link): ${_err}")
        set(_status 1)
    else()
        execute_process(COMMAND "${_otool}" -l "${_vexe}"
            RESULT_VARIABLE _orc OUTPUT_VARIABLE _lcs ERROR_QUIET)
        if(_orc EQUAL 0 AND _lcs MATCHES "minos 12\\.3\\.1")
            message("PASS ${tgt}/versionmin (LC_BUILD_VERSION minos 12.3.1)")
        else()
            message("FAIL ${tgt}/versionmin: minos 12.3.1 not in load commands")
            set(_status 1)
        endif()
    endif()
    file(REMOVE "${_vexe}" "${_vsrc}")
endforeach()

if(NOT _ran_any)
    message("SKIP: no osx cross compilers (mcc-<tgt>) for any target")
    cmake_language(EXIT 77)
endif()
if(NOT _status EQUAL 0)
    cmake_language(EXIT 1)
endif()
cmake_language(EXIT 0)
