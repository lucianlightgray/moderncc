execute_process(COMMAND "${PY}" "${TOOL}"
                        "--build=${BINDIR}"
                        "--manifest=${MANIFEST}"
                        "--must-run=${MUSTRUN}"
                        "--min-rows=${MINROWS}"
                        "--min-proved=${MINPROVED}"
                        "--max-unfloored=${MAXUNFLOORED}"
                        "--max-unproved=${MAXUNPROVED}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _clean EQUAL 0)
    message("${_out}")
    message(FATAL_ERROR "ci/gate-contract-known-positive: the unmutated check "
                        "is already failing, so this cell cannot say anything "
                        "about whether the gate contract is being enforced at "
                        "all")
endif()

set(_why_drop-row "a gate that dropped out of the manifest is a gate nobody declares a floor or a prover for")
set(_why_relax-floor "a floor the build stopped passing leaves the subject unbounded below")
set(_why_forge-prover "a prover that is not registered proves nothing, and naming one must not be enough")
set(_why_unlisted-prover "a prover absent from tests/must-run.txt can stop being registered with nothing noticing, so naming one must not count as proof")
set(_why_empty "an empty manifest asserts nothing, and must not read as every gate being covered")

foreach(_m drop-row relax-floor forge-prover unlisted-prover empty)
    execute_process(COMMAND "${PY}" "${TOOL}"
                            "--build=${BINDIR}"
                            "--manifest=${MANIFEST}"
                            "--must-run=${MUSTRUN}"
                            "--min-rows=${MINROWS}"
                            "--min-proved=${MINPROVED}"
                            "--max-unfloored=${MAXUNFLOORED}"
                            "--max-unproved=${MAXUNPROVED}"
                            "--mutate=${_m}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
    if(_rc EQUAL 0)
        message("${_mout}")
        message(FATAL_ERROR "ci/gate-contract-known-positive: the '${_m}' "
                            "mutation was applied and the contract still "
                            "passed -- ${_why_${_m}}")
    endif()
endforeach()
message("ci/gate-contract-known-positive: clean OK, drop-row/relax-floor/"
        "forge-prover/unlisted-prover/empty all detected")
