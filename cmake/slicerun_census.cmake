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

# T-lin-10391: a dedicated header-free corpus, compiled -nostdinc, so no system
# header's inline bodies enter the arena and the counts are one column that
# serves every platform -- rather than tests/exec, which drags in per-platform
# headers and forced a hard-pinned column per {arch,os} that any tests/exec edit
# stranded on the boxes that could not measure it.
file(GLOB_RECURSE _srcs "${SRCDIR}/tests/census/*.c")
list(SORT _srcs)
foreach(_s IN LISTS _srcs)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
                "${MCC}" -nostdinc -c "${_s}" -o "${BINDIR}/slicerun-census.o" -O1
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
endforeach()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/census: MCC_ARENA_DUMP produced nothing")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --census
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")

# 77 from the runner means this backend emits nothing the suite exercises --
# the Metal arm has no frame kernel builder (TODO.md §5 stage M2). Treating it
# as failure graded the backend instead of the differential.
if(${_rc} EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/census: the runner reports nothing to compare on "
                            "this backend, but MCC_GPU_REQUIRED is set")
    endif()
    message("slice/census: this backend emits nothing this cell compares, skipping")
    cmake_language(EXIT 77)
endif()

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "slice/census: the census run failed")
endif()

# Every figure below is a count over tests/census, a fixed HEADER-FREE corpus
# compiled -nostdinc, so it is a property of that corpus and the callee
# classifier alone -- no system-header inline body enters the arena and the AST/
# RIR shape does not vary by host. That is what makes ONE column serve every
# platform, replacing the per-{arch,os} split that any tests/exec edit stranded
# on the boxes that could not measure it (T-lin-10391; the split's history is in
# DETAILS.md#t-lin-10391-slicecensus-strands-the-columns-the-adding-session-cannot-measure).
# To re-take after a deliberate classifier change: run this cell (it prints the
# numbers) or `slicerun --arenas <dump> --census`. Not tools/slice-census.py,
# which is a different instrument and prints none of these keys.
# `=` rather than `;` between key and value: a ;-joined pair is indistinguishable
# from two list elements once foreach(... IN LISTS ...) flattens it.
# One column for every platform. The header-free corpus carries no system-header
# inline bodies, so these AST/RIR counts are a property of the fixed corpus and
# the callee classifier alone -- not of the host. Measured on arm64-Darwin;
# lin-x64 to confirm the identical column on arm64-Linux and x86_64-Linux, after
# which the last reason for a per-{arch,os} split is gone (T-lin-10391).
set(_census_bank "blocks=50" "inv-blocks=20" "all-internal=7"
                 "all-external=6" "mixed=6" "any-indirect=1")

foreach(_entry IN LISTS _census_bank)
    string(REPLACE "=" ";" _pair "${_entry}")
    list(GET _pair 0 _k)
    list(GET _pair 1 _v)
    if(NOT _out MATCHES "${_k}=([0-9]+)")
        message(FATAL_ERROR "slice/census: no ${_k} in the census output")
    endif()
    if(NOT CMAKE_MATCH_1 EQUAL _v)
        message(FATAL_ERROR "slice/census: ${_k} is ${CMAKE_MATCH_1}, banked ${_v}. "
                            "The corpus or the callee classifier moved; re-take the "
                            "numbers by re-running this cell (it prints them) or with "
                            "`slicerun --arenas <dump> --census` before re-banking.")
    endif()
endforeach()

foreach(_pair "inv-sole-blocker;17" "inline-unblocked;4")
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
