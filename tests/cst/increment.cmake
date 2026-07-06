# CST recursive dynamic-value re-include gate (docs/CST.md D3). increment.h
# re-includes ITSELF several times under an incrementing, depth-gated macro
# (#if (INCREMENTING) < 3). Every pass reads the same bytes, so all passes
# hash-cons to ONE content-addressed SourceFile template — the core D3 claim
# that a file's bytes fix its template regardless of include context. The main
# file must still round-trip byte-identically through all the recursion.
#   cmake -DMCC= -DSRC= -DOUT= -DIDIR= -P increment.cmake
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
# The recursively re-included header collapses to exactly one template.
if(NOT _all MATCHES "CST store: 1 templates")
    message(FATAL_ERROR "recursive re-include did not hash-cons to ONE template:\n${_all}")
endif()
message(STATUS "increment OK: recursive re-include deduped to 1 SourceFile template")
