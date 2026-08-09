execute_process(COMMAND "${PY}" "${BENCH}" --check
                        "--bank=${SRCDIR}/tests/optfire/levelbench.tsv"
                        "--mccopt=${SRCDIR}/src/mccopt.h"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "optbench/levelbench-bank-known-positive: the unmutated "
                        "check is already failing, so this cell cannot say "
                        "anything about whether the banked ladder table is "
                        "being compared to src/mccopt.h at all")
endif()

execute_process(COMMAND "${PY}" "${BENCH}" --check --mutate
                        "--bank=${SRCDIR}/tests/optfire/levelbench.tsv"
                        "--mccopt=${SRCDIR}/src/mccopt.h"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "optbench/levelbench-bank-known-positive: the table was "
                        "put back to a generation stale -- a row naming a flag "
                        "no longer at levels 1-3, a row whose level drifted off "
                        "what src/mccopt.h ships, and a shipped rung with no "
                        "row at all, which is the shape 32 of the old 47 rows "
                        "had and the shape that made narrow/tree-copy-prop's "
                        "stale rows get read as unmeasured -- and --check still "
                        "passed, so it is comparing nothing")
endif()
message("optbench/levelbench-bank-known-positive: clean OK, reintroduced generation stale detected")
