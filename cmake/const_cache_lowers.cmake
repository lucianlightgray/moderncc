# gpu/*-const-cache: a single region with more than 512 distinct integer
# constants must lower (T-lin-10037). The SPV/MSL constant cache is a 512-entry
# dedup table, not a device limit; once it filled, the emitter used to set
# m.failed and refuse the whole region. This runs the gate device-free
# (--emit-only) over its built-in `bigconst` case (o0*1 + o0*3 + ... , 600
# distinct constants) and fails if that region does not lower.
#
# Inputs: GATE = the spvgate/mslgate executable; BINDIR = a writable dir.

set(_out "${BINDIR}/const_cache_emit")
file(REMOVE_RECURSE "${_out}")
file(MAKE_DIRECTORY "${_out}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env SPVGATE_CONST_CACHE=1
            "${GATE}" --emit-only "${_out}"
    OUTPUT_VARIABLE _log ERROR_VARIABLE _err RESULT_VARIABLE _rc)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "const-cache: gate exited ${_rc}\n${_log}${_err}")
endif()
if("${_log}" MATCHES "bigconst[ \t]+w=[0-9]+[ \t]+SKIP")
    message(FATAL_ERROR
        "const-cache: the >512-constant region did NOT lower -- the SPV/MSL "
        "constant-cache cap rebound (T-lin-10037).\n${_log}")
endif()
if(NOT "${_log}" MATCHES "bigconst[ \t]+OK")
    message(FATAL_ERROR
        "const-cache: the bigconst case is absent from the emit-only run; was "
        "it removed from tools/spvgate.c?\n${_log}")
endif()
message(STATUS "const-cache: >512-constant region lowered (bigconst OK)")
