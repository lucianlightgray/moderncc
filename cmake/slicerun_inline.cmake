# D4b step 1: the first call-boundary increment, and the three things that have
# to be true before it counts as one.
#
# The board's hard precondition is that the CPU reference has no AST_Invoke case
# at all -- zero occurrences in ast_eval_slice.h and mccslice.h -- so a
# differential over an invoke-bearing arena is vacuous, not merely weak. The
# increment taken here removes the Invoke from the shared tree instead of
# teaching one executor about it, which is why the reference arm and the
# emitter arm are the same arm and land together by construction.
#
# Three teeth, because each of the first two can pass while measuring nothing:
#
#   1. invoke-inlined > 0     the graft fired at all.
#   2. frame-stmts strictly   without inlining this corpus compares two frame
#      greater than without   runs with ZERO statements between them -- Return
#      inlining               only. A mutation that perturbs stores cannot be
#                             seen by a run with no stores, so the un-inlined
#                             arm is provably blind and the cell would report
#                             success over an empty comparison.
#   3. --mutate goes red on   the device kernel returns one bit wrong. This is
#      frame-mismatches       asserted on frame-mismatches specifically, not on
#      specifically           the exit code: the expression-slice arm mutates
#                             too and would redden the process on its own,
#                             which would let a completely unwired frame
#                             comparison ride along on someone else's failure.

set(_dump "${BINDIR}/slicerun-inline.txt")
file(REMOVE "${_dump}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
            "${MCC}" -c "${SRCDIR}/tests/gpu/inline_leaf.c"
            -o "${BINDIR}/slicerun-inline.o" -O1
    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/inline: MCC_ARENA_DUMP produced nothing for the "
                        "leaf-callee fixture")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet --no-inline
                RESULT_VARIABLE _base_rc OUTPUT_VARIABLE _base ERROR_VARIABLE _base)
message("${_base}")
execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet
                RESULT_VARIABLE _inl_rc OUTPUT_VARIABLE _inl ERROR_VARIABLE _inl)
message("${_inl}")

# 77 from the runner means this backend emits nothing the suite exercises --
# the Metal arm has no frame kernel builder (TODO.md §5 stage M2). Treating it
# as failure graded the backend instead of the differential.
if(${_base_rc} EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/inline: the runner reports nothing to compare on "
                            "this backend, but MCC_GPU_REQUIRED is set")
    endif()
    message("slice/inline: this backend emits nothing this cell compares, skipping")
    cmake_language(EXIT 77)
endif()

if(NOT _base_rc EQUAL 0 OR NOT _inl_rc EQUAL 0)
    message(FATAL_ERROR "slice/inline: the unmutated differential is already "
                        "failing")
endif()
if(NOT _inl MATCHES "invoke-inlined=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/inline: no AST_Invoke was inlined, so the arm "
                        "under test never ran")
endif()
if(NOT _base MATCHES "frame-stmts=([0-9]+)")
    message(FATAL_ERROR "slice/inline: no frame-stmts line in the baseline run")
endif()
set(_bs "${CMAKE_MATCH_1}")
if(NOT _inl MATCHES "frame-stmts=([0-9]+)")
    message(FATAL_ERROR "slice/inline: no frame-stmts line in the inlined run")
endif()
set(_is "${CMAKE_MATCH_1}")
if(NOT _is GREATER _bs)
    message(FATAL_ERROR "slice/inline: inlining did not increase the number of "
                        "frame statements compared (${_bs} -> ${_is}); the graft "
                        "fired but nothing new reached either executor")
endif()

if(NOT _inl MATCHES "available=1")
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/inline: no usable device, but MCC_GPU_REQUIRED "
                            "is set")
    endif()
    message("slice/inline: no usable device; CPU runner verified over the graft")
    return()
endif()
if(NOT _inl MATCHES "frame-compared=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/inline: no frame run was compared on the device")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet --mutate
                OUTPUT_VARIABLE _mut ERROR_VARIABLE _mut)
message("${_mut}")
if(NOT _mut MATCHES "frame-mismatches=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/inline: every frame kernel was perturbed and the "
                        "frame differential still reported clean, so the inlined "
                        "call is not actually being compared")
endif()
execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet --no-inline --mutate
                OUTPUT_VARIABLE _bmut ERROR_VARIABLE _bmut)
message("${_bmut}")
if(NOT _bmut MATCHES "frame-mismatches=0")
    message(FATAL_ERROR "slice/inline: the un-inlined arm already reddens on "
                        "mutation, so the mutation above is not attributable to "
                        "the graft")
endif()
message("slice/inline: clean OK, mutation detected, and blind without the graft")
