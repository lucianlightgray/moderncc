execute_process(COMMAND "${GATE}" "${SRCDIR}/src"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "trace-gate-known-positive: the real tree is already "
                        "failing the invariant, so this cell cannot say anything "
                        "about whether the gate can detect a violation:\n${_out}")
endif()

execute_process(COMMAND "${GATE}" "${SRCDIR}/tests/tracegate/known-positive"
                RESULT_VARIABLE _bad OUTPUT_VARIABLE _bout ERROR_VARIABLE _bout)
message("${_bout}")
if(_bad EQUAL 0)
    message(FATAL_ERROR "trace-gate-known-positive: the fixture is armed ONLY "
                        "through MCC_TRACE_WHEN (it carries no bare MCC_TRACE), "
                        "and holds one function that opens with no trace site and "
                        "one that opens with the wrong message. The gate passed, "
                        "so it either did not arm a _WHEN-only file -- the exact "
                        "hole this cell exists to keep shut -- or is not checking "
                        "the message:\n${_bout}")
endif()
if(NOT _bout MATCHES "does not open with MCC_TRACE")
    message(FATAL_ERROR "trace-gate-known-positive: the _WHEN-only fixture has a "
                        "function opening with no trace site and the gate did not "
                        "report it. A file armed only through MCC_TRACE_WHEN is "
                        "being skipped -- the file-arming gap has regressed:\n${_bout}")
endif()
if(NOT _bout MATCHES "opens with MCC_TRACE but not")
    message(FATAL_ERROR "trace-gate-known-positive: the fixture opens a function "
                        "with MCC_TRACE_WHEN(x, \"wrong\") and the gate did not "
                        "flag the message. The arg_is_n skip-args message check is "
                        "not firing, so a conditional trace site need not name "
                        "itself:\n${_bout}")
endif()
message("trace-gate-known-positive: clean OK on src, and a _WHEN-only fixture's "
        "missing-opener and wrong-message violations both detected -- a file "
        "instrumented solely with MCC_TRACE_WHEN is armed and its openers checked")
