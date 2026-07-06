# CST structural-hash invariance gate (slice C/G, PLAN §3 / §8.4) on real trees.
# compact.c and spaced.c share token structure but differ in whitespace/comments
# -> equal root H_s. changed.c changes a token -> different H_s.
#   cmake -DMCC= -DDIR= -DIDIR= -DOUT= -P hashinv.cmake
if(NOT MCC OR NOT DIR)
    message(FATAL_ERROR "usage: -DMCC= -DDIR= -DIDIR= -DOUT= -P hashinv.cmake")
endif()

function(roothash src outvar)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_HASHDUMP=1
                "${MCC}" -c "${src}" -o "${OUT}" "-I${IDIR}"
        OUTPUT_VARIABLE _o ERROR_VARIABLE _e RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "mcc failed on ${src}:\n${_o}${_e}")
    endif()
    if(NOT "${_o}${_e}" MATCHES "roothash: ([0-9a-f]+)")
        message(FATAL_ERROR "no roothash for ${src}:\n${_o}${_e}")
    endif()
    set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

roothash("${DIR}/compact.c" _h_compact)
roothash("${DIR}/spaced.c" _h_spaced)
roothash("${DIR}/changed.c" _h_changed)

if(NOT _h_compact STREQUAL _h_spaced)
    message(FATAL_ERROR "hash NOT whitespace-invariant: ${_h_compact} != ${_h_spaced}")
endif()
if(_h_compact STREQUAL _h_changed)
    message(FATAL_ERROR "hash did not change on a token edit: ${_h_compact}")
endif()
message(STATUS "hashinv OK: ws-invariant=${_h_compact}, changed=${_h_changed}")
