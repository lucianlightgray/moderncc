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

# Every figure below is a count over a corpus this build just compiled, so it
# is a property of the target's AST/RIR shape and not a portable invariant. The
# banked column was x86-64's; on arm64 the same corpus yields different block
# and callee-class counts, and comparing against x86-64's numbers failed for a
# reason that says nothing about drift. One column per architecture, re-taken
# with tools/slice-census.py as the message below instructs.
# `=` rather than `;` between key and value: a ;-joined pair is indistinguishable
# from two list elements once foreach(... IN LISTS ...) flattens it.
#
# The key is architecture AND operating system, because both move these counts.
# The corpus is compiled here, so it picks up the host's system headers: same
# arm64, `blocks` is 990 on Darwin and 1022 on Linux. Measured columns:
#
#   x86_64-Linux   941   -- 947 until the arena ternary normalisation merged six
#                           two-exit `if`s into one block each; re-taken here
#   arm64-Darwin   990   -- this machine, taken before that normalisation
#   arm64-Linux   1022   -- Debian bookworm in Docker, likewise
#
# An unbanked combination skips the exact half rather than comparing against a
# foreign column; the ratchets below still run. If this list starts to feel
# unmanageable the real fix is to give the census a corpus that does not include
# system headers, which would make one column serve everywhere.
if(CENSUS_ARCH STREQUAL "arm64" AND CENSUS_OS STREQUAL "Darwin")
    set(_census_bank "blocks=990" "inv-blocks=517" "all-internal=165"
                     "all-external=265" "mixed=85" "any-indirect=2")
elseif(CENSUS_ARCH STREQUAL "arm64" AND CENSUS_OS STREQUAL "Linux")
    set(_census_bank "blocks=1022" "inv-blocks=554" "all-internal=164"
                     "all-external=306" "mixed=83" "any-indirect=1")
elseif(CENSUS_ARCH STREQUAL "x86_64" AND
       (CENSUS_OS STREQUAL "Linux" OR CENSUS_OS STREQUAL ""))
    set(_census_bank "blocks=941" "inv-blocks=454" "all-internal=169"
                     "all-external=197" "mixed=87" "any-indirect=1")
else()
    message("slice/census: no banked column for ${CENSUS_ARCH}-${CENSUS_OS}; the "
            "exact-count half of this cell is skipped, the ratchets below still run")
    set(_census_bank "")
endif()

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
                            "numbers with tools/slice-census.py before re-banking.")
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
