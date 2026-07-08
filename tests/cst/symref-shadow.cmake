# CST symbol-ref shadowing boundary driver (slice I).
#
# Compiles SRC (shadow.c) with MCC_CST_SYMDUMP and asserts the *current* v1
# behavior: a file-scope name shadowed inside a function resolves both uses to
# the same def (last-declaration-wins, no scope stack). This pins the documented
# limitation as a boundary — a future scope-aware resolver would map the two `x`
# uses to different defs and this assertion would (correctly) need updating.
if(NOT MCC OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -P symref-shadow.cmake")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_SYMDUMP=1
            "${MCC}" -c "${SRC}" -o "${OUT}" "-I${IDIR}"
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
set(_all "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to compile ${SRC} (rc=${_rc}):\n${_all}")
endif()

# Collect the def-node id each use of `x` resolves to.
string(REGEX MATCHALL "x' -> def\\[[0-9]+\\]" _maps "${_all}")
list(LENGTH _maps _n)
if(_n LESS 2)
    message(FATAL_ERROR "symref-shadow: expected >=2 `x` use->def mappings, got ${_n}:\n${_all}")
endif()

set(_first "")
foreach(_m IN LISTS _maps)
    string(REGEX REPLACE ".*def\\[([0-9]+)\\].*" "\\1" _id "${_m}")
    if(_first STREQUAL "")
        set(_first "${_id}")
    elseif(NOT _id STREQUAL _first)
        message(FATAL_ERROR
            "symref-shadow: two `x` uses resolved to DIFFERENT defs "
            "(${_first} vs ${_id}). The v1 resolver is last-declaration-wins and "
            "should map both to the same def — a scope-aware resolver now exists, "
            "so update this boundary test (and TODO 'CST slice-I') to assert the "
            "correct scoped mappings.\n${_all}")
    endif()
endforeach()

message(STATUS "symref-shadow: ${_n} `x` uses all resolve to def[${_first}] "
               "(documented v1 last-declaration-wins boundary)")
