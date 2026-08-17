execute_process(COMMAND "${PY}" "${TOOL}" "${MCC}" "${SRC}" --runs "${RUNS}"
                        --from-build "${BDIR}" -- "${OPT}" -c
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    message("opt-determinism-known-positive: SKIP: the clean arm reported 77, "
            "so this host has no runnable mcc or no compile_commands.json")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "opt-determinism-known-positive: the unmutated run is "
                        "already failing, so this cell cannot say anything "
                        "about whether the byte comparison can detect a "
                        "compiler whose output depends on the run")
endif()

execute_process(COMMAND "${PY}" "${TOOL}" "${MCC}" "${SRC}" --runs "${RUNS}"
                        --mutate --from-build "${BDIR}" -- "${OPT}" -c
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 77)
    message("opt-determinism-known-positive: SKIP: the mutated arm reported 77, "
            "so the mutation never ran and this cannot count as a detection")
    cmake_language(EXIT 77)
endif()
if(_mut EQUAL 0)
    message(FATAL_ERROR "opt-determinism-known-positive: run 0 was compiled at "
                        "-O0 and the rest at ${OPT}, so the objects genuinely "
                        "differ, and the tool still reported every run "
                        "byte-identical. A determinism gate that passes over "
                        "objects that differ is comparing a file against "
                        "itself, and its OK line is the strongest-looking "
                        "vacuous pass in the tree")
endif()
message("opt-determinism-known-positive: clean OK, perturbed run detected")
