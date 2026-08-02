cmake_minimum_required(VERSION 3.22)

if(NOT MCC OR NOT CORPUS OR NOT EXTRA OR NOT TMPDIR)
    message(FATAL_ERROR "rir_c2: MCC, CORPUS, EXTRA, TMPDIR are required")
endif()
if(NOT OPT)
    set(OPT "-O1")
endif()
set(_mccflags "")
if(MCCFLAGS)
    separate_arguments(_mccflags NATIVE_COMMAND "${MCCFLAGS}")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")
set(ENV{SOURCE_DATE_EPOCH} "1000000000")

file(WRITE "${TMPDIR}/probe.c" "int rir_c2_probe(int a, int b) { return a * b + 1; }\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_REPLAY_IR=5"
            "${MCC}" -w "${OPT}" ${_mccflags} -c -o "${TMPDIR}/probe.o" "${TMPDIR}/probe.c"
    OUTPUT_QUIET ERROR_VARIABLE _perr RESULT_VARIABLE _prc)
if(NOT _perr MATCHES "\\[rir-total\\]")
    message(STATUS "rir_c2: no [rir-total] output — build has no MCC_REPLAY_IR; SKIP")
    cmake_language(EXIT 77)
endif()
if(NOT _perr MATCHES "c2try=[1-9]")
    message(STATUS "rir_c2: c2try=0 on the probe — the C2 re-emit needs a "
                   "-DMCC_REPLAY_IR_C2=1 build; SKIP")
    cmake_language(EXIT 77)
endif()

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
list(APPEND _srcs "${EXTRA}")
list(SORT _srcs)
list(LENGTH _srcs _nsrcs)

set(_files 0)
set(_fn 0)
set(_faithful 0)
set(_try 0)
set(_skip 0)
set(_ok 0)
set(_bytes 0)
set(_len 0)
set(_err 0)
set(_invalid 0)
set(_notok "")
foreach(_f ${_srcs})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_REPLAY_IR=5"
                "${MCC}" -w "${OPT}" ${_mccflags} -c -o "${TMPDIR}/a.o" "${_f}"
        OUTPUT_QUIET ERROR_VARIABLE _ferr RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        list(APPEND _notok "${_f}")
        continue()
    endif()
    if(NOT _ferr MATCHES "\\[rir-total\\]")
        continue()
    endif()
    math(EXPR _files "${_files} + 1")
    if(_ferr MATCHES "\\[rir-total\\] fn=([0-9]+) faithful=([0-9]+)")
        math(EXPR _fn "${_fn} + ${CMAKE_MATCH_1}")
        math(EXPR _faithful "${_faithful} + ${CMAKE_MATCH_2}")
    endif()
    if(_ferr MATCHES "c2try=([0-9]+) c2skip=([0-9]+) c2ok=([0-9]+) c2bytes=([0-9]+) c2len=([0-9]+) c2err=([0-9]+) c2invalid=([0-9]+)")
        math(EXPR _try "${_try} + ${CMAKE_MATCH_1}")
        math(EXPR _skip "${_skip} + ${CMAKE_MATCH_2}")
        math(EXPR _ok "${_ok} + ${CMAKE_MATCH_3}")
        math(EXPR _bytes "${_bytes} + ${CMAKE_MATCH_4}")
        math(EXPR _len "${_len} + ${CMAKE_MATCH_5}")
        math(EXPR _err "${_err} + ${CMAKE_MATCH_6}")
        math(EXPR _invalid "${_invalid} + ${CMAKE_MATCH_7}")
    endif()
endforeach()

math(EXPR _gap "${_try} - ${_ok}")
list(LENGTH _notok _nnotok)
message(STATUS "rir_c2: ${OPT} srcs=${_nsrcs} ok=${_files} notok=${_nnotok} "
               "fn=${_fn} faithful=${_faithful} c2ok=${_ok}/${_try} gap=${_gap} "
               "(bytes=${_bytes} len=${_len} err=${_err} invalid=${_invalid}) "
               "c2skip=${_skip}")
foreach(_n ${_notok})
    message(STATUS "  rc!=0 ${_n}")
endforeach()

if(_files EQUAL 0)
    message(FATAL_ERROR "rir_c2: compiled nothing")
endif()
if(_try EQUAL 0)
    message(FATAL_ERROR "rir_c2: 0 body(ies) re-emitted — the C2 instrument "
                        "measured nothing, so a pass here is vacuous")
endif()
if(NOT DEFINED BANKGAP)
    message(STATUS "rir_c2: no BANKGAP — measurement only, nothing ratcheted")
    return()
endif()

if(NOT DEFINED BANKFN)
    message(FATAL_ERROR "rir_c2: BANKGAP without BANKFN — a gap ratchet over an "
                        "unpinned population passes by measuring fewer bodies")
endif()
if(_fn LESS BANKFN)
    math(EXPR _lost "${BANKFN} - ${_fn}")
    message(FATAL_ERROR "rir_c2: ${_fn} body(ies) against a banked ${BANKFN} — "
                        "${_lost} fewer. The population shrank, so the gap below "
                        "is not comparable to the bank whichever way it reads")
endif()
if(_gap GREATER BANKGAP)
    math(EXPR _worse "${_gap} - ${BANKGAP}")
    message(FATAL_ERROR "rir_c2: gap ${_gap} against a banked ${BANKGAP} — "
                        "${_worse} more body(ies) whose C2 re-emit does not "
                        "reproduce the parser's bytes")
endif()
if(DEFINED BANKSKIP)
    if(_skip GREATER BANKSKIP)
        math(EXPR _worse "${_skip} - ${BANKSKIP}")
        message(FATAL_ERROR "rir_c2: c2skip ${_skip} against a banked "
                            "${BANKSKIP} — ${_worse} more body(ies) declined the "
                            "re-emit outright")
    endif()
endif()
if(_gap LESS BANKGAP)
    message(STATUS "rir_c2: gap ${_gap} is BELOW the banked ${BANKGAP} — rebank "
                   "this cell before the improvement can be held")
endif()
message(STATUS "rir_c2: OK — c2ok ${_ok}/${_try}, gap ${_gap} <= banked "
               "${BANKGAP}, fn ${_fn} >= banked ${BANKFN}")
