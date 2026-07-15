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
# Optional: EXTRA (extra ;-list of sources), REGEN (0/1)
#

if(NOT MCC OR NOT CORPUS OR NOT BASELINE OR NOT TMPDIR)
    message(FATAL_ERROR "verify_ratchet: MCC, CORPUS, BASELINE, TMPDIR are required")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
if(EXTRA)
    list(APPEND _srcs ${EXTRA})
endif()
list(SORT _srcs)

set(_gaps "")
foreach(_f ${_srcs})
    file(RELATIVE_PATH _rel "${CORPUS}" "${_f}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_AST_VERIFY=1" "MCC_AST_TEMPLATES=0"
                "${MCC}" -w -O2 -c -o "${TMPDIR}/verify_sweep.o" "${_f}"
        OUTPUT_QUIET ERROR_VARIABLE _err RESULT_VARIABLE _rc)
    string(REPLACE "\n" ";" _lines "${_err}")
    foreach(_ln ${_lines})
        # "[ast-verify] <verdict>\t?\t<func>"
        if(_ln MATCHES "\\[ast-verify\\] (desync|unfaithful|stackresidue)\t[^\t]*\t(.+)")
            list(APPEND _gaps "${_rel}\t${CMAKE_MATCH_2}\t${CMAKE_MATCH_1}")
        endif()
    endforeach()
endforeach()
list(SORT _gaps)
list(JOIN _gaps "\n" _current)

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

set(_new_gaps "")
foreach(_g ${_gaps})
    if(NOT _g IN_LIST _baseline)
        list(APPEND _new_gaps "${_g}")
    endif()
endforeach()
set(_fixed "")
foreach(_b ${_baseline})
    if(NOT _b IN_LIST _gaps)
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
