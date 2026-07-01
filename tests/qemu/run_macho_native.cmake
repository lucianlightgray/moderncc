

















foreach(_req SRC MCC BDIR WORK)
    if(NOT DEFINED ${_req})
        message(FATAL_ERROR "missing required -D${_req}=...")
    endif()
endforeach()

set(CONF "${SRC}/tests/qemu/conformance")
set(INC "${SRC}/runtime/include")


if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message("SKIP: host is not Darwin (native Mach-O needs a macOS host)")
    cmake_language(EXIT 77)
endif()



if((NOT EXISTS "${MCC}") OR (IS_DIRECTORY "${MCC}"))
    message("SKIP: no native mcc (${MCC})")
    cmake_language(EXIT 77)
endif()



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
