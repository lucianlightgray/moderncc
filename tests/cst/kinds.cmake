# CST node-kind coverage gate (D4, docs/CST.md). Compiles SRC with the CST tree
# dump on and asserts every kind name in KINDS (a ';'-separated list) is produced
# — a *coverage* assertion, since round-trip alone passed even while these kinds
# were reserved-but-unproduced. Also re-checks the round-trip self-check.
#   cmake -DMCC= -DSRC= -DOUT= -DIDIR= -DKINDS=a;b;c -P kinds.cmake
if(NOT MCC OR NOT SRC OR NOT KINDS)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -DKINDS= -P kinds.cmake")
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
    message(FATAL_ERROR "CST round-trip did not hold for ${SRC}:\n${_all}")
endif()
if(_all MATCHES "MISMATCH")
    message(FATAL_ERROR "CST round-trip MISMATCH for ${SRC}:\n${_all}")
endif()
foreach(_k IN LISTS KINDS)
    # The tree dump prints internal nodes as "<Kind> [lo,hi)"; anchor on the
    # trailing space+bracket so e.g. "Paren" does not match "ParamList".
    if(NOT _all MATCHES "[^A-Za-z]${_k} \\[")
        message(FATAL_ERROR "CST kind '${_k}' not produced for ${SRC}:\n${_all}")
    endif()
endforeach()
message(STATUS "kinds OK: all of [${KINDS}] produced; round-trip clean")
