# parts-suite: compile every tests/diff/parts/run_*.c wrapper with gcc, clang and
# mcc, run each, and require byte-identical stdout across all three. Each wrapper
# is a standalone unit test of one parts/*.h unit; the same units are aggregated
# (via #include) into tests/diff/full_language.c for the all-in-one C11 test.
#
# Args: -DGCC= -DCLANG= -DMCC= -DBDIR= -DIDIR= -DPARTS= -DWORK=
cmake_minimum_required(VERSION 3.16)
file(MAKE_DIRECTORY "${WORK}")
file(GLOB _wraps "${PARTS}/run_*.c")
list(SORT _wraps)

set(_fail 0)
set(_ok 0)
foreach(_w ${_wraps})
    get_filename_component(_name "${_w}" NAME_WE)
    set(_flags "-I${PARTS}" -w -O0 -std=gnu11 -lm)

    execute_process(
        COMMAND "${GCC}" "${_w}" ${_flags} -o "${WORK}/${_name}.gcc"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(NOT _rc EQUAL 0)
        message(SEND_ERROR "[${_name}] gcc build failed:\n${_o}${_e}")
        math(EXPR _fail "${_fail}+1")
        continue()
    endif()
    execute_process(COMMAND "${WORK}/${_name}.gcc" OUTPUT_FILE "${WORK}/${_name}.out.gcc")

    execute_process(
        COMMAND "${CLANG}" "${_w}" ${_flags} -o "${WORK}/${_name}.clang"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(NOT _rc EQUAL 0)
        message(SEND_ERROR "[${_name}] clang build failed:\n${_o}${_e}")
        math(EXPR _fail "${_fail}+1")
        continue()
    endif()
    execute_process(COMMAND "${WORK}/${_name}.clang" OUTPUT_FILE "${WORK}/${_name}.out.clang")

    execute_process(
        COMMAND "${MCC}" "-B${BDIR}" "-I${IDIR}" "-I${PARTS}" "${_w}" -lm -o "${WORK}/${_name}.mcc"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(NOT _rc EQUAL 0)
        message(SEND_ERROR "[${_name}] mcc build failed:\n${_o}${_e}")
        math(EXPR _fail "${_fail}+1")
        continue()
    endif()
    execute_process(COMMAND "${WORK}/${_name}.mcc" OUTPUT_FILE "${WORK}/${_name}.out.mcc")

    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${WORK}/${_name}.out.gcc" "${WORK}/${_name}.out.clang" RESULT_VARIABLE _dc)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${WORK}/${_name}.out.gcc" "${WORK}/${_name}.out.mcc" RESULT_VARIABLE _dm)
    if(NOT _dc EQUAL 0 OR NOT _dm EQUAL 0)
        file(READ "${WORK}/${_name}.out.gcc" _rg)
        file(READ "${WORK}/${_name}.out.mcc" _rm)
        message(SEND_ERROR
            "[${_name}] output diverged (gcc==clang:${_dc} gcc==mcc:${_dm})\n"
            "--- gcc ---\n${_rg}\n--- mcc ---\n${_rm}")
        math(EXPR _fail "${_fail}+1")
    else()
        math(EXPR _ok "${_ok}+1")
    endif()
endforeach()

message(STATUS "parts-suite: ${_ok} unit(s) 3-way-identical, ${_fail} diverged")
if(_fail GREATER 0)
    message(FATAL_ERROR "parts-suite: ${_fail} unit(s) failed")
endif()
