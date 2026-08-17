if(WIN32)
    set(_libs -lm)
else()
    set(_libs -lm -lpthread)
endif()

set(_subjects
    structs_unions/inline_sret_locrec
    structs_unions/struct_byval
    optimizer/cse
    types/floating_point
    features_c99_c11/c11_complex_convert
    statements/chained_assign)

set(_work "${BINDIR}/rir-recmiss")
file(MAKE_DIRECTORY "${_work}")

set(_ran 0)
set(_moved 0)
set(_bad "")

foreach(_s IN LISTS _subjects)
    set(_src "${SRCDIR}/tests/exec/${_s}.c")
    if(NOT EXISTS "${_src}")
        list(APPEND _bad "${_s}: absent from tests/exec")
        continue()
    endif()
    get_filename_component(_nm "${_s}" NAME)

    execute_process(COMMAND "${MCC}" -B "${BINDIR}" -I "${SRCDIR}/runtime/include"
                            -w -O0 "${_src}" -o "${_work}/${_nm}.ref" ${_libs}
                    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    if(NOT _rc EQUAL 0)
        continue()
    endif()
    execute_process(COMMAND "${_work}/${_nm}.ref"
                    RESULT_VARIABLE _refrc OUTPUT_VARIABLE _refout ERROR_QUIET)

    execute_process(COMMAND "${CMAKE_COMMAND}" -E env MCC_RIR_REC_FORCE_MISS=1
                            "${MCC}" -B "${BINDIR}" -I "${SRCDIR}/runtime/include"
                            -w -O2 "${_src}" -o "${_work}/${_nm}.miss" ${_libs}
                    RESULT_VARIABLE _rc2 OUTPUT_QUIET ERROR_QUIET)
    if(NOT _rc2 EQUAL 0)
        list(APPEND _bad "${_s}: build failed with MCC_RIR_REC_FORCE_MISS=1")
        continue()
    endif()
    execute_process(COMMAND "${_work}/${_nm}.miss"
                    RESULT_VARIABLE _missrc OUTPUT_VARIABLE _missout ERROR_QUIET)

    if(NOT _refrc STREQUAL _missrc OR NOT _refout STREQUAL _missout)
        list(APPEND _bad "${_s}: differs from -O0 when every record take is forced to miss")
    endif()
    math(EXPR _ran "${_ran} + 1")

    execute_process(COMMAND "${MCC}" -B "${BINDIR}" -I "${SRCDIR}/runtime/include"
                            -w -O2 -c "${_src}" -o "${_work}/${_nm}.plain.o"
                    RESULT_VARIABLE _rc3 OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env MCC_RIR_REC_FORCE_MISS=1
                            "${MCC}" -B "${BINDIR}" -I "${SRCDIR}/runtime/include"
                            -w -O2 -c "${_src}" -o "${_work}/${_nm}.miss.o"
                    RESULT_VARIABLE _rc4 OUTPUT_QUIET ERROR_QUIET)
    if(_rc3 EQUAL 0 AND _rc4 EQUAL 0)
        execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                                "${_work}/${_nm}.plain.o" "${_work}/${_nm}.miss.o"
                        RESULT_VARIABLE _same OUTPUT_QUIET ERROR_QUIET)
        if(NOT _same EQUAL 0)
            math(EXPR _moved "${_moved} + 1")
        endif()
    endif()
endforeach()

if(_bad)
    foreach(_m IN LISTS _bad)
        message("FAIL rir/rec-miss: ${_m}")
    endforeach()
    message(FATAL_ERROR "rir/rec-miss: ${_ran} subject(s) checked, failures above")
endif()

if(_ran LESS 4)
    message(FATAL_ERROR
            "rir/rec-miss: only ${_ran} subject(s) ran; this cell exists to exercise the "
            "take-by-fit failure arm and proves nothing on an empty corpus")
endif()

if(_moved EQUAL 0)
    message(FATAL_ERROR
            "rir/rec-miss: MCC_RIR_REC_FORCE_MISS changed no object across ${_ran} "
            "subject(s), so the injection is inert and this cell is vacuous. The hook "
            "must actually divert allocation to the frontier fallback for the arm to "
            "be under test")
endif()

message("PASS rir/rec-miss: ${_ran} subject(s) agree with -O0 while every record take "
        "is forced to miss; the injection moved ${_moved} object(s), so the frontier "
        "fallback is the code under test and not a no-op")
