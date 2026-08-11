execute_process(COMMAND "${SMOKERUN}" --mcc "${MCC}" --srcdir "${SRCDIR}"
                        --work "${WORK}-clean"
                        --min-cases 12800000 --min-passes 150 --min-strats 22
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "smoke/strats-known-positive: the unmutated run is "
                        "already failing, so this cell cannot say anything "
                        "about whether the --min-strats floor measures the "
                        "strategy table at all")
endif()

execute_process(COMMAND "${SMOKERUN}" --mcc "${MCC}" --srcdir "${SRCDIR}"
                        --work "${WORK}-high"
                        --min-cases 12800000 --min-passes 150 --min-strats 23
                RESULT_VARIABLE _high OUTPUT_VARIABLE _hout ERROR_VARIABLE _hout)
message("${_hout}")
if(_high EQUAL 0)
    message(FATAL_ERROR "smoke/strats-known-positive: the run was told to "
                        "expect 23 strategies over a 22-row table and reported "
                        "OK. A floor that does not read the table it floors "
                        "renders a subject reaching 8 strategies identically "
                        "to one reaching all 22 -- which is the state this "
                        "cell exists to refuse, and the state the suite was "
                        "actually in until 2026-08-10")
endif()

if(NOT _hout MATCHES "below the --min-strats")
    message(FATAL_ERROR "smoke/strats-known-positive: the over-high floor did "
                        "fail the run, but not with the --min-strats "
                        "diagnostic, so the failure cannot be attributed to "
                        "the strategy floor")
endif()

message("smoke/strats-known-positive: 22 of 22 accepted, 23 refused, and the "
        "refusal named the strategy floor")
