execute_process(COMMAND "${PY}" "${BENCH}" --selfcheck
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "optbench/null-subject-known-positive: the unmutated "
                        "selfcheck is already failing, so this cell cannot say "
                        "anything about whether it can detect a null "
                        "experiment being reported as a number")
endif()

execute_process(COMMAND "${PY}" "${BENCH}" --selfcheck --mutate
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "optbench/null-subject-known-positive: the tool was put "
                        "back to reporting a geometric mean over bit-identical "
                        "kernel binaries as a gain -- the exact defect that let "
                        "the ladder write-up say storeval-rot 'changes 0.0000% "
                        "of emitted instructions' for a flag costing 2.31% of "
                        "stage-1 -- and the selfcheck still passed, so it is "
                        "checking nothing")
endif()
message("optbench/null-subject-known-positive: clean OK, reintroduced null experiment detected")
