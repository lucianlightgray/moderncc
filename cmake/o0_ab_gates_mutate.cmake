set(_env "C2_NO_EXTRA=1" "O0_AB_CHECK=1" "O0_AB_GATES=1" "O0_AB_MIN_KEYS=${MINKEYS}")

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_env}
                        sh "${SCRIPT}" "${BUILD}" measurable "${OUTDIR}/clean"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
string(REGEX MATCH "o0_ab: O0_AB_GATES[^\n]*" _csum "${_out}")
message("${_csum}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "ast/o0-baseline-gated-known-positive: the unmutated "
                        "check is already failing, so this cell cannot say "
                        "anything about whether forcing the level knobs on is "
                        "measured at all. Run tools/o0_ab.sh yourself:\n${_out}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_env} "O0_AB_NOGATES=1"
                        sh "${SCRIPT}" "${BUILD}" measurable "${OUTDIR}/mutated"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
string(REGEX MATCH "[^\n]*gated counters are identical to the ungated bank[^\n]*"
       _msum "${_mout}")
message("${_msum}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "ast/o0-baseline-gated-known-positive: the level knobs "
                        "were derived and then not passed, so measurement B "
                        "was the ungated one under a gated name, and the "
                        "gated bank still agreed with it. That bank is a "
                        "second copy of tests/ast/o0-baseline/<key>.rir.txt "
                        "and holds nothing of its own -- which is exactly what "
                        "the thirteen files this cell replaced were")
endif()
message("ast/o0-baseline-gated-known-positive: clean OK, dropped knobs detected")
