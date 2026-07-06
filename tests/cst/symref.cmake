# CST symbol-ref correctness driver (slice I, PLAN §1 Symbols / §8.3).
# Compiles SRC with MCC_CST_SYMDUMP and asserts each use resolves to the right
# def node (checked by the source text the def/use nodes span).
if(NOT MCC OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -P symref.cmake")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_SYMDUMP=1
            "${MCC}" -c "${SRC}" -o "${OUT}" "-I${IDIR}"
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
set(_all "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to compile ${SRC} (rc=${_rc}):\n${_all}")
endif()

# Each pattern: a use of NAME must map to a def whose spanned text is also NAME.
foreach(_pair "helper' -> def.*helper'" "myglobal' -> def.*myglobal'"
              "local' -> def.*local'" "p' -> def.*p'")
    if(NOT _all MATCHES "${_pair}")
        message(FATAL_ERROR "symref: expected mapping /${_pair}/ not found in:\n${_all}")
    endif()
endforeach()
