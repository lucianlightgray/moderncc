# D4b step 3: the depth bailout guard, proven to fire rather than truncate.
#
# The expansion bound comes from -fdepth-census, which is an OBSERVATION and not
# a proof, so the thing that has to be tested is not the happy path but the
# excursion: what a slice does when the runtime goes deeper than the instrumented
# maximum. A truncated answer would be identical in both executors and the
# differential would stay green while the result was wrong, so the guard has to
# turn an excursion into "no answer" on both sides at once.
#
# This cell drives the excursion deliberately: MCC_SLICE_INL_DEPTH is set far
# below the depth the fixture actually recurses to, so every deep call reaches an
# AST_Bailout. Four teeth:
#
#   1. depth-guards-emitted > 0   the compiler planted guards at the bound.
#   2. depth-guards-hit > 0       the CPU reference REACHED one at run time, i.e.
#                                 the excursion is real and not hypothetical. A
#                                 cell that only checked "no mismatch" could not
#                                 tell a fired guard from a guard never reached.
#   3. frame-mismatches = 0       and the device agreed with it. The guard clears
#      with depth-guards-gpu > 0  the same per-lane definedness flag the loop
#                                 trip cap already uses, so both executors report
#                                 undefined for the same tuples; a device that
#                                 truncated instead would show up here.
#   4. --mutate reddens           the comparison is not blind.
#
# The A/B arm (MCC_SLICE_INL_DEPTH unset) must plant no guards at all, which is
# what makes tooth 1 attributable to the depth expansion rather than to anything
# else in the pipeline.

set(_on "${BINDIR}/mcc-depth-bailout-on.txt")
set(_off "${BINDIR}/mcc-depth-bailout-off.txt")
file(REMOVE "${_on}" "${_off}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_on}"
            "MCC_SLICE_INL_DEPTH=3"
            "${MCC}" -w -O2 -c "${SRCDIR}/tests/gpu/depth_rec.c"
            -o "${BINDIR}/mcc-depth-bailout.o"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _log ERROR_VARIABLE _log)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_on}")
    message(FATAL_ERROR "mcc-depth-bailout: the expanding compile failed\n${_log}")
endif()
message("${_log}")
if(NOT _log MATCHES "rec-grafts=([1-9][0-9]*)")
    message(FATAL_ERROR "mcc-depth-bailout: no recursive graft fired, so the "
                        "depth expansion never ran\n${_log}")
endif()
set(_rec "${CMAKE_MATCH_1}")
if(NOT _log MATCHES "bailouts=([1-9][0-9]*)")
    message(FATAL_ERROR "mcc-depth-bailout: the expansion planted no guard at "
                        "its bound, so an excursion would truncate\n${_log}")
endif()
set(_bail "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_off}"
            "${MCC}" -w -O2 -c "${SRCDIR}/tests/gpu/depth_rec.c"
            -o "${BINDIR}/mcc-depth-bailout.o"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _log2 ERROR_VARIABLE _log2)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_off}")
    message(FATAL_ERROR "mcc-depth-bailout: the unexpanded compile failed\n${_log2}")
endif()
if(NOT _log2 MATCHES "bailouts=0")
    message(FATAL_ERROR "mcc-depth-bailout: guards appeared without "
                        "MCC_SLICE_INL_DEPTH, so tooth 1 is not attributable to "
                        "the depth expansion\n${_log2}")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_on}" --quiet --no-inline
                RESULT_VARIABLE _on_rc OUTPUT_VARIABLE _ontxt ERROR_VARIABLE _ontxt)
message("${_ontxt}")
if(_on_rc EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "mcc-depth-bailout: nothing to compare on this "
                            "backend, but MCC_GPU_REQUIRED is set")
    endif()
    message("mcc-depth-bailout: this backend emits no frame kernel, skipping")
    cmake_language(EXIT 77)
endif()
if(NOT _on_rc EQUAL 0)
    message(FATAL_ERROR "mcc-depth-bailout: the unmutated differential is "
                        "already failing\n${_ontxt}")
endif()

if(NOT _ontxt MATCHES "depth-guards-emitted=([0-9]+) depth-guards-hit=([0-9]+) depth-guards-gpu=([0-9]+)")
    message(FATAL_ERROR "mcc-depth-bailout: no depth-guards line in the run")
endif()
set(_emit "${CMAKE_MATCH_1}")
set(_hit "${CMAKE_MATCH_2}")
set(_gpu "${CMAKE_MATCH_3}")

if(_emit LESS 1)
    message(FATAL_ERROR "mcc-depth-bailout: the dump carried no guard node, so "
                        "the compiler's guard never reached the runner")
endif()
if(_hit LESS 1)
    message(FATAL_ERROR "mcc-depth-bailout: no guard was REACHED at run time "
                        "(${_emit} planted). Every tuple stayed inside the "
                        "bound, so this cell proved nothing about the "
                        "excursion it exists to test")
endif()
if(NOT _ontxt MATCHES "frame-mismatches=0")
    message(FATAL_ERROR "mcc-depth-bailout: the two executors disagreed while "
                        "guards were firing, so one of them is truncating "
                        "instead of reporting undefined\n${_ontxt}")
endif()

if(NOT _ontxt MATCHES "available=1")
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "mcc-depth-bailout: no usable device, but "
                            "MCC_GPU_REQUIRED is set")
    endif()
    message("mcc-depth-bailout: no usable device; CPU reference verified "
            "(${_rec} recursive grafts, ${_bail} guards planted, ${_hit} hit)")
    return()
endif()
if(_gpu LESS 1)
    message(FATAL_ERROR "mcc-depth-bailout: a device is present and ${_hit} "
                        "guards fired on the CPU reference, but no guard was "
                        "emitted into a kernel, so the device arm never carried "
                        "the bailout and its agreement is vacuous")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_on}" --quiet --no-inline --mutate
                OUTPUT_VARIABLE _mut ERROR_VARIABLE _mut)
message("${_mut}")
if(NOT _mut MATCHES "frame-mismatches=([1-9][0-9]*)")
    message(FATAL_ERROR "mcc-depth-bailout: every frame kernel was perturbed and "
                        "the differential still reported clean, so the guarded "
                        "slices are not actually being compared")
endif()

message("mcc-depth-bailout: ${_rec} recursive grafts, ${_bail} guards planted, "
        "${_emit} in the dump, ${_hit} reached at run time, ${_gpu} emitted "
        "into kernels, both executors agreed")
