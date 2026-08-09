execute_process(COMMAND "${GATE}" "${SRCDIR}/src" "${SRCDIR}/tools"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
string(REGEX MATCH "idiom-gate subject:[^\n]*" _csum "${_out}")
message("${_csum}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "idiom-gate-known-positive: the real tree is already "
                        "failing the invariant, so this cell cannot say "
                        "anything about whether the gate can detect "
                        "anything:\n${_out}")
endif()

execute_process(COMMAND "${GATE}" "${SRCDIR}/tests/idiom/known-positive"
                RESULT_VARIABLE _bad OUTPUT_VARIABLE _bout ERROR_VARIABLE _bout)
message("${_bout}")
if(_bad EQUAL 0)
    message(FATAL_ERROR "idiom-gate-known-positive: tests/idiom/known-positive "
                        "tests a value-kind config macro with #ifdef, a "
                        "flag-kind one as a value, and another value-kind one "
                        "through defined() -- one of each violation the gate "
                        "names -- and the gate passed. It is not reading the "
                        "files it walks")
endif()

execute_process(COMMAND "${GATE}" "${SRCDIR}/tests/idiom/empty"
                RESULT_VARIABLE _empty OUTPUT_VARIABLE _eout ERROR_VARIABLE _eout)
message("${_eout}")
if(_empty EQUAL 0)
    message(FATAL_ERROR "idiom-gate-known-positive: the gate reported OK over "
                        "an empty directory. A run that scanned no file must "
                        "not be character-for-character identical to a run "
                        "that checked the whole tree")
endif()
message("idiom-gate-known-positive: clean OK, violations detected, empty walk refused")
