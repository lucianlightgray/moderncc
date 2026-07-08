# AST replay differential-exec driver (docs/AST.md §17).
# Compiles SRC twice — once through the -O0 parser->emit path, once with the AST
# replay driver on (MCC_AST_REPLAY=1, which builds the intention tree and re-emits
# from it) — links, runs both, and asserts the same exit code. When REPLAYED is
# set, it also asserts the named function actually went through the replay path
# (the dump fired) rather than silently falling back, so the test proves the AST
# path is exercised, not bypassed.
#   cmake -DMCC=<mcc> -DSRC=<file> -DOUT=<dir> -DEXPECT_RC=<n> [-DREPLAYED=main] -P replay.cmake
if(NOT MCC OR NOT SRC OR NOT OUT)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DEXPECT_RC= [-DREPLAYED=] -P replay.cmake")
endif()

# mcc won't create the output file's parent dir; on a fresh build tree OUT
# does not exist yet, so ensure it before the first compile writes into it.
file(MAKE_DIRECTORY "${OUT}")

get_filename_component(_name "${SRC}" NAME_WE)
set(_o0 "${OUT}/${_name}.O0")
set(_r1 "${OUT}/${_name}.R1")

function(build_and_run out_exe expect_rc env_on out_all)
    if(env_on)
        set(_env "${CMAKE_COMMAND}" -E env MCC_AST_REPLAY=1 MCC_AST_REPLAY_DUMP=1)
        if(TEMPLATES)
            list(APPEND _env MCC_AST_TEMPLATES=1)
        endif()
        if(PROMOTE)
            list(APPEND _env MCC_AST_PROMOTE=1)
        endif()
    else()
        set(_env "${CMAKE_COMMAND}" -E env)
    endif()
    execute_process(
        COMMAND ${_env} "${MCC}" "-B${BDIR}" "${SRC}" -o "${out_exe}"
        OUTPUT_VARIABLE _co ERROR_VARIABLE _ce RESULT_VARIABLE _crc)
    if(NOT _crc EQUAL 0)
        message(FATAL_ERROR "compile failed (replay=${env_on}) for ${SRC} (rc=${_crc}):\n${_co}${_ce}")
    endif()
    execute_process(COMMAND "${out_exe}" RESULT_VARIABLE _rrc)
    if(NOT _rrc EQUAL expect_rc)
        message(FATAL_ERROR "run (replay=${env_on}) exit ${_rrc}, expected ${expect_rc} for ${SRC}")
    endif()
    set(${out_all} "${_co}${_ce}" PARENT_SCOPE)
endfunction()

build_and_run("${_o0}" "${EXPECT_RC}" OFF _o0_all)
build_and_run("${_r1}" "${EXPECT_RC}" ON _r1_all)

if(REPLAYED)
    if(NOT _r1_all MATCHES "\\[ast-replay\\] ${REPLAYED}\n")
        message(FATAL_ERROR "expected function '${REPLAYED}' to go through the AST replay path, but it fell back:\n${_r1_all}")
    endif()
endif()

# PROMOTES: with Tier-3 register promotion on (PROMOTE), each named function must
# have actually promoted >=1 local into a register (proving promotion fired and the
# exec-golden output is unchanged — a promoted function's bytes differ from -O0, so
# byte-verify is bypassed and this run/exit-code equality is the gate). PROMOTES is a
# list so both the call-free (R10/R9/R8) and call-ful (callee-saved) paths can be asserted.
if(PROMOTES)
    string(REPLACE "," ";" _promotes "${PROMOTES}") # comma-separated (a literal ; would split the add_test COMMAND)
    foreach(_fn IN LISTS _promotes)
        if(NOT _r1_all MATCHES "\\[ast-promote\\] ([1-9][0-9]*) ${_fn}\n")
            message(FATAL_ERROR "expected register promotion to fire on '${_fn}', but it did not:\n${_r1_all}")
        endif()
    endforeach()
endif()

# FOLDS: with the const-fold template on (TEMPLATES), the named function must
# have actually folded at least one Binary(Lit,Lit) — proving the template fired
# and the byte-verify net kept the replay faithful (not a silent no-op/fallback).
if(FOLDS)
    if(NOT _r1_all MATCHES "\\[ast-template\\] const-fold ([1-9][0-9]*) ${FOLDS}\n")
        message(FATAL_ERROR "expected the const-fold template to fire on '${FOLDS}', but it did not (fell back or folded nothing):\n${_r1_all}")
    endif()
endif()

# NOREPLAY: the function must *not* faithfully replay (an unmodeled construct),
# proving the byte-verify safety net falls back to correct -O0 emission.
if(NOREPLAY)
    if(_r1_all MATCHES "\\[ast-replay\\] ${NOREPLAY}\n")
        message(FATAL_ERROR "expected function '${NOREPLAY}' to fall back, but it replayed:\n${_r1_all}")
    endif()
endif()

message(STATUS "ast/replay ${_name}: -O0 and replay both exit ${EXPECT_RC}")
