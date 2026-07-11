# Regression for the AST-replay frame-size low-water bug (TODO §34b): mcc's
# default -O1/-O2 must not under-size main's stack frame (which caused an
# uninitialized-stack read / non-deterministic wrong exit code). Compile the
# TU at every -O level with the plain default optimizer (no forced inline —
# inlining masked the bug) and require the deterministic correct exit code.
if(NOT MCC OR NOT SRC OR NOT OUT)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= [-DEXPECT_RC=] -P frame_regress.cmake")
endif()
if(NOT DEFINED EXPECT_RC)
    set(EXPECT_RC 42)
endif()

foreach(_o O0 O1 O2 O3)
    set(_exe "${OUT}-${_o}")
    execute_process(
        COMMAND "${MCC}" "-${_o}" "-B${BDIR}" "${SRC}" -o "${_exe}"
        RESULT_VARIABLE _crc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _crc EQUAL 0)
        message(FATAL_ERROR "compile -${_o} failed (${_crc}):\n${_out}${_err}")
    endif()
    # Run several times: the bug was a non-deterministic uninitialized read.
    foreach(_i 1 2 3 4)
        execute_process(COMMAND "${_exe}" RESULT_VARIABLE _rrc)
        if(NOT _rrc EQUAL EXPECT_RC)
            message(FATAL_ERROR "-${_o} run ${_i} exit ${_rrc}, expected ${EXPECT_RC} (frame under-sized?)")
        endif()
    endforeach()
endforeach()
message(STATUS "ast/inline-frame: -O0..-O3 all exit ${EXPECT_RC} deterministically")
