execute_process(COMMAND sh "${SCRIPT}" "${MCC}" "${SRCDIR}" --min-files ${MINFILES}
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _clean EQUAL 0)
    message("${_out}")
    message(FATAL_ERROR "osx/headers-parse-known-positive: the unmutated check "
                        "is already failing, so this cell cannot say anything "
                        "about whether the header set is being enforced at all")
endif()

set(_hdr "${SRCDIR}/runtime/osx/include/math.h")
set(_hidden "${SRCDIR}/runtime/osx/include/math.h.kp-hidden")

if(NOT EXISTS "${_hdr}")
    message(FATAL_ERROR "osx/headers-parse-known-positive: ${_hdr} is absent, so "
                        "the mutation has nothing to remove")
endif()

file(RENAME "${_hdr}" "${_hidden}")
execute_process(COMMAND sh "${SCRIPT}" "${MCC}" "${SRCDIR}" --min-files ${MINFILES}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
file(RENAME "${_hidden}" "${_hdr}")

if(_rc EQUAL 0)
    message("${_mout}")
    message(FATAL_ERROR "osx/headers-parse-known-positive: math.h was removed "
                        "from runtime/osx/include and the check still passed -- "
                        "a header set that reports coverage after losing a "
                        "header it is supposed to ship is measuring nothing")
endif()

execute_process(COMMAND sh "${SCRIPT}" "${MCC}" "${SRCDIR}" --min-files 100000
                RESULT_VARIABLE _frc OUTPUT_VARIABLE _fout ERROR_VARIABLE _fout)
if(_frc EQUAL 0)
    message("${_fout}")
    message(FATAL_ERROR "osx/headers-parse-known-positive: the --min-files floor "
                        "was set above every corpus file and the check still "
                        "passed, so a shrinking subject would read as coverage")
endif()

message("osx/headers-parse-known-positive: clean OK, a missing header and an "
        "unmet floor both detected")
