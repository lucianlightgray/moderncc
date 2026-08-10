execute_process(COMMAND "${PY}" "${TOOL}" "${MCC}" "${SRC}" "--opt=${OPT}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    message("opt-search-determinism-known-positive: SKIP: the clean arm "
            "reported 77, so this host has no runnable mcc, no subject, or no "
            "os.fork to build the loaded arm with")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "opt-search-determinism-known-positive: the unmutated "
                        "run is already failing, so this cell cannot say "
                        "anything about whether the byte comparison can detect "
                        "a search whose coverage depends on the clock")
endif()

execute_process(COMMAND "${PY}" "${TOOL}" "${MCC}" "${SRC}" "--opt=${OPT}"
                        --mutate
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "opt-search-determinism-known-positive: the loaded "
                        "compile was run with -fopt-search-ticks=0 and the "
                        "idle one at ${OPT}'s shipped tick count, so one "
                        "searched and the other did not and the objects "
                        "genuinely differ -- and the tool still reported them "
                        "byte-identical. A reproducibility gate that passes "
                        "over objects that differ is comparing a file against "
                        "itself, and its OK line is the strongest-looking "
                        "vacuous pass in the tree")
endif()
message("opt-search-determinism-known-positive: clean OK, perturbed run detected")
