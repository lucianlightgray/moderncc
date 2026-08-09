set(_env "C2_NO_EXTRA=1" "O0_AB_CHECK=1" "O0_AB_MIN_KEYS=${MINKEYS}")

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_env}
                        sh "${SCRIPT}" "${BUILD}" measurable "${OUTDIR}/clean"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
string(REGEX MATCH "o0_ab: measurable[^\n]*" _csum "${_out}")
message("${_csum}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "ast/o0-baseline-known-positive: the unmutated check is "
                        "already failing, so this cell cannot say anything "
                        "about whether the -O0 object bank is being compared "
                        "at all. Run tools/o0_ab.sh yourself:\n${_out}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_env} "O0_AB_MUTATE=1"
                        sh "${SCRIPT}" "${BUILD}" measurable "${OUTDIR}/mutated"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
string(REGEX MATCH "o0_ab: [^\n]*an -O0 object moved[^\n]*" _msum "${_mout}")
message("${_msum}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "ast/o0-baseline-known-positive: measurement A was "
                        "taken at -O1 instead of -O0, so every banked object "
                        "sha256 in tests/ast/o0-baseline/ had to move, and the "
                        "check still passed. It is diffing an empty corpus "
                        "against an empty bank, or not diffing at all")
endif()
message("ast/o0-baseline-known-positive: clean OK, -O1 measurement detected")
