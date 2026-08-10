execute_process(COMMAND "${PY}" "${TOOL}"
                        --mcc "${MCC}" --corpus "${CORPUS}" --known "${KNOWN}"
                        --work "${WORK}-clean" --levels 0,1 --jobs "${JOBS}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    message(FATAL_ERROR "optlevel/torture-differential-known-positive: the clean arm "
                        "skipped, so there was no corpus to perturb; this twin is "
                        "registered only when the real cell is")
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "optlevel/torture-differential-known-positive: the unmutated "
                        "-O0 vs -O1 differential is already failing, so this cell "
                        "cannot say anything about whether it can fail")
endif()
if(NOT _out MATCHES "subjects=([1-9][0-9]*)")
    message(FATAL_ERROR "optlevel/torture-differential-known-positive: the clean arm "
                        "reported no subjects, so it compared nothing")
endif()

execute_process(COMMAND "${PY}" "${TOOL}"
                        --mcc "${MCC}" --corpus "${CORPUS}" --known "${KNOWN}"
                        --work "${WORK}-mutated" --levels 0,1 --jobs "${JOBS}"
                        --mutate "${SUBJECT}"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "optlevel/torture-differential-known-positive: the recorded -O0 "
                        "answer for ${SUBJECT} was perturbed and the differential still "
                        "reported clean, so it is not comparing anything")
endif()
if(_mut EQUAL 77)
    message(FATAL_ERROR "optlevel/torture-differential-known-positive: the mutated arm "
                        "skipped where the clean arm did not")
endif()
if(NOT _mout MATCHES "NEW   ${SUBJECT}")
    message(FATAL_ERROR "optlevel/torture-differential-known-positive: the mutated arm "
                        "failed, but not by naming ${SUBJECT} as an unknown divergence; "
                        "it failed for some other reason and proves nothing")
endif()
message("optlevel/torture-differential-known-positive: clean OK, perturbed -O0 answer for "
        "${SUBJECT} reported as an unknown divergence")
