execute_process(COMMAND "${PY}" "${CENSUS}" --selfcheck
                        "--oracle=${ORACLE}" "--root=${SRCDIR}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "fmt/census-oracle-known-positive: the unmutated "
                        "selfcheck is already failing, so this cell cannot say "
                        "anything about whether it can detect drift")
endif()

execute_process(COMMAND "${PY}" "${CENSUS}" --selfcheck --mutate
                        "--oracle=${ORACLE}" "--root=${SRCDIR}"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
string(REGEX MATCH "fmt-census selfcheck: [^\n]*" _msum "${_mout}")
message("${_msum}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "fmt/census-oracle-known-positive: the port was put "
                        "back to appending one item per literal byte -- the "
                        "exact drift that made the board read 140/162 when the "
                        "compiler accepted 142 -- and the selfcheck still "
                        "agreed with mcc_fmt_compile, so it is comparing "
                        "nothing")
endif()
message("fmt/census-oracle-known-positive: clean OK, reintroduced drift detected")
