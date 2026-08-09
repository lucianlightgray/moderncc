execute_process(COMMAND "${PY}" "${TOOL}" "${BDIR}" "${LEVELS}" self
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    message("untyped-probe-known-positive: SKIP: the clean arm reported 77, so "
            "this host has no compile_commands.json or no mcc to census")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "untyped-probe-known-positive: the unmutated probe is "
                        "already failing, so this cell cannot say anything "
                        "about whether it notices its subject going missing")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PROBE_ENV=MCC_RIR_PROD=0"
                        "${PY}" "${TOOL}" "${BDIR}" "${LEVELS}" self
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "untyped-probe-known-positive: MCC_RIR_PROD=0 stops the "
                        "compiler emitting the [rir-untyped] record the probe "
                        "reads, so nodes=0 and every share has an empty "
                        "denominator -- and the probe still exited 0. This "
                        "tool's whole subject is a denominator that has to "
                        "reach zero, so a run that censused nothing renders "
                        "identically to the goal being met")
endif()
message("untyped-probe-known-positive: clean OK, vanished subject detected")
