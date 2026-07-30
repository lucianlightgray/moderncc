cmake_minimum_required(VERSION 3.22)
#
# AST recorder-fidelity ratchet.
#
# Sweeps the exec-golden corpus under MCC_AST_VERIFY=1 and compares the set of
# non-faithful functions (desync / unfaithful / stackresidue) against a checked-in
# per-target baseline. Fails on any drift so the gap set can only shrink:
#   - a function that regressed into a gap not in the baseline (a real regression)
#   - a baseline gap that is now faithful but still listed (regenerate to record the win)
#
# Regenerate the baseline after intentionally changing the gap set:
#   ctest -R ast-verify-ratchet ...            (fails, prints the diff)
#   cmake -DMCC=<mcc> -DCORPUS=<dir> -DEXTRA=<file;...> -DBASELINE=<file> \
#         -DTMPDIR=<dir> -DREGEN=1 -P tests/ast/verify_ratchet.cmake
#
# Required -D args: MCC CORPUS BASELINE TMPDIR
# Optional: EXTRA (extra ;-list of sources), REGEN (0/1), OPT (default -O2),
#           JOURNAL (0/1)
#
# OPT selects the sweep optimization level. The recorder-modelling gates are
# enabled from -O1 up, so the -O1 and -O2 gap sets are identical and both levels
# share one baseline; sweeping each guards against them drifting apart.
#
# JOURNAL COLUMN
#
# The same sweep runs with MCC_JOURNAL=1, so the operation journal's per-function
# verdicts arrive on the same stderr as the recorder's and cost one extra pass
# over the vstack rather than a second compile. The journal is an oracle running
# ALONGSIDE the tree recorder, gated off by default, and it does not replace the
# tree until it is at parity or better -- this column is the mechanism that
# decides that, mechanically and every run, instead of a hand sweep.
#
# The invariant: a function the TREE reproduces byte-faithfully must also be one
# the JOURNAL reproduces byte-faithfully. Journal regression on ground the tree
# already holds is a hard failure. The converse -- journal faithful where the
# tree is not -- is the expected win and is only counted, never failed on; that
# count IS the parity metric, reported as a delta each run.
#
# `jempty` (the journal recorded no operation) is accepted opposite a faithful
# tree: it means the body emitted zero bytes, which replays vacuously. The two
# systems disagree only on naming there -- the recorder calls a zero-byte body
# `faithful` and reserves `empty` for a childless tree that did NOT replay.
#
# With JOURNAL=1 the script checks ONLY that column and SKIPs (77) when the
# build has no MCC_JOURNAL_HOOKS, so the parity test never fails a build that
# simply did not compile the journal in. Without it the column is reported but
# not enforced, and the baseline ratchet is the verdict.
#

if(NOT MCC OR NOT CORPUS OR NOT BASELINE OR NOT TMPDIR)
    message(FATAL_ERROR "verify_ratchet: MCC, CORPUS, BASELINE, TMPDIR are required")
endif()

if(NOT OPT)
    set(OPT "-O2")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
if(EXTRA)
    list(APPEND _srcs ${EXTRA})
endif()
list(SORT _srcs)

set(_gaps "")
set(_ast_faithful "")   # keys the tree reproduces byte-faithfully
set(_ast_tried 0)       # recorder rows that are not skip:* (i.e. journal ran too)
set(_jrn_rows 0)
set(_jrn_faithful 0)
set(_jrn_empty 0)
set(_jrn_bad "")        # keys whose journal verdict is a failure
foreach(_f ${_srcs})
    file(RELATIVE_PATH _rel "${CORPUS}" "${_f}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_AST_VERIFY=1" "MCC_AST_TEMPLATES=0"
                "MCC_JOURNAL=1"
                "${MCC}" -w "${OPT}" -c -o "${TMPDIR}/verify_sweep.o" "${_f}"
        OUTPUT_QUIET ERROR_VARIABLE _err RESULT_VARIABLE _rc)
    string(REPLACE "\n" ";" _lines "${_err}")
    foreach(_ln ${_lines})
        # "[ast-verify] <verdict>[:<line>]\t?\t<func>"
        if(_ln MATCHES "\\[ast-verify\\] (desync|unfaithful|stackresidue|bail)(:[0-9]+)?\t[^\t]*\t(.+)")
            list(APPEND _gaps "${_rel}\t${CMAKE_MATCH_3}\t${CMAKE_MATCH_1}")
        endif()
        if(_ln MATCHES "\\[ast-verify\\] ([^\t]+)\t[^\t]*\t(.+)")
            set(_v "${CMAKE_MATCH_1}")
            if(NOT _v MATCHES "^skip:")
                math(EXPR _ast_tried "${_ast_tried} + 1")
            endif()
            if(_v STREQUAL "faithful")
                list(APPEND _ast_faithful "${_rel}\t${CMAKE_MATCH_2}")
            endif()
        endif()
        # "[jrn-verify] <verdict>\t?\t<func>\tops=..."
        if(_ln MATCHES "\\[jrn-verify\\] ([^\t]+)\t[^\t]*\t([^\t]+)\tops=")
            set(_v "${CMAKE_MATCH_1}")
            math(EXPR _jrn_rows "${_jrn_rows} + 1")
            if(_v STREQUAL "jfaithful")
                math(EXPR _jrn_faithful "${_jrn_faithful} + 1")
            elseif(_v STREQUAL "jempty")
                math(EXPR _jrn_empty "${_jrn_empty} + 1")
            else()
                list(APPEND _jrn_bad "${_rel}\t${CMAKE_MATCH_2}\t${_v}")
            endif()
        endif()
    endforeach()
endforeach()
list(SORT _gaps)
list(JOIN _gaps "\n" _current)

list(LENGTH _ast_faithful _n_ast_faithful)

# The journal column. `_jrn_ok` is jfaithful plus the vacuous jempty; the delta
# against the tree is the parity metric this whole column exists to publish.
math(EXPR _jrn_ok "${_jrn_faithful} + ${_jrn_empty}")
math(EXPR _delta "${_jrn_ok} - ${_n_ast_faithful}")
set(_parity_violations "")
foreach(_b ${_jrn_bad})
    string(REGEX REPLACE "\t[^\t]+$" "" _k "${_b}")
    if(_k IN_LIST _ast_faithful)
        list(APPEND _parity_violations "${_b}")
    endif()
endforeach()

if(_jrn_rows GREATER 0)
    message(STATUS "verify_ratchet: parity ${OPT} — tree faithful ${_n_ast_faithful}, "
                   "journal faithful ${_jrn_ok} (${_jrn_faithful} + ${_jrn_empty} vacuous) "
                   "of ${_jrn_rows} journalled, delta ${_delta}")
endif()

if(JOURNAL)
    if(_jrn_rows EQUAL 0)
        message(STATUS "verify_ratchet: no [jrn-verify] rows — this build has no "
                       "MCC_JOURNAL_HOOKS; journal column SKIP")
        cmake_language(EXIT 77)
    endif()
    # A journal that quietly stops observing functions would show perfect parity
    # on the handful it still sees, so require it to have covered every function
    # the recorder actually attempted.
    if(NOT _jrn_rows EQUAL _ast_tried)
        message(FATAL_ERROR "verify_ratchet: journal observed ${_jrn_rows} functions but the "
                            "recorder attempted ${_ast_tried} — the journal hooks are not firing "
                            "on every body")
    endif()
    if(_parity_violations)
        message(STATUS "verify_ratchet: journal REGRESSED on functions the tree reproduces:")
        foreach(_v ${_parity_violations})
            message(STATUS "  ! ${_v}")
        endforeach()
        list(LENGTH _parity_violations _nv)
        message(FATAL_ERROR "verify_ratchet: ${_nv} parity violation(s) — the journal must be "
                            "faithful wherever the tree is")
    endif()
    list(LENGTH _jrn_bad _n_jrn_bad)
    message(STATUS "verify_ratchet: journal parity OK — ${_n_jrn_bad} non-faithful journal "
                   "verdict(s), none on ground the tree holds")
    return()
endif()

if(REGEN)
    file(WRITE "${BASELINE}" "${_current}\n")
    list(LENGTH _gaps _n)
    message(STATUS "verify_ratchet: wrote ${_n} baseline gaps to ${BASELINE}")
    return()
endif()

if(NOT EXISTS "${BASELINE}")
    message(STATUS "verify_ratchet: no baseline for this target (${BASELINE}); SKIP — regenerate with REGEN=1")
    cmake_language(EXIT 77)
endif()

file(READ "${BASELINE}" _base_raw)
string(REPLACE "\r" "" _base_raw "${_base_raw}")
string(STRIP "${_base_raw}" _base_raw)
set(_baseline "")
if(NOT _base_raw STREQUAL "")
    string(REPLACE "\n" ";" _baseline "${_base_raw}")
endif()
list(SORT _baseline)

# Compare on the (file, func) key only, not the verdict: a gap being reclassified
# (e.g. unfaithful -> desync as the recorder learns to decline a shape it cannot
# model) is not a regression. The verdict stays in the baseline as information.
set(_gap_keys "")
foreach(_g ${_gaps})
    string(REGEX REPLACE "\t[^\t]+$" "" _k "${_g}")
    list(APPEND _gap_keys "${_k}")
endforeach()
set(_base_keys "")
foreach(_b ${_baseline})
    string(REGEX REPLACE "\t[^\t]+$" "" _k "${_b}")
    list(APPEND _base_keys "${_k}")
endforeach()
set(_new_gaps "")
foreach(_g ${_gaps})
    string(REGEX REPLACE "\t[^\t]+$" "" _k "${_g}")
    if(NOT _k IN_LIST _base_keys)
        list(APPEND _new_gaps "${_g}")
    endif()
endforeach()
set(_fixed "")
foreach(_b ${_baseline})
    string(REGEX REPLACE "\t[^\t]+$" "" _k "${_b}")
    if(NOT _k IN_LIST _gap_keys)
        list(APPEND _fixed "${_b}")
    endif()
endforeach()

list(LENGTH _gaps _ng)
list(LENGTH _baseline _nb)
message(STATUS "verify_ratchet: ${_ng} gaps now, ${_nb} in baseline")

if(_new_gaps OR _fixed)
    if(_new_gaps)
        message(STATUS "verify_ratchet: NEW recorder-fidelity gaps (regressions):")
        foreach(_g ${_new_gaps})
            message(STATUS "  + ${_g}")
        endforeach()
    endif()
    if(_fixed)
        message(STATUS "verify_ratchet: baseline gaps now FAITHFUL (regenerate to bank the win):")
        foreach(_g ${_fixed})
            message(STATUS "  - ${_g}")
        endforeach()
    endif()
    message(FATAL_ERROR "verify_ratchet: gap set drifted from baseline (see above)")
endif()

message(STATUS "verify_ratchet: gap set matches baseline — OK")
