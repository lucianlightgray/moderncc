# The CPU reference against a cross oracle.
#
# ast_eval_slice is a second implementation of C integer semantics and until
# this cell it was only ever checked against the device -- two runners built
# from one reading of one tree. This re-emits every accepted expression slice as
# C, hands it to gcc and clang at two optimisation levels, and requires the
# reference's answer to survive. It runs whether or not a device is present,
# which matters because the device backend is frozen and the frame slicer has no
# caller in src/.
#
# Four teeth: the corpus must resolve, a floor of tuples must actually be
# re-checked, the mutation arm must be detected, and a device that IS present
# must have dispatched.

if(NOT EXISTS "${CORPUS}")
    message("slice/cref-oracle: SKIP: no corpus at ${CORPUS}")
    return()
endif()

execute_process(COMMAND "${PY}" "${TOOL}"
                        --corpus "${CORPUS}" --mcc "${MCC}"
                        --slicerun "${RUNNER}"
                        --oracle-cc "${ORACLE_CC}" --suite-cc "${SUITE_CC}"
                        --work "${WORK}" --quiet --recursive
                        --min-adjudicated "${MINPROG}"
                        --min-cref-tuples "${MINTUPLE}"
                        "--cflags=${CFLAGS}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")

if(_clean EQUAL 77)
    message("slice/cref-oracle: SKIP: ${_out}")
    return()
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR
            "slice/cref-oracle: the CPU reference disagreed with an oracle, or "
            "a floor was not met. ast_eval_slice is a second implementation of "
            "C semantics; a value disagreement here is a miscompile and outranks "
            "any coverage number in this cell:\n${_out}")
endif()

# A run that adjudicated nothing prints the same OK line as a run that checked
# the whole corpus. Assert on the counts, not on the exit status.
if(NOT _out MATCHES "qualified=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/cref-oracle: no program survived cross-oracle "
                        "qualification, so every figure below is over an empty "
                        "denominator")
endif()
if(NOT _out MATCHES "cref-oracle [^ ]+ ok=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/cref-oracle: no oracle compiled and ran a single "
                        "re-emitted slice; the reference was compared against "
                        "nothing")
endif()

# `accepted` is not evidence and neither is `available`. If a device came up, it
# has to have dispatched, or this cell silently degrades to a CPU-only run while
# still reporting the same text.
if(_out MATCHES "device-mismatch-progs")
    if(_out MATCHES "dispatches=0 ")
        message(FATAL_ERROR "slice/cref-oracle: a device was found and never "
                            "dispatched")
    endif()
endif()

# Known-positive. Without it the cell cannot tell "the reference agrees with C"
# from "nothing was compared". The mutation lands on the emitted C, so a driver
# that ignored the oracle's stdout would still be caught.
execute_process(COMMAND "${PY}" "${TOOL}"
                        --corpus "${CORPUS}" --mcc "${MCC}"
                        --slicerun "${RUNNER}"
                        --oracle-cc "${ORACLE_CC}" --suite-cc "${SUITE_CC}"
                        --work "${WORK}-mut" --quiet --recursive
                        --mutate --expect-cref-mismatch
                        "--cflags=${CFLAGS}"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(NOT _mut EQUAL 0)
    message(FATAL_ERROR
            "slice/cref-oracle: the mutated slice bodies still agreed with the "
            "CPU reference under every oracle, so this differential is blind "
            "and its clean result above means nothing:\n${_mout}")
endif()

message("slice/cref-oracle: clean OK, mutation detected")
