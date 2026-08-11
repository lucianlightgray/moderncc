execute_process(COMMAND "${SMOKERUN}" --mcc "${MCC}" --srcdir "${SRCDIR}"
                        --work "${WORK}-clean"
                        --engines --min-engines 8 --min-cases 9000000
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "smoke/engines-identity: the unmutated engine arm is "
                        "already failing, so this cell cannot say anything "
                        "about whether the arm can tell its engines apart")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env MCC_RIR_FORCE=1
                        "${SMOKERUN}" --mcc "${MCC}" --srcdir "${SRCDIR}"
                        --work "${WORK}-rir"
                        --engines --min-engines 5
                RESULT_VARIABLE _rir OUTPUT_VARIABLE _rout ERROR_VARIABLE _rout)
message("${_rout}")
if(_rir EQUAL 0)
    message(FATAL_ERROR "smoke/engines-identity: MCC_RIR_FORCE=1 puts the RIR "
                        "replay evaluator underneath the ast arm, which exists "
                        "precisely to run without it, and the arm reported OK. "
                        "An arm that cannot notice this reports six engines "
                        "agreeing when it has really run one engine six times "
                        "-- which is the state smoke was in until this cell, "
                        "because smokerun set MCC_FORCE_REPLAY=1 on every "
                        "compile it made")
endif()
if(NOT _rout MATCHES "recorded [0-9]+ replayed evaluations")
    message(FATAL_ERROR "smoke/engines-identity: the MCC_RIR_FORCE=1 run did "
                        "fail, but not with the RIR-census diagnostic, so the "
                        "failure cannot be attributed to the ast arm losing "
                        "its identity")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env MCC_JIT_LAZY=1
                        "${SMOKERUN}" --mcc "${MCC}" --srcdir "${SRCDIR}"
                        --work "${WORK}-jit"
                        --engines --min-engines 4
                RESULT_VARIABLE _jit OUTPUT_VARIABLE _jout ERROR_VARIABLE _jout)
message("${_jout}")
if(_jit EQUAL 0)
    message(FATAL_ERROR "smoke/engines-identity: MCC_JIT_LAZY=1 stops the "
                        "embedded engine swapping any function at boot, so the "
                        "jit arm ran the AOT code it was supposed to be "
                        "replacing, and the arm reported OK. A jit arm that "
                        "does not check a swap happened agrees with the AOT "
                        "baseline because it IS the AOT baseline")
endif()
if(NOT _jout MATCHES "without swapping a single function")
    message(FATAL_ERROR "smoke/engines-identity: the MCC_JIT_LAZY=1 run did "
                        "fail, but not with the swap diagnostic, so the failure "
                        "cannot be attributed to the jit arm losing its "
                        "identity")
endif()

execute_process(COMMAND "${SMOKERUN}" --mcc "${MCC}" --srcdir "${SRCDIR}"
                        --work "${WORK}-drop"
                        --engines --min-engines 8 --engines-drop rir-o4
                RESULT_VARIABLE _drop OUTPUT_VARIABLE _dout ERROR_VARIABLE _dout)
message("${_dout}")
if(_drop EQUAL 0)
    message(FATAL_ERROR "smoke/engines-identity: one non-device engine (rir-o4) "
                        "was dropped and the arm still reported OK. This is the "
                        "defect the floor exists to catch, and it is the state "
                        "the tree was in while --min-engines was 5 against nine "
                        "registered engines: three non-device engines could stop "
                        "running and a present device kept the count above the "
                        "floor. The floor must count only engines that cannot "
                        "skip on their own")
endif()
if(NOT _dout MATCHES "--min-engines")
    message(FATAL_ERROR "smoke/engines-identity: the dropped-engine run did fail, "
                        "but not with the --min-engines diagnostic, so the "
                        "failure cannot be attributed to the floor noticing a "
                        "missing required engine")
endif()

message("smoke/engines-identity: the clean arm passed, a replay leak under the "
        "ast arm was refused, a jit arm that never swapped was refused, and a "
        "dropped non-device engine was caught by the required-engine floor")
