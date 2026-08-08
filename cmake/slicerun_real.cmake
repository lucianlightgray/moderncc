# Real arenas, not synthetic ones: compile a slice of the compiler's own corpus
# with MCC_ARENA_DUMP on, turn every lowerable subtree of every recorded body
# into a work item, and require the CPU and device runners to agree tuple for
# tuple. Four teeth, because each one has a distinct way of reporting success
# after measuring nothing.

set(_dump "${BINDIR}/slicerun-arenas.txt")
file(REMOVE "${_dump}")

file(GLOB_RECURSE _srcs "${SRCDIR}/tests/exec/*.c")
list(SORT _srcs)
if(NOT _srcs)
    message(FATAL_ERROR "slice/real: no corpus found under ${SRCDIR}/tests/exec")
endif()
list(LENGTH _srcs _n)
if(_n GREATER 60)
    list(SUBLIST _srcs 0 60 _srcs)
endif()

foreach(_s IN LISTS _srcs)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
                "${MCC}" -c "${_s}" -o "${BINDIR}/slicerun-real.o" -O1
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
endforeach()

if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/real: MCC_ARENA_DUMP produced nothing; the hook is "
                        "not firing and this cell would measure nothing")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --limit 400 --quiet
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")

if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "slice/real: real slices disagree between the CPU and "
                        "device runners")
endif()
if(NOT _out MATCHES "slices=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/real: zero real slices became schedulable work; a "
                        "clean result here would mean the runners never ran")
endif()
# Accepted is not evidence. A run the predicate admits but the kernel builder
# refuses is counted, never dispatched and never compared -- that gap was 174 of
# 300 before it was found, inflating the coverage figure 2.4x. Assert on what a
# device dispatch actually checked.
if(_out MATCHES "available=1")
    if(NOT _out MATCHES "frame-compared=([1-9][0-9]*)")
        message(FATAL_ERROR "slice/real: no frame run was compared against the "
                            "CPU on the device; frame-accepted counts runs that "
                            "were never built and proves nothing")
    endif()
endif()
if(_out MATCHES "available=1")
    if(NOT _out MATCHES "dispatches=([1-9][0-9]*)")
        message(FATAL_ERROR "slice/real: a device was found and never dispatched")
    endif()
    # Known-positive. Without this the cell cannot distinguish "the runners
    # agree" from "the comparison is not wired up".
    execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --limit 400 --quiet
                            --mutate
                    RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
    message("${_mout}")
    if(_mut EQUAL 0)
        message(FATAL_ERROR "slice/real: the mutated device kernel still agreed "
                            "with the CPU runner, so the differential is blind")
    endif()
    message("slice/real: clean OK, mutation detected")
elseif(MCC_GPU_REQUIRED)
    message(FATAL_ERROR "slice/real: no usable device, but MCC_GPU_REQUIRED is "
                        "set -- this cell exists to exercise a device and there "
                        "is none")
else()
    message("slice/real: no usable device; CPU runner verified over real slices")
endif()
