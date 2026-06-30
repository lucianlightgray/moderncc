# Native Mach-O (Darwin) RUNTIME conformance — cmake -P port of run_macho_native.sh.
#
# Faithful port of run_macho_native.sh so the test no longer needs a POSIX shell.
# The codegen-run / image-run / apple-libc drivers exist to approximate, on a
# Linux/x86_64 host, what a real macOS host gives for free: a loader, a real
# dyld, real libSystem, working TLV. On Darwin none of that scaffolding is
# needed — the native `mcc` already emits a native Mach-O executable linked
# against the system libSystem, so each self-checking conformance program just
# compiles and runs. This therefore covers a STRICT SUPERSET of the Linux
# approximations: the TLS programs (Darwin TLV thunks the ELF shortcut can't
# provide) and the libc-dependent programs (real locale/stdio/malloc) all run
# here, where target == host.
#
# Usage: cmake -DSRC=<src-dir> -DMCC=<mcc> -DBDIR=<bdir> -DWORK=<work-dir>
#              -P run_macho_native.cmake
# Self-skips (exit 77, ctest SKIP_RETURN_CODE) off a Darwin host or without a
# Mach-O-targeting mcc.

foreach(_req SRC MCC BDIR WORK)
    if(NOT DEFINED ${_req})
        message(FATAL_ERROR "missing required -D${_req}=...")
    endif()
endforeach()

set(CONF "${SRC}/tests/qemu/conformance")
set(INC "${SRC}/runtime/include")

# [ "$(uname -s)" = Darwin ] || skip
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message("SKIP: host is not Darwin (native Mach-O needs a macOS host)")
    cmake_language(EXIT 77)
endif()

# [ -x "$MCC" ] || skip   (cmake has no -x test; EXISTS + not-a-directory is the
# closest pure-CMake approximation of an executable mcc)
if((NOT EXISTS "${MCC}") OR (IS_DIRECTORY "${MCC}"))
    message("SKIP: no native mcc (${MCC})")
    cmake_language(EXIT 77)
endif()

# Confirm this mcc actually targets Mach-O (a cross build on macOS could target
# ELF/PE, in which case the produced binary won't run on the host).
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/probe.c" "int main(void){return 0;}\n")

execute_process(
    COMMAND "${MCC}" "-B${BDIR}" "${WORK}/probe.c" -o "${WORK}/probe"
    RESULT_VARIABLE _probe_rc
    OUTPUT_QUIET
    ERROR_VARIABLE _probe_err)
file(WRITE "${WORK}/probe.err" "${_probe_err}")
if(NOT _probe_rc EQUAL 0)
    string(REGEX REPLACE "\n.*$" "" _probe_err1 "${_probe_err}")
    message("SKIP: native mcc cannot link an executable: ${_probe_err1}")
    cmake_language(EXIT 77)
endif()

execute_process(
    COMMAND file -b "${WORK}/probe"
    RESULT_VARIABLE _file_rc
    OUTPUT_VARIABLE _probe_type
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _probe_type MATCHES "^Mach-O")
    message("SKIP: native mcc does not target Mach-O (${_probe_type})")
    cmake_language(EXIT 77)
endif()

# The full self-checking set. Unlike the Linux drivers, nothing here is excluded
# for want of a loader/libc/TLV: tls* exercise Darwin TLV, and the *_libc / libc*
# programs exercise the real macOS C library. Each returns 0 on success.
set(PROGS
    atomics control integers floats lexical aggregates varargs complex_annexg
    control_libc floats_libc libc libc_struct varargs_fp vla tls tls_aggr)

set(status 0)
foreach(t IN LISTS PROGS)
    if(NOT EXISTS "${CONF}/${t}.c")
        message("FAIL ${t} (missing source)")
        set(status 1)
        continue()
    endif()

    execute_process(
        COMMAND "${MCC}" "-B${BDIR}" "-I${INC}" "${CONF}/${t}.c" -o "${WORK}/${t}"
        RESULT_VARIABLE _compile_rc
        OUTPUT_QUIET
        ERROR_VARIABLE _compile_err)
    file(WRITE "${WORK}/${t}.err" "${_compile_err}")
    if(NOT _compile_rc EQUAL 0)
        string(REGEX REPLACE "\n.*$" "" _compile_err1 "${_compile_err}")
        message("FAIL osx/${t} (compile): ${_compile_err1}")
        set(status 1)
        continue()
    endif()

    execute_process(
        COMMAND file -b "${WORK}/${t}"
        OUTPUT_VARIABLE _img_type
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _img_type MATCHES "^Mach-O")
        message("FAIL osx/${t}: not a Mach-O image")
        set(status 1)
        continue()
    endif()

    execute_process(
        COMMAND "${WORK}/${t}"
        RESULT_VARIABLE _run_rc
        OUTPUT_QUIET
        ERROR_QUIET)
    if(_run_rc EQUAL 0)
        message("PASS osx/${t} (native Mach-O executed)")
    else()
        message("FAIL osx/${t} (run, rc=${_run_rc})")
        set(status 1)
    endif()
    file(REMOVE "${WORK}/${t}")
endforeach()

if(NOT status EQUAL 0)
    cmake_language(EXIT 1)
endif()
cmake_language(EXIT 0)
