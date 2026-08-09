# Known-positive for the effect record/replay self-check. Unlike the device
# differentials this arm needs no device: the executor and the replayed side are
# both the CPU reference, so --device-or-skip is deliberately NOT passed and the
# cell can never report a host property as a verdict about the replayer.
#
# The mutated arm perturbs the recorded log itself. If a replay against a log
# that no longer describes what the executor did still reports clean, the
# replayer is not comparing anything and every effectful differential built on
# top of it would pass vacuously.

execute_process(COMMAND "${RUNNER}" effect RESULT_VARIABLE _clean
                OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "slice/effect-known-positive: the unmutated record/replay "
                        "self-check is already failing")
endif()

execute_process(COMMAND "${RUNNER}" effect --mutate RESULT_VARIABLE _mut
                OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "slice/effect-known-positive: the recorded effect log was "
                        "perturbed and record/replay still reported clean, so the "
                        "replayer is blind")
endif()
if(_mut EQUAL 77)
    message(FATAL_ERROR "slice/effect-known-positive: the mutated arm skipped; a "
                        "CPU-only self-check has nothing to skip on")
endif()
message("slice/effect-known-positive: clean OK, perturbed log detected")
