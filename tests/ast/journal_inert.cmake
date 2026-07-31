cmake_minimum_required(VERSION 3.22)
#
# Operation-journal inertness check (portable; runs identically on POSIX and Win32).
#
# The journal is an oracle, not a code path: observing a compile must never move
# a byte of the object produced. This sweeps the exec-golden corpus and asserts
# byte-identity, in one of two modes:
#
#   runtime (no NOJRN): compile each source twice with the SAME compiler, once
#           plain and once under MCC_JOURNAL=1, and fail on any difference. This
#           is the runtime half of inertness -- turning the oracle on at run time
#           must not steer the output it certifies.
#
#   hooks (NOJRN=<mcc-nojrn>): compile each source with the hooked mcc and with a
#           -DMCC_JOURNAL_HOOKS=0 build, and fail on any difference. This is the
#           compile-time half -- the presence of the hooks in the source must not
#           change codegen. An anti-probe first fails outright if the hooks-off
#           binary still journals.
#
# Both modes SKIP (77) when the hooked mcc has no MCC_JOURNAL_HOOKS, so a build
# that simply did not compile the journal in is reported as SKIP, not FAIL.
#
# A third mode selects the Replay_IR side-car instead of the journal:
#
#   rir (RIR=1): same runtime shape, but side B runs under MCC_REPLAY_IR=1. The
#           Replay_IR model is a side-car on the same terms as the journal --
#           observing a compile must not move a byte -- so it is held to the
#           identical bar by the identical driver.
#
# Required -D args: MCC CORPUS EXTRA TMPDIR
# Optional: OPT (default -O1), NOJRN (selects hooks mode when non-empty),
#           RIR (selects Replay_IR mode when non-empty)
#

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
set(_gate_env "MCC_JOURNAL=1")
set(_gate_marker "jrn-verify")
if(_rir_mode)
    set(_gate_env "MCC_REPLAY_IR=1")
    set(_gate_marker "rir-verify")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")

# Pin SOURCE_DATE_EPOCH so __DATE__/__TIME__ expand identically on both sides.
# Without it, a source embedding __TIME__ (e.g. preprocessor/predefined_macros.c)
# yields a different object each second, and the two sides — separate compiler
# processes — flake across a second boundary, masquerading as non-inertness.
set(ENV{SOURCE_DATE_EPOCH} "1000000000")

# In hooks mode, a missing hooks-off binary is an honest SKIP, not a failure.
if(_hooks_mode AND NOT EXISTS "${NOJRN}")
    message(STATUS "journal_inert: ${NOJRN} missing; SKIP")
    cmake_language(EXIT 77)
endif()

# Count the gate's per-function verdict lines a compile emits on stderr with the
# gate on. Which gate and which marker is chosen above by mode.
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
                   "MCC_JOURNAL_HOOKS/MCC_REPLAY_IR; SKIP")
    cmake_language(EXIT 77)
endif()

if(_hooks_mode)
    _jrn_count(_anti "${NOJRN}")
    if(NOT _anti EQUAL 0)
        message(FATAL_ERROR "journal_inert: the -DMCC_JOURNAL_HOOKS=0 build still "
                            "emits [jrn-verify]")
    endif()
endif()

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
list(APPEND _srcs "${EXTRA}")
list(SORT _srcs)

set(_n 0)
set(_bad 0)
set(_diffs "")
foreach(_f ${_srcs})
    # Side A: the hooked mcc, plain.
    execute_process(
        COMMAND "${MCC}" -w "${OPT}" -c -o "${TMPDIR}/a.o" "${_f}"
        OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE _rcA)
    if(NOT _rcA EQUAL 0)
        continue()
    endif()
    # Side B: the same mcc under MCC_JOURNAL=1 (runtime mode) or the hooks-off
    # mcc (hooks mode).
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
elseif(_rir_mode)
    set(_tag "rir_inert(runtime)")
else()
    set(_tag "journal_inert(runtime)")
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
