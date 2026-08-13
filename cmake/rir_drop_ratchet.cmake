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
#
# TWO COMPILES, AND THEY ASK DIFFERENT QUESTIONS. The ratchet reads a default
# build: what drops in the configuration the compiler actually ships. The
# liveness probe reads the same source with -freg-disp: the strategy that
# PRODUCES the allowlisted opcode. They have to be separate because on arm64
# the default answer is legitimately an empty histogram -- reg-disp is
# MCC_OPTD_SPECIAL, so it is off unless asked, and the only producer of
# VT_REGDISP there is the ast_regdisp_env-gated site in mccgen.c. x86_64 has a
# second, ungated producer in its struct-pair path (x86_64-gen.c), which is why
# a default build drops regaddi there and not here. Ratcheting the flagged arm
# instead would have been the tempting fix and is the wrong one: it changes what
# the banked numbers mean on the host that banked them.

if(NOT CPU)
    message(FATAL_ERROR "rir/drop-ratchet: CPU is required. Without it the -I list "
                        "would name a backend directory the target does not have, "
                        "src/mcc.h's own #include of <cpu>-gen.h would not resolve, "
                        "and the compile would fail into an empty histogram.")
endif()

set(ALLOWED_regaddi 4)
set(_allowed regaddi)

function(rir_drop_report _flags _tag _outvar)
    set(_o "${BINDIR}/rir-drop-ratchet${_tag}.txt")
    file(REMOVE "${_o}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "MCC_RIR_PROD=2" "MCC_RIR_PROD_OUT=${_o}"
                "${MCC}" ${_flags} -c "${SRCDIR}/src/mcc.c"
                -o "${BINDIR}/rir-drop-ratchet${_tag}.o" -O1
                -I${SRCDIR}/include -I${SRCDIR}/src -I${SRCDIR}/src/arch
                -I${SRCDIR}/src/arch/${CPU}
                -I${SRCDIR}/src/objfmt -I${SRCDIR}/src/formats
                -I${SRCDIR}/src/algorithms
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
    # The compile's exit status, not just the report's existence.
    # MCC_RIR_PROD_OUT is written even when the compile fails, so a broken -I
    # list produces a real file holding an empty histogram -- the same shape as
    # "nothing dropped". That is how this cell spent its whole life red on
    # arm64 while reading as a finding about regaddi.
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "rir/drop-ratchet: mcc failed to compile src/mcc.c "
                            "with '${_flags}' (rc=${_rc}), so any histogram read "
                            "from it describes a compile that did not "
                            "happen:\n${_err}")
    endif()
    if(NOT EXISTS "${_o}")
        message(FATAL_ERROR "rir/drop-ratchet: the RIR report for '${_flags}' "
                            "produced nothing, so this cell would assert an empty "
                            "histogram and pass without measuring anything.")
    endif()
    file(STRINGS "${_o}" _l REGEX "^\\[rir-drop-op\\]")
    set(${_outvar} "${_l}" PARENT_SCOPE)
endfunction()

rir_drop_report("" "" _lines)
rir_drop_report("-freg-disp" "-regdisp" _live)

# Anti-vacuity, and it is not optional: the first version of this cell read the
# wrong report file, found zero drop lines, and passed every deliberately-broken
# bank thrown at it. An empty histogram is indistinguishable from a histogram
# that was never produced, so require every allowlisted opcode to actually
# appear -- in the arm where the strategy that produces it is ON, which is the
# only arm where its absence is unambiguously a finding.
foreach(_op IN LISTS _allowed)
    set(_seen 0)
    foreach(_l IN LISTS _live)
        if(_l MATCHES "^\\[rir-drop-op\\] ${_op}=")
            set(_seen 1)
        endif()
    endforeach()
    if(NOT _seen)
        message(FATAL_ERROR "rir/drop-ratchet: '${_op}' is banked as dropping but "
                            "does not appear in the -freg-disp report at all, so "
                            "the histogram this cell ratchets is not being "
                            "produced. Either the measurement did not run, or it "
                            "genuinely stopped dropping and the allowlist should "
                            "shrink.")
    endif()
endforeach()

# An opcode outside the allowlist is a finding on either arm: the ratchet arm
# because the shipping configuration started dropping it, the flagged arm
# because a strategy can reach a drop nobody has a handler for.
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
foreach(_l IN LISTS _live)
    if(_l MATCHES "^\\[rir-drop-op\\] ([A-Za-z0-9_]+)=([0-9]+)")
        set(_op "${CMAKE_MATCH_1}")
        list(FIND _allowed "${_op}" _idx)
        if(_idx EQUAL -1)
            list(APPEND _bad "${_op} under -freg-disp (not in the allowlist -- a "
                             "strategy reaches a drop with no handler in src/mccrir.c)")
        endif()
    endif()
endforeach()

if(_bad)
    message(FATAL_ERROR "rir/drop-ratchet: silently dropped opcodes regressed: ${_bad}")
endif()

list(LENGTH _lines _nk)
list(LENGTH _live _nl)
message("rir/drop-ratchet: OK (${_nk} opcode kind(s) dropped in a default build, "
        "${_nl} under -freg-disp, all within the bank)")
