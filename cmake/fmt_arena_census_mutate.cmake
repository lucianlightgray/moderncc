execute_process(COMMAND "${PY}" "${CENSUS}" "--arena-check=${BINDIR}"
                        "--root=${SRCDIR}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _clean EQUAL 0)
    message("${_out}")
    message(FATAL_ERROR "fmt/arena-census-known-positive: the unmutated check "
                        "is already failing, so this cell cannot say anything "
                        "about whether the de-duplicated arena census is being "
                        "compared at all")
endif()

foreach(_m dup shrink share)
    execute_process(COMMAND "${PY}" "${CENSUS}" "--arena-check=${BINDIR}"
                            "--root=${SRCDIR}" "--mutate-arenas=${_m}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _mout
                    ERROR_VARIABLE _mout)
    if(_rc EQUAL 0)
        message("${_mout}")
        message(FATAL_ERROR "fmt/arena-census-known-positive: the '${_m}' "
                            "mutation moved the arena census and the bank "
                            "check still passed, so it is comparing nothing. "
                            "'dup' is the loop-over-src/*.c contamination the "
                            "8,250-arena figure was taken with, 'shrink' is a "
                            "dump that silently lost bodies, and 'share' is the "
                            "0.825% the board quotes")
    endif()
endforeach()
message("fmt/arena-census-known-positive: clean OK, dup/shrink/share all "
        "detected")
