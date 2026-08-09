# -fdepth-census against ground truth that is known by construction.
#
# tests/depthcensus/known_depth.c calls fact(0..7), fib(0..9) and chain(12), so
# every figure below is derivable by hand and none of it depends on timing, on
# the host, or on the optimiser:
#
#   fact  1+1+2+3+4+5+6+7            =  29 calls, deepest chain 7, width 1
#   fib   sum of the fib call counts = 276 calls, deepest chain 9, width 7
#   chain 12+1                       =  13 calls, deepest chain 13, width 1
#   main                             =   1 call
#
# The width column is the point of the cell. Depth alone cannot tell a chain
# recursion from a tree one, and the two want different execution strategies, so
# the census records calls per level and the frontier width at a level is that
# count over the number of root invocations. fact and chain are width 1 at every
# level; fib is not. A cell that only checked `max=` would pass on a census that
# had lost the width data entirely.
#
# Three teeth:
#   1. the map file names every instrumented body,
#   2. the runtime dump matches the arithmetic above exactly,
#   3. the negative control -- without -fdepth-census there is no dump at all,
#      so tooth 2 is attributable to the flag and not to something else writing
#      the file.

set(_map "${BINDIR}/depth-census-known.map")
set(_dump "${BINDIR}/depth-census-known.dump")
set(_exe "${BINDIR}/depth-census-known.exe")
set(_src "${SRCDIR}/tests/depthcensus/known_depth.c")
file(REMOVE "${_map}" "${_dump}" "${_exe}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_DEPTH_CENSUS_MAP=${_map}"
            "${MCC}" -w -fdepth-census -O0 "${_src}" -o "${_exe}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _log ERROR_VARIABLE _log)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_exe}")
    message(FATAL_ERROR "depth-census-control: the instrumented build failed\n${_log}")
endif()
if(NOT EXISTS "${_map}")
    message(FATAL_ERROR "depth-census-control: -fdepth-census wrote no map file")
endif()
file(READ "${_map}" _maptxt)
foreach(_fn fact fib chain main)
    if(NOT _maptxt MATCHES "\\[dfn\\] id=[0-9]+ fn=${_fn} ")
        message(FATAL_ERROR "depth-census-control: ${_fn} is missing from the "
                            "map, so one body was not instrumented\n${_maptxt}")
    endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MCC_DEPTH_CENSUS=${_dump}"
                        "${_exe}"
                RESULT_VARIABLE _rrc OUTPUT_VARIABLE _rout ERROR_VARIABLE _rout)
if(NOT _rrc EQUAL 0)
    message(FATAL_ERROR "depth-census-control: the instrumented program failed "
                        "(rc=${_rrc})\n${_rout}")
endif()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "depth-census-control: the instrumented program wrote "
                        "no dump; the runtime helper never ran")
endif()
file(READ "${_dump}" _dtxt)
message("${_dtxt}")

# id -> expected "calls max wmax", read back through the map so the check does
# not depend on the order bodies happen to be numbered in.
set(_want_fact "29 7 1")
set(_want_fib "276 9 7")
set(_want_chain "13 13 1")
set(_want_main "1 1 1")
foreach(_fn fact fib chain main)
    if(NOT _maptxt MATCHES "\\[dfn\\] id=([0-9]+) fn=${_fn} ")
        message(FATAL_ERROR "depth-census-control: no id for ${_fn}")
    endif()
    set(_id "${CMAKE_MATCH_1}")
    if(NOT _dtxt MATCHES "\\[depth\\] id=${_id} calls=([0-9]+) max=([0-9]+) roots=[0-9]+ wmax=([0-9]+) ")
        message(FATAL_ERROR "depth-census-control: no [depth] record for ${_fn} "
                            "(id=${_id})\n${_dtxt}")
    endif()
    set(_got "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3}")
    if(NOT "${_got}" STREQUAL "${_want_${_fn}}")
        message(FATAL_ERROR
            "depth-census-control: ${_fn} measured 'calls max wmax' = ${_got}, "
            "expected ${_want_${_fn}}. These are hand-derivable from the "
            "fixture, so a mismatch is the census being wrong, not the "
            "fixture drifting")
    endif()
endforeach()

set(_dump2 "${BINDIR}/depth-census-off.dump")
set(_exe2 "${BINDIR}/depth-census-off.exe")
file(REMOVE "${_dump2}" "${_exe2}")
execute_process(COMMAND "${MCC}" -w -O0 "${_src}" -o "${_exe2}"
                RESULT_VARIABLE _rc2 OUTPUT_VARIABLE _log2 ERROR_VARIABLE _log2)
if(NOT _rc2 EQUAL 0)
    message(FATAL_ERROR "depth-census-control: the uninstrumented build failed\n${_log2}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MCC_DEPTH_CENSUS=${_dump2}"
                        "${_exe2}" RESULT_VARIABLE _rrc2
                OUTPUT_VARIABLE _rout2 ERROR_VARIABLE _rout2)
if(NOT _rrc2 EQUAL 0)
    message(FATAL_ERROR "depth-census-control: the uninstrumented program failed\n${_rout2}")
endif()
if(EXISTS "${_dump2}")
    message(FATAL_ERROR "depth-census-control: a build WITHOUT -fdepth-census "
                        "still produced a census dump, so the figures above are "
                        "not attributable to the flag")
endif()

message("depth-census-control: OK -- 4 bodies instrumented, fact/fib/chain/main "
        "matched their hand-derived call, depth and width figures, and the "
        "uninstrumented control produced no dump")
