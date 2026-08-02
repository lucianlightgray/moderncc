cmake_minimum_required(VERSION 3.22)
#
# Replay_IR faithfulness gate (portable; POSIX and Win32).
#
# Instruction 36's deletion bar is absolute: 100% of bodies faithful on every
# target at every -O. That makes this a hard assert rather than a ratchet
# against a banked number -- unlike the journal, which needs an honesty bank
# because it legitimately carries raw blobs on some targets, Replay_IR has one
# permitted verdict per body:
#
#   rfaithful  replayed byte-identically against the parser's own output
#   rempty     the body emitted no bytes, so there was nothing to reproduce
#
# Anything else is a failure. The gate therefore asserts faithful + empty == fn,
# and separately that the sweep measured something at all, so a run that
# journalled nothing cannot report a vacuous pass (instruction 18).
#
# It deliberately does NOT ratchet the fallback / label / region counts. Those
# move with corpus and codegen and are target-specific by design -- i386 and arm
# report 483 chain fallbacks where x86_64 reports 7, because a value wider than
# the target register lowers its comparison chain inside a single journalled op.
# Pinning them here would turn a word-width property into a false regression.
#
# Required -D args: MCC CORPUS EXTRA TMPDIR
# Optional: OPT (default -O1), MCCFLAGS (default empty), FORCE + SRCDIR
#
# MCCFLAGS is what makes this runnable against a CROSS compiler: a sysroot, its
# usr/include and an ABI flag. --sysroot alone is not enough -- the explicit
# -I<sysroot>/usr/include is what makes the system headers resolve, and without
# it the sweep silently shrinks to the include-free files and reports a
# plausible, wrong census (journal_sweep.cmake:167 records the same trap).
#
# FORCE is what makes -O0 measurable at all. ast_replay_env needs optimize >= 1,
# so a bare -O0 sweep journals nothing and reports fn=0 -- caught below as a
# vacuous pass rather than mistaken for one. MCC_FORCE_REPLAY=1 lifts that, but
# alone it measures a different compiler: the gates whose default is
# `o4 || optimize >= 1` are all off at -O0, so the replay is asked to reproduce
# an emit no other cell covers. Forcing every one of them on is what makes the
# -O0 census comparable to -O1's. The list is DERIVED from SRCDIR rather than
# copied, because a copied list silently shrinks as gates are added or renamed,
# and a short list reads as a pass over fewer bodies rather than as a defect.
#

if(NOT MCC OR NOT CORPUS OR NOT EXTRA OR NOT TMPDIR)
    message(FATAL_ERROR "rir_parity: MCC, CORPUS, EXTRA, TMPDIR are required")
endif()
if(NOT OPT)
    set(OPT "-O1")
endif()
set(_mccflags "")
if(MCCFLAGS)
    separate_arguments(_mccflags NATIVE_COMMAND "${MCCFLAGS}")
endif()

# Env prefix shared by the probe and every sweep compile. Both must carry it or
# the probe decides SKIP against a different compiler than the sweep measures.
set(_env "MCC_REPLAY_IR=1")
if(FORCE)
    if(NOT SRCDIR)
        message(FATAL_ERROR "rir_parity: FORCE requires SRCDIR to derive the "
                            "optimize>=1 gate list from")
    endif()
    file(GLOB _gsrcs "${SRCDIR}/*.c")
    set(_gates "")
    foreach(_gs ${_gsrcs})
        file(READ "${_gs}" _gtext)
        string(REGEX MATCHALL
               "ast_env_gate\\(\"MCC_AST_[A-Z0-9_]+\", *o4 \\|\\| s1->optimize >= 1\\)"
               _gm "${_gtext}")
        foreach(_g ${_gm})
            string(REGEX REPLACE "^.*\"(MCC_AST_[A-Z0-9_]+)\".*$" "\\1" _gn "${_g}")
            list(APPEND _gates "${_gn}")
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _gates)
    list(SORT _gates)
    list(LENGTH _gates _ngates)
    if(_ngates EQUAL 0)
        message(FATAL_ERROR "rir_parity: FORCE derived 0 gates from ${SRCDIR} — "
                            "the ast_env_gate spelling changed, so this run "
                            "would measure -O0 with every pass off and call it "
                            "parity")
    endif()
    list(APPEND _env "MCC_FORCE_REPLAY=1")
    foreach(_g ${_gates})
        list(APPEND _env "${_g}=1")
    endforeach()
    message(STATUS "rir_parity: FORCE — ${_ngates} optimize>=1 gate(s) forced on")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")
set(ENV{SOURCE_DATE_EPOCH} "1000000000")

# A build without MCC_REPLAY_IR emits no [rir-total] at all; that is an honest
# SKIP rather than a vacuous pass.
file(WRITE "${TMPDIR}/probe.c" "int rir_probe(int a, int b) { return a * b + 1; }\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env ${_env}
            "${MCC}" -w "${OPT}" ${_mccflags} -c -o "${TMPDIR}/probe.o" "${TMPDIR}/probe.c"
    OUTPUT_QUIET ERROR_VARIABLE _perr RESULT_VARIABLE _prc)
if(NOT _perr MATCHES "\\[rir-total\\]")
    message(STATUS "rir_parity: no [rir-total] output — build has no "
                   "MCC_REPLAY_IR; SKIP")
    cmake_language(EXIT 77)
endif()

file(GLOB_RECURSE _srcs "${CORPUS}/*.c")
list(APPEND _srcs "${EXTRA}")
list(SORT _srcs)

set(_fn 0)
set(_faithful 0)
set(_empty 0)
set(_unbal 0)
set(_ovf 0)
set(_files 0)
set(_badnames "")
foreach(_f ${_srcs})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env ${_env}
                "${MCC}" -w "${OPT}" ${_mccflags} -c -o "${TMPDIR}/a.o" "${_f}"
        OUTPUT_QUIET ERROR_VARIABLE _err RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        continue()
    endif()
    # A file that aborted mid-way prints per-body verdicts but no total, which
    # would silently shrink the census -- count files that produced a total, not
    # files that exited 0.
    if(NOT _err MATCHES "\\[rir-total\\]")
        continue()
    endif()
    math(EXPR _files "${_files} + 1")
    if(_err MATCHES "\\[rir-total\\] fn=([0-9]+) faithful=([0-9]+)")
        math(EXPR _fn "${_fn} + ${CMAKE_MATCH_1}")
        math(EXPR _faithful "${_faithful} + ${CMAKE_MATCH_2}")
    endif()
    if(_err MATCHES "unbal=([0-9]+) ovf=([0-9]+)")
        math(EXPR _unbal "${_unbal} + ${CMAKE_MATCH_1}")
        math(EXPR _ovf "${_ovf} + ${CMAKE_MATCH_2}")
    endif()
    string(REGEX MATCHALL "\\[rir-verify\\][ \t]+rempty" _es "${_err}")
    list(LENGTH _es _en)
    math(EXPR _empty "${_empty} + ${_en}")
    string(REGEX MATCHALL "\\[rir-verify\\][ \t]+r(unfaithful|desync|bail)[^\n]*"
           _lines "${_err}")
    foreach(_l ${_lines})
        list(APPEND _badnames "${_l}")
    endforeach()
endforeach()

math(EXPR _accounted "${_faithful} + ${_empty}")
message(STATUS "rir_parity: ${OPT} files=${_files} bodies=${_fn} "
               "faithful=${_faithful} empty=${_empty} "
               "unbalanced=${_unbal} overflow=${_ovf}")
foreach(_b ${_badnames})
    message(STATUS "  NOT FAITHFUL ${_b}")
endforeach()

if(_files EQUAL 0)
    message(FATAL_ERROR "rir_parity: compiled nothing")
endif()
if(_fn EQUAL 0)
    message(FATAL_ERROR "rir_parity: 0 bodies journalled — the instrument "
                        "measured nothing, so a pass here is vacuous")
endif()
if(NOT _unbal EQUAL 0)
    message(FATAL_ERROR "rir_parity: ${_unbal} body(ies) with unbalanced region "
                        "markers — a region begin without its end, or an end "
                        "that unwound past its own kind")
endif()
if(NOT _ovf EQUAL 0)
    message(FATAL_ERROR "rir_parity: ${_ovf} body(ies) overflowed the region "
                        "stack")
endif()
if(NOT _accounted EQUAL _fn)
    math(EXPR _missing "${_fn} - ${_accounted}")
    message(FATAL_ERROR "rir_parity: ${_missing} of ${_fn} body(ies) are neither "
                        "faithful nor empty. Instruction 36's bar is 100% on "
                        "every target at every -O, so this is a hard failure, "
                        "not a ratchet")
endif()
message(STATUS "rir_parity: OK — ${_accounted}/${_fn} (${_faithful} faithful + "
               "${_empty} empty)")
