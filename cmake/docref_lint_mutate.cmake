execute_process(COMMAND "${PY}" "${LINT}" "--root=${SRCDIR}" --min-refs 440
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "docs/refs-known-positive: the unmutated lint is "
                        "already failing, so this cell cannot say anything "
                        "about whether it resolves citations at all")
endif()

execute_process(COMMAND "${PY}" "${LINT}" "--root=${SRCDIR}" --min-refs 440
                        --mutate
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "docs/refs-known-positive: one defect of every shape "
                        "this lint claims to catch was planted in memory -- a "
                        "path citation naming a file that is not in the tree, "
                        "a file:line anchor past the end of the file it names, "
                        "a project-namespaced symbol quoted beside a file:line "
                        "it does not occur at (the docs/PLAN.md:435 defect "
                        "verbatim, from the plan since retired into "
                        "docs/ARCHIVED.md), and a failed-to-reproduce table whose row "
                        "count disagrees with the one sentence that states it, "
                        "and a DETAILS.md#anchor citation resolving to no such "
                        "anchor, and a merge-conflict marker left in a live doc -- and the "
                        "lint did not report all six. A lint that "
                        "walks 800 citations and resolves none of them prints "
                        "the same OK line as one that resolves all of them")
endif()

execute_process(COMMAND "${PY}" "${LINT}" "--root=${SRCDIR}" --min-refs 100000
                RESULT_VARIABLE _floor OUTPUT_VARIABLE _fout ERROR_VARIABLE _fout)
message("${_fout}")
if(_floor EQUAL 0)
    message(FATAL_ERROR "docs/refs-known-positive: the lint reported OK having "
                        "been told to expect 100000 citations. The subject "
                        "floor is what stops a docs tree that stopped being "
                        "read from rendering identically to a docs tree with "
                        "nothing wrong in it")
endif()

execute_process(COMMAND "${PY}" "${LINT}" "--root=${SRCDIR}/tools"
                RESULT_VARIABLE _nodocs OUTPUT_VARIABLE _nout ERROR_VARIABLE _nout)
message("${_nout}")
if(_nodocs EQUAL 0)
    message(FATAL_ERROR "docs/refs-known-positive: the lint reported OK over a "
                        "root with no docs/ in it. A run that read no document "
                        "must not be reportable as a pass")
endif()

message("docs/refs-known-positive: clean OK, all six planted citation shapes "
        "detected, subject floor fired, missing docs tree refused")
