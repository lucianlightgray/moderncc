execute_process(COMMAND "${FS}" devgate "${MCC}" "${BDIR}" "${IDIR}" "${WORK}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "flagsweep/dev-gate-known-positive: the unmutated gate "
                        "check is already failing, so this cell cannot say "
                        "whether it observes the refusal at all")
endif()

set(ENV{MCC_DEV} "1")
execute_process(COMMAND "${FS}" devgate "${MCC}" "${BDIR}" "${IDIR}" "${WORK}-mut"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
unset(ENV{MCC_DEV})
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "flagsweep/dev-gate-known-positive: the gate was "
                        "disarmed by putting MCC_DEV=1 in the environment the "
                        "refusal phase runs in, every gated -f flag was "
                        "accepted, and flagsweep/dev-gate still passed. It is "
                        "asserting nothing, which is the shape nine slice/* "
                        "cells shipped in: Passed while executing zero checks")
endif()
message("flagsweep/dev-gate-known-positive: clean OK, disarmed gate detected")
