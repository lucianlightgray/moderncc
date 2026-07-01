# Runtime-verify mcc's PE (Windows) codegen under wine: for each win32 cross
# target, assemble a -B tree (crt objects + libmcc1.a + the .def import libs),
# link every tests/qemu/conformance/*.c into a PE executable, and run it under
# wine. Each conformance program self-checks and must exit 0.
#
# CMake -P port of run_pe_wine.sh (no POSIX shell required).
# Usage: cmake -DSRC=<src-dir> -DXB=<cross-build-dir> -DWORK=<work-dir> -P run_pe_wine.cmake
# Whole-test skip -> EXIT 77 (wine or win32 cross compilers absent).
# Any failure -> EXIT 1.  Else EXIT 0.

if(NOT DEFINED SRC OR NOT DEFINED XB OR NOT DEFINED WORK)
    message(FATAL_ERROR "usage: cmake -DSRC=<src> -DXB=<cross-build> -DWORK=<work> -P run_pe_wine.cmake")
endif()

set(CONF "${SRC}/tests/qemu/conformance")

# pick a wine: prefer plain wine/wine64, fall back to a proton build
function(pick out)
    foreach(c ${ARGN})
        find_program(_pick_${c} NAMES "${c}")
        if(_pick_${c})
            set(${out} "${_pick_${c}}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} "" PARENT_SCOPE)
endfunction()

pick(WINE64 wine64 wine wine64-proton-10.0.4)
pick(WINE32 wine wine32 wine-proton-10.0.4)
if(NOT WINE64 AND NOT WINE32)
    message("SKIP: no wine found")
    cmake_language(EXIT 77)
endif()

# env vars consumed by wine (execute_process has no ENVIRONMENT option, so
# export them into this process's environment via $ENV{} like the .sh did)
set(ENV{WINEDEBUG} "-all")
set(ENV{WINEPREFIX} "${WORK}/.wineprefix")

set(status 0)
set(any_target 0)
foreach(tgt x86_64-win32 i386-win32)
    set(MCC "${XB}/mcc-${tgt}")
    if(NOT EXISTS "${MCC}")
        message("SKIP ${tgt}: no mcc-${tgt}")
        continue()
    endif()
    if(tgt STREQUAL "i386-win32")
        set(WINE "${WINE32}")
    else()
        set(WINE "${WINE64}")
    endif()
    if(NOT WINE)
        message("SKIP ${tgt}: no matching wine")
        continue()
    endif()
    set(any_target 1)

    set(B "${WORK}/B-${tgt}")
    file(REMOVE_RECURSE "${B}")
    file(MAKE_DIRECTORY "${B}/lib")
    # tolerate missing files (the .sh's `|| true`): glob then copy what exists
    file(GLOB _defs "${SRC}/runtime/win32/lib/*.def")
    file(GLOB _objs "${XB}/lib-${tgt}/*.o")
    set(_a "${XB}/${tgt}-libmcc1.a")
    if(_defs)
        file(COPY ${_defs} DESTINATION "${B}/lib")
    endif()
    if(_objs)
        file(COPY ${_objs} DESTINATION "${B}/lib")
        file(COPY ${_objs} DESTINATION "${B}")
    endif()
    if(EXISTS "${_a}")
        file(COPY "${_a}" DESTINATION "${B}/lib")
        file(COPY "${_a}" DESTINATION "${B}")
    endif()

    file(GLOB _confs "${CONF}/*.c")
    foreach(f ${_confs})
        get_filename_component(n "${f}" NAME_WE)
        set(exe "${WORK}/pe_${tgt}_${n}.exe")
        execute_process(
            COMMAND "${MCC}" "-B${B}"
                    "-I${SRC}/runtime/win32/include" "-I${SRC}/runtime/include"
                    "${f}" -o "${exe}"
            RESULT_VARIABLE crc
            OUTPUT_VARIABLE cout
            ERROR_VARIABLE  cerr)
        if(NOT crc EQUAL 0)
            string(REGEX REPLACE "\n.*" "" _firstline "${cerr}")
            message("FAIL ${tgt}/${n} (compile): ${_firstline}")
            set(status 1)
            continue()
        endif()
        # Redirect to a real file (not OUTPUT_QUIET, which drains a pipe): wine
        # daemonizes wineserver, which inherits the captured-output pipe and keeps
        # it open until its idle-kill timeout (~3s) — so a pipe makes each call
        # block for seconds (×32 ≈ 100s). A file fd has no such reader to wait on,
        # matching the shell's `>/dev/null 2>&1`.
        execute_process(
            COMMAND "${WINE}" "${exe}"
            RESULT_VARIABLE rc
            OUTPUT_FILE "${WORK}/wine-run.out" ERROR_FILE "${WORK}/wine-run.out")
        if(rc EQUAL 0)
            message("PASS ${tgt}/${n}")
        else()
            message("FAIL ${tgt}/${n} (run, rc=${rc})")
            set(status 1)
        endif()
        file(REMOVE "${exe}")
    endforeach()
endforeach()

if(NOT any_target)
    message("SKIP: no win32 cross-mcc for any target")
    cmake_language(EXIT 77)
endif()

if(NOT status EQUAL 0)
    cmake_language(EXIT 1)
endif()
cmake_language(EXIT 0)
