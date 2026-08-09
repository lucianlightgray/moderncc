# Known-positive for a device differential suite. Every built kernel returns a
# value one bit wrong; if the suite still reports clean, the comparison is not
# wired up and a passing cell would mean nothing.
#
# --device-or-skip is not optional here. Suites whose CPU half runs without a
# device (frame, sched, bytes) exit 0 on a device-less host with both arms, and
# this driver would then report "the mutant survived, so it is blind" about a
# host that never dispatched anything. Asking the runner to exit 77 up front is
# what keeps the verdict about the differential rather than about the host.

execute_process(COMMAND "${RUNNER}" ${SUITE} --device-or-skip RESULT_VARIABLE _clean
                OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/${SUITE}-known-positive: no usable device, but "
                            "MCC_GPU_REQUIRED is set")
    endif()
    message("slice/${SUITE}-known-positive: no usable device, skipping")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "slice/${SUITE}-known-positive: the unmutated "
                        "differential is already failing")
endif()

execute_process(COMMAND "${RUNNER}" ${SUITE} --device-or-skip --mutate RESULT_VARIABLE _mut
                OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "slice/${SUITE}-known-positive: every ${SUITE} kernel was "
                        "perturbed and the differential still reported clean, so "
                        "it is blind")
endif()
# 77 from the mutated run means the runner found nothing to perturb on this
# device -- not that it perturbed something and caught it. Without this branch
# any non-zero counted as "mutation detected", so the cell reported a positive
# result about a comparison that never happened. slice/f64 on a device without
# shaderFloat64 (MoltenVK on Apple silicon) is the case that surfaced it.
if(_mut EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/${SUITE}-known-positive: nothing mutable on this "
                            "device, but MCC_GPU_REQUIRED is set")
    endif()
    message("slice/${SUITE}-known-positive: nothing mutable on this device, skipping")
    cmake_language(EXIT 77)
endif()
message("slice/${SUITE}-known-positive: clean OK, mutation detected")
