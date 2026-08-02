cmake_minimum_required(VERSION 3.22)

if(NOT MCC OR NOT CORPUS OR NOT EXTRA OR NOT TMPDIR)
    message(FATAL_ERROR "rir_position: MCC, CORPUS, EXTRA, TMPDIR are required")
endif()
if(NOT OPT)
    set(OPT "-O1")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")
set(ENV{SOURCE_DATE_EPOCH} "1000000000")

file(WRITE "${TMPDIR}/probe.c" "int rir_probe(int a, int b) { return a * b + 1; }\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_REPLAY_IR=3"
            "${MCC}" -w "${OPT}" -c -o "${TMPDIR}/probe.o" "${TMPDIR}/probe.c"
    OUTPUT_QUIET ERROR_VARIABLE _perr RESULT_VARIABLE _prc)
if(NOT _perr MATCHES "\\[rir-total\\]")
    message(STATUS "rir_position: no [rir-total] output — build has no "
                   "MCC_REPLAY_IR; SKIP")
    cmake_language(EXIT 77)
endif()

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
list(APPEND _srcs "${EXTRA}")
list(SORT _srcs)

set(_ok 0)
set(_bad 0)
set(_open 0)
set(_skip 0)
set(_fn 0)
set(_files 0)
set(_badnames "")
foreach(_f ${_srcs})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_REPLAY_IR=3"
                "${MCC}" -w "${OPT}" -c -o "${TMPDIR}/a.o" "${_f}"
        OUTPUT_QUIET ERROR_VARIABLE _err RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        continue()
    endif()
    math(EXPR _files "${_files} + 1")
    if(_err MATCHES "shiftok=([0-9]+) shiftbad=([0-9]+) shiftskip=([0-9]+) shiftopen=([0-9]+)")
        math(EXPR _ok "${_ok} + ${CMAKE_MATCH_1}")
        math(EXPR _bad "${_bad} + ${CMAKE_MATCH_2}")
        math(EXPR _skip "${_skip} + ${CMAKE_MATCH_3}")
        math(EXPR _open "${_open} + ${CMAKE_MATCH_4}")
    endif()
    if(_err MATCHES "\\[rir-total\\] fn=([0-9]+)")
        math(EXPR _fn "${_fn} + ${CMAKE_MATCH_1}")
    endif()
    string(REGEX MATCHALL "\\[rir-verify\\][^\n]*shift=bad[^\n]*" _lines "${_err}")
    foreach(_l ${_lines})
        list(APPEND _badnames "${_l}")
    endforeach()
endforeach()

message(STATUS "rir_position: ${OPT} files=${_files} bodies=${_fn} "
               "shift ok=${_ok} open=${_open} skip=${_skip} bad=${_bad}")
foreach(_b ${_badnames})
    message(STATUS "  BAD ${_b}")
endforeach()

if(_files EQUAL 0)
    message(FATAL_ERROR "rir_position: compiled nothing")
endif()
if(_ok EQUAL 0)
    message(FATAL_ERROR "rir_position: 0 bodies replayed at a shifted base — "
                        "the instrument measured nothing, so bad=0 is vacuous")
endif()
if(NOT _bad EQUAL 0)
    message(FATAL_ERROR "rir_position: ${_bad} body(ies) re-emit different bytes "
                        "at a shifted base — Replay_IR control flow is not "
                        "position-independent")
endif()
message(STATUS "rir_position: OK")
