# Known-positive for the threading-to-dependency mapping. Like the effect arm
# this needs no device: every construct here is a graph property checked on the
# CPU, so --device-or-skip is deliberately NOT passed.
#
# The mutated arm drops the edges join and a conflicting lock derive. If the
# ordering assertions still pass with no edge in the graph, they were never
# observing the schedule and every mapping built on them would be vacuous --
# which is exactly the shape the nine vacuous slice cells had.

execute_process(COMMAND "${RUNNER}" thread RESULT_VARIABLE _clean
                OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "slice/thread-known-positive: the unmutated mapping is "
                        "already failing")
endif()

execute_process(COMMAND "${RUNNER}" thread --mutate RESULT_VARIABLE _mut
                OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "slice/thread-known-positive: the join and lock edges "
                        "were removed and the ordering assertions still passed, "
                        "so the schedule is not being observed")
endif()
if(_mut EQUAL 77)
    message(FATAL_ERROR "slice/thread-known-positive: the mutated arm skipped; a "
                        "CPU-only graph check has nothing to skip on")
endif()
message("slice/thread-known-positive: clean OK, edge-free graph detected")
