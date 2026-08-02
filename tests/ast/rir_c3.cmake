cmake_minimum_required(VERSION 3.22)
#
# Replay_IR C3 gate: the optimizer passes must survive an arena the AST hooks
# never built.
#
# C2 asks whether a reconstructed arena re-emits the parser's bytes. C3 asks a
# different question -- whether that arena can be OPTIMIZED. Bytes are
# deliberately NOT the oracle: a pass that folds is expected to change them.
#
# The probe has two halves, and they are compiled in independently:
#
#   PASS EQUIVALENCE (always compiled in) runs the pass pipeline over the tree's
#   arena and over Replay_IR's for the same body and compares the results. This
#   is the half the C3 phase is named for, and it carries the hard assertion:
#
#     samehash == pair    every paired body reaches the same post-pass hash
#
#   ARENA SURVIVAL (needs MCC_REPLAY_IR_C2) clones the reconstruction, runs the
#   passes on the clone -- C2's byte comparison must still see the unoptimized
#   arena -- and re-validates that what comes out is still something
#   ast_replay_* can be handed. Its assertion is likewise a hard zero:
#
#     broke == 0          no body came out of the passes structurally invalid
#
# Everything else -- folds, samefolds, pairfired -- moves with corpus and
# codegen and is printed, not pinned. Pinning it would turn a coverage property
# into a false regression, the same reason rir_parity.cmake refuses to ratchet
# the fallback counts. samefolds in particular is legitimately below pair: two
# arenas can reach the same hash by different fold counts.
#
# Non-vacuity is asserted separately: a run that paired nothing cannot report a
# pass, and the arena-survival assertions are skipped rather than faked when
# that half is not compiled in. Without that, a build which quietly lost
# Replay_IR reports green -- the failure mode this whole family exists to
# prevent.
#
# Required -D args: MCC CORPUS EXTRA TMPDIR
# Optional: OPT (default -O1), MCCFLAGS (default empty)
#
# MCCFLAGS is what makes this runnable against a CROSS compiler: a sysroot, its
# usr/include and an ABI flag. --sysroot alone is not enough -- the explicit
# -I<sysroot>/usr/include is what makes the system headers resolve, and without
# it the sweep silently shrinks to the include-free files and reports a
# plausible, wrong census.
#

if(NOT MCC OR NOT CORPUS OR NOT EXTRA OR NOT TMPDIR)
    message(FATAL_ERROR "rir_c3: MCC, CORPUS, EXTRA, TMPDIR are required")
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

# A build without MCC_REPLAY_IR emits no [rir-total] at all, and one whose C3
# probe is compiled out emits no [rir-c3]; both are honest SKIPs rather than
# vacuous passes.
file(WRITE "${TMPDIR}/probe.c" "int rir_c3_probe(int a, int b) { return a * b + 1; }\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_REPLAY_IR=6"
            "${MCC}" -w "${OPT}" ${_mccflags} -c -o "${TMPDIR}/probe.o" "${TMPDIR}/probe.c"
    OUTPUT_QUIET ERROR_VARIABLE _perr RESULT_VARIABLE _prc)
if(NOT _perr MATCHES "\\[rir-total\\]")
    message(STATUS "rir_c3: no [rir-total] output — build has no MCC_REPLAY_IR; SKIP")
    cmake_language(EXIT 77)
endif()
if(NOT _perr MATCHES "\\[rir-c3\\]")
    message(STATUS "rir_c3: no [rir-c3] output — neither C3 half is compiled in "
                   "for this build; SKIP")
    cmake_language(EXIT 77)
endif()

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
list(APPEND _srcs "${EXTRA}")
list(SORT _srcs)

set(_files 0)
set(_try 0)
set(_ran 0)
set(_folds 0)
set(_broke 0)
set(_pair 0)
set(_samefolds 0)
set(_samehash 0)
set(_pairfired 0)
set(_badlines "")
foreach(_f ${_srcs})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_REPLAY_IR=6"
                "${MCC}" -w "${OPT}" ${_mccflags} -c -o "${TMPDIR}/a.o" "${_f}"
        OUTPUT_QUIET ERROR_VARIABLE _err RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        continue()
    endif()
    # A file that aborted mid-way prints per-body lines but no total, which would
    # silently shrink the census -- count files that produced a total, not files
    # that exited 0.
    if(NOT _err MATCHES "\\[rir-total\\]")
        continue()
    endif()
    math(EXPR _files "${_files} + 1")
    if(_err MATCHES "\\[rir-c3\\] try=([0-9]+) ran=([0-9]+) folds=([0-9]+) broke=([0-9]+) pair=([0-9]+) samefolds=([0-9]+) samehash=([0-9]+) pairfired=([0-9]+)")
        math(EXPR _try "${_try} + ${CMAKE_MATCH_1}")
        math(EXPR _ran "${_ran} + ${CMAKE_MATCH_2}")
        math(EXPR _folds "${_folds} + ${CMAKE_MATCH_3}")
        math(EXPR _broke "${_broke} + ${CMAKE_MATCH_4}")
        math(EXPR _pair "${_pair} + ${CMAKE_MATCH_5}")
        math(EXPR _samefolds "${_samefolds} + ${CMAKE_MATCH_6}")
        math(EXPR _samehash "${_samehash} + ${CMAKE_MATCH_7}")
        math(EXPR _pairfired "${_pairfired} + ${CMAKE_MATCH_8}")
    endif()
    string(REGEX MATCHALL "\\[rir-c3\\][^\n]*INVALID-AFTER-PASSES[^\n]*" _lines "${_err}")
    foreach(_l ${_lines})
        list(APPEND _badlines "${_l}")
    endforeach()
endforeach()

message(STATUS "rir_c3: ${OPT} files=${_files} try=${_try} ran=${_ran} broke=${_broke} "
               "folds=${_folds} pair=${_pair} samefolds=${_samefolds} "
               "samehash=${_samehash} pairfired=${_pairfired}")
foreach(_b ${_badlines})
    message(STATUS "  ${_b}")
endforeach()

if(_files EQUAL 0)
    message(FATAL_ERROR "rir_c3: compiled nothing")
endif()
if(_pair EQUAL 0)
    message(FATAL_ERROR "rir_c3: 0 paired body(ies) — the pass-equivalence "
                        "instrument measured nothing, so a pass here is vacuous")
endif()
if(NOT _samehash EQUAL _pair)
    math(EXPR _div "${_pair} - ${_samehash}")
    message(FATAL_ERROR "rir_c3: ${_div} of ${_pair} paired body(ies) reached a "
                        "DIFFERENT post-pass hash from the tree's arena. The "
                        "passes are arena-parameterized, so the same passes over "
                        "the same body must converge whichever arena carried it")
endif()
# The arena-survival half lives behind MCC_REPLAY_IR_C2. Report honestly which
# assertions actually ran rather than reporting a full pass for half a check.
if(_try EQUAL 0)
    message(STATUS "rir_c3: arena-survival half NOT CHECKED — it needs a "
                   "-DMCC_REPLAY_IR_C2=1 build; pass equivalence was checked")
else()
    if(NOT _ran EQUAL _try)
        math(EXPR _lost "${_try} - ${_ran}")
        message(FATAL_ERROR "rir_c3: ${_lost} of ${_try} body(ies) entered the "
                            "passes and did not come out — the probe aborted "
                            "rather than reporting a verdict")
    endif()
    if(NOT _broke EQUAL 0)
        message(FATAL_ERROR "rir_c3: ${_broke} of ${_try} body(ies) failed "
                            "ast_validate after the optimizer passes ran on a "
                            "reconstructed arena — the arena could not survive "
                            "them, which is not the same as a fold being wrong")
    endif()
    message(STATUS "rir_c3: arena survival OK — ${_ran}/${_try} body(ies), "
                   "${_folds} fold(s), 0 invalid")
endif()
message(STATUS "rir_c3: OK — pass equivalence ${_samehash}/${_pair} "
               "(samefolds ${_samefolds}, pairfired ${_pairfired})")
