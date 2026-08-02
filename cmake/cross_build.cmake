if(XDIR STREQUAL BDIR)
    message(STATUS "cross dir is this build dir; mcc_build already covered it")
    return()
endif()

if(NOT EXISTS "${XDIR}/CMakeCache.txt")
    message(STATUS "SKIP: no configured cross build dir at ${XDIR}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E true)
    return()
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${XDIR}"
                RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "cross build in ${XDIR} failed (rc=${_rc})")
endif()
