if(NOT MCC OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -P increment.cmake")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_SELFCHECK=1 MCC_CST_STORE=1
            "${MCC}" -c "${SRC}" -o "${OUT}" "-I${IDIR}"
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
set(_all "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to compile ${SRC} (rc=${_rc}):\n${_all}")
endif()
if(NOT _all MATCHES "round-trip OK")
    message(FATAL_ERROR "CST round-trip did not hold through the recursion:\n${_all}")
endif()
if(NOT _all MATCHES "CST store: 1 templates")
    message(FATAL_ERROR "recursive re-include did not hash-cons to ONE template:\n${_all}")
endif()
if(NOT _all MATCHES "template 0: [0-9]+ nodes, [1-9][0-9]* PPConditional, render_identity [0-9]+/[0-9]+ OK")
    message(FATAL_ERROR "template is not full-concrete / does not round-trip:\n${_all}")
endif()
message(STATUS "increment OK: recursive re-include -> 1 full-concrete SourceFile template")
