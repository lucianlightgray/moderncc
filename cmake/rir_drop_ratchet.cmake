# Phase -1's other half. src/mccrir.c has a case for most IR opcodes and a bare
# `default:` for the rest, which used to skip them with no counter and no
# diagnostic. rir_drop_note() now records them per opcode (excluding the five
# CFG opcodes that are correctly handled in other switches), and this cell is
# what stops that histogram from being decoration: an opcode that starts
# dropping, or one that drops more often, fails here instead of quietly
# degrading replay fidelity.
#
# The allowlist is the measured state, not an aspiration. Shrink it by writing a
# handler; never grow it to make this green.

set(ALLOWED_regaddi 4)
set(_allowed regaddi)

set(_out "${BINDIR}/rir-drop-ratchet.txt")
file(REMOVE "${_out}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "MCC_RIR_PROD=2" "MCC_RIR_PROD_OUT=${_out}"
            "${MCC}" -c "${SRCDIR}/src/mcc.c" -o "${BINDIR}/rir-drop-ratchet.o" -O1
            -I${SRCDIR}/include -I${SRCDIR}/src -I${SRCDIR}/src/arch
            -I${SRCDIR}/src/arch/x86_64 -I${SRCDIR}/src/arch/i386
            -I${SRCDIR}/src/objfmt -I${SRCDIR}/src/formats
            -I${SRCDIR}/src/algorithms
    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)

if(NOT EXISTS "${_out}")
    message(FATAL_ERROR "rir/drop-ratchet: the RIR report produced nothing, so "
                        "this cell would assert an empty histogram and pass "
                        "without measuring anything. mcc rc=${_rc} ${_err}")
endif()

file(STRINGS "${_out}" _lines REGEX "^\\[rir-drop-op\\]")

# Anti-vacuity, and it is not optional: the first version of this cell read the
# wrong report file, found zero drop lines, and passed every deliberately-broken
# bank thrown at it. An empty histogram is indistinguishable from a histogram
# that was never produced, so require every allowlisted opcode to actually
# appear. If regaddi stops dropping, that is good news and this list shrinks --
# by editing it deliberately, not by the cell silently going quiet.
foreach(_op IN LISTS _allowed)
    set(_seen 0)
    foreach(_l IN LISTS _lines)
        if(_l MATCHES "^\\[rir-drop-op\\] ${_op}=")
            set(_seen 1)
        endif()
    endforeach()
    if(NOT _seen)
        message(FATAL_ERROR "rir/drop-ratchet: '${_op}' is banked as dropping but "
                            "does not appear in the report at all. Either the "
                            "measurement did not run, or it genuinely stopped "
                            "dropping and the allowlist should shrink.")
    endif()
endforeach()

set(_bad "")
foreach(_l IN LISTS _lines)
    if(_l MATCHES "^\\[rir-drop-op\\] ([A-Za-z0-9_]+)=([0-9]+)")
        set(_op "${CMAKE_MATCH_1}")
        set(_n "${CMAKE_MATCH_2}")
        list(FIND _allowed "${_op}" _idx)
        if(_idx EQUAL -1)
            list(APPEND _bad "${_op}=${_n} (not in the allowlist -- it needs a "
                             "handler in src/mccrir.c, not an allowlist entry)")
        elseif(_n GREATER "${ALLOWED_${_op}}")
            list(APPEND _bad "${_op}=${_n} > banked ${ALLOWED_${_op}}")
        endif()
    endif()
endforeach()

if(_bad)
    message(FATAL_ERROR "rir/drop-ratchet: silently dropped opcodes regressed: ${_bad}")
endif()

list(LENGTH _lines _nk)
message("rir/drop-ratchet: OK (${_nk} opcode kind(s) dropped, all within the bank)")
