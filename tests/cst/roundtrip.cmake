if(NOT MCC OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -P roundtrip.cmake")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_SELFCHECK=1
            "MCC_CST_SNAPSHOT=${OUT}.snap"
            "${MCC}" --lsp -c "${SRC}" -o "${OUT}" "-I${IDIR}"
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)

set(_all "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to compile ${SRC} (rc=${_rc}):\n${_all}")
endif()
if(_all MATCHES "MISMATCH")
    message(FATAL_ERROR "CST round-trip MISMATCH for ${SRC}:\n${_all}")
endif()
if(NOT _all MATCHES "round-trip OK")
    message(FATAL_ERROR "CST self-check did not run for ${SRC}:\n${_all}")
endif()
if(NOT _all MATCHES "snapshot: reload OK")
    message(FATAL_ERROR "CST snapshot reload failed for ${SRC}:\n${_all}")
endif()
