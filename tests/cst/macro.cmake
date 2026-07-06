# CST macro-fidelity driver (slice J, PLAN §4). Asserts macro uses become
# CST_MacroInvocation nodes AND the written source round-trips byte-identically.
#   cmake -DMCC= -DSRC= -DOUT= -DIDIR= -P macro.cmake
if(NOT MCC OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -P macro.cmake")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_SELFCHECK=1 MCC_CST_TREE=1
            "${MCC}" -c "${SRC}" -o "${OUT}" "-I${IDIR}"
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
set(_all "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to compile ${SRC} (rc=${_rc}):\n${_all}")
endif()
if(NOT _all MATCHES "round-trip OK")
    message(FATAL_ERROR "macro source did not round-trip:\n${_all}")
endif()
# SQUARE, ADD (function-like) and LIMIT (object-like) -> at least 3 MacroInv.
string(REGEX MATCHALL "MacroInv" _mi "${_all}")
list(LENGTH _mi _n)
if(_n LESS 3)
    message(FATAL_ERROR "expected >=3 MacroInvocation nodes, got ${_n}:\n${_all}")
endif()
message(STATUS "macro OK: ${_n} MacroInvocation nodes, round-trip clean")
