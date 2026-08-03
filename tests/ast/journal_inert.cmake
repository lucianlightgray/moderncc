cmake_minimum_required(VERSION 3.22)

if(NOT MCC OR NOT CORPUS OR NOT EXTRA OR NOT TMPDIR)
    message(FATAL_ERROR "journal_inert: MCC, CORPUS, EXTRA, TMPDIR are required")
endif()
if(NOT OPT)
    set(OPT "-O1")
endif()

set(_hooks_mode FALSE)
if(NOJRN)
    set(_hooks_mode TRUE)
endif()
set(_rir_mode FALSE)
if(RIR)
    set(_rir_mode TRUE)
endif()
if(_rir_mode AND _hooks_mode)
    message(FATAL_ERROR "journal_inert: RIR and NOJRN are mutually exclusive")
endif()
set(_gate_env "MCC_REPLAY_IR=1")
set(_gate_marker "rir-verify")

file(MAKE_DIRECTORY "${TMPDIR}")

set(ENV{SOURCE_DATE_EPOCH} "1000000000")

if(_hooks_mode AND NOT EXISTS "${NOJRN}")
    message(STATUS "journal_inert: ${NOJRN} missing; SKIP")
    cmake_language(EXIT 77)
endif()

function(_jrn_count outvar mcc)
    file(WRITE "${TMPDIR}/probe.c"
         "int jrn_probe(int a, int b) { return a * b + 1; }\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "${_gate_env}"
                "${mcc}" -w "${OPT}" -c -o "${TMPDIR}/probe.o" "${TMPDIR}/probe.c"
        OUTPUT_QUIET ERROR_VARIABLE _err RESULT_VARIABLE _rc)
    string(REGEX MATCHALL "\\[${_gate_marker}\\]" _hits "${_err}")
    list(LENGTH _hits _n)
    set(${outvar} "${_n}" PARENT_SCOPE)
endfunction()

_jrn_count(_probe "${MCC}")
if(_probe EQUAL 0)
    message(STATUS "journal_inert: no [${_gate_marker}] output — build has no "
                   "MCC_REPLAY_IR; SKIP")
    cmake_language(EXIT 77)
endif()

if(_hooks_mode)
    _jrn_count(_anti "${NOJRN}")
    if(NOT _anti EQUAL 0)
        message(FATAL_ERROR "journal_inert: the -DMCC_JOURNAL_HOOKS=0 build still "
                            "emits [${_gate_marker}]")
    endif()
endif()

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
list(APPEND _srcs "${EXTRA}")
list(SORT _srcs)

set(_n 0)
set(_bad 0)
set(_diffs "")
foreach(_f ${_srcs})
    execute_process(
        COMMAND "${MCC}" -w "${OPT}" -c -o "${TMPDIR}/a.o" "${_f}"
        OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _rcA)
    if(NOT _rcA EQUAL 0)
        continue()
    endif()
    if(_hooks_mode)
        execute_process(
            COMMAND "${NOJRN}" -w "${OPT}" -c -o "${TMPDIR}/b.o" "${_f}"
            OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _rcB)
    else()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "${_gate_env}"
                    "${MCC}" -w "${OPT}" -c -o "${TMPDIR}/b.o" "${_f}"
            OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _rcB)
    endif()
    if(NOT _rcB EQUAL 0)
        continue()
    endif()
    math(EXPR _n "${_n} + 1")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${TMPDIR}/a.o" "${TMPDIR}/b.o"
        RESULT_VARIABLE _cmp OUTPUT_QUIET ERROR_QUIET)
    if(NOT _cmp EQUAL 0)
        math(EXPR _bad "${_bad} + 1")
        file(RELATIVE_PATH _rel "${CORPUS}" "${_f}")
        list(APPEND _diffs "${_rel}")
    endif()
endforeach()

if(_hooks_mode)
    set(_tag "journal_inert(hooks)")
else()
    set(_tag "rir_inert(runtime)")
endif()
message(STATUS "${_tag}: ${OPT} compared=${_n} differing=${_bad}")
foreach(_d ${_diffs})
    message(STATUS "  DIFF ${_d}")
endforeach()

if(_n EQUAL 0)
    message(FATAL_ERROR "${_tag}: compiled nothing")
endif()
if(NOT _bad EQUAL 0)
    message(FATAL_ERROR "${_tag}: ${_bad} object(s) differ — the side-car is not inert")
endif()
message(STATUS "${_tag}: OK")
