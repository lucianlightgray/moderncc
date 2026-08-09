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

if(_out MATCHES "device-mismatch-progs")
    if(_out MATCHES "dispatches=0 ")
        message(FATAL_ERROR "slice/cref-oracle: a device was found and never "
                            "dispatched")
    endif()
endif()

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
