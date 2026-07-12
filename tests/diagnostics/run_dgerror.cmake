if(NOT MCC OR NOT BDIR OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DBDIR= -DSRC= [-DOUT=] -P run_dgerror.cmake")
endif()

if(NOT OUT)
    get_filename_component(_stem "${SRC}" NAME_WE)
    set(OUT "${BDIR}/dgerror_${_stem}.o")
endif()

file(READ "${SRC}" _txt)
string(REGEX MATCH "dg-error:[ \t]*([^\r\n]*)" _marker "${_txt}")
if(NOT _marker)
    message(FATAL_ERROR "no 'dg-error:' marker in ${SRC}")
endif()
set(_want "${CMAKE_MATCH_1}")
string(REGEX REPLACE "[ \t]*\\*/.*$" "" _want "${_want}")
string(STRIP "${_want}" _want)
if(_want STREQUAL "")
    message(FATAL_ERROR "empty 'dg-error:' substring in ${SRC}")
endif()

execute_process(
    COMMAND "${MCC}" "-B${BDIR}" -c "${SRC}" -o "${OUT}"
    OUTPUT_VARIABLE _co ERROR_VARIABLE _ce RESULT_VARIABLE _rc)
set(_all "${_co}${_ce}")

if(_rc EQUAL 0)
    message(FATAL_ERROR "expected compile to FAIL for ${SRC}, but it succeeded (rc=0)\n${_all}")
endif()

string(FIND "${_all}" "${_want}" _pos)
if(_pos EQUAL -1)
    message(FATAL_ERROR "compile failed (rc=${_rc}) but diagnostic did not contain expected substring.\n  want: ${_want}\n  got:\n${_all}")
endif()
