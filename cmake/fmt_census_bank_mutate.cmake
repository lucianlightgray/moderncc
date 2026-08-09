execute_process(COMMAND "${PY}" "${CENSUS}" --check "--root=${SRCDIR}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "fmt/census-bank-known-positive: the unmutated check is "
                        "already failing, so this cell cannot say anything "
                        "about whether the banked site census is being "
                        "compared at all")
endif()

execute_process(COMMAND "${PY}" "${CENSUS}" --check --mutate "--root=${SRCDIR}"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "fmt/census-bank-known-positive: the port was put back "
                        "to appending one item per literal byte, which moves "
                        "'accepted' off 148 and 'module budget' off 9 -- both "
                        "quoted on the board -- and the bank check still "
                        "passed, so it is comparing nothing")
endif()
message("fmt/census-bank-known-positive: clean OK, moved board figures detected")
