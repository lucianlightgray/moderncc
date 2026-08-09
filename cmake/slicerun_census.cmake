# D4b step 0, ratcheted. The block-level Invoke census exists because the
# board's 12,901 / 78.01% came from an ad-hoc pass over MCC_ARENA_DUMP text that
# was never committed and does not reproduce. A census that is not pinned by a
# cell is the same kind of number, so the figures that depend only on the corpus
# and the callee classifier -- not on what the frame predicate happens to accept
# this week -- are asserted exactly here. Change the corpus or the classifier
# and this cell tells you, which is the whole difference between a measurement
# and a memory.
#
# The two payoff figures are floors rather than equalities on purpose: they move
# every time the frame predicate widens, and a cell that forbids them from
# moving would be a cell against the project.

set(_dump "${BINDIR}/slicerun-census.txt")
file(REMOVE "${_dump}")

file(GLOB_RECURSE _srcs "${SRCDIR}/tests/exec/*.c")
list(SORT _srcs)
list(LENGTH _srcs _n)
if(_n GREATER 60)
    list(SUBLIST _srcs 0 60 _srcs)
endif()
foreach(_s IN LISTS _srcs)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
                "${MCC}" -c "${_s}" -o "${BINDIR}/slicerun-census.o" -O1
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
endforeach()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/census: MCC_ARENA_DUMP produced nothing")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --census
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "slice/census: the census run failed")
endif()

foreach(_pair "blocks;947" "inv-blocks;454" "all-internal;169"
              "all-external;197" "mixed;87" "any-indirect;1")
    list(GET _pair 0 _k)
    list(GET _pair 1 _v)
    if(NOT _out MATCHES "${_k}=([0-9]+)")
        message(FATAL_ERROR "slice/census: no ${_k} in the census output")
    endif()
    if(NOT CMAKE_MATCH_1 EQUAL _v)
        message(FATAL_ERROR "slice/census: ${_k} is ${CMAKE_MATCH_1}, banked ${_v}. "
                            "The corpus or the callee classifier moved; re-take the "
                            "numbers in docs/TODO.md board item 3 before re-banking.")
    endif()
endforeach()

foreach(_pair "inv-sole-blocker;33" "inline-unblocked;7")
    list(GET _pair 0 _k)
    list(GET _pair 1 _v)
    if(NOT _out MATCHES "${_k}=([0-9]+)")
        message(FATAL_ERROR "slice/census: no ${_k} in the census output")
    endif()
    if(CMAKE_MATCH_1 LESS _v)
        message(FATAL_ERROR "slice/census: ${_k} fell from ${_v} to ${CMAKE_MATCH_1}; "
                            "the frame predicate lost ground")
    endif()
endforeach()
message("slice/census: banked")
