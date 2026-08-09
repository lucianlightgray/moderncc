set(_dump "${BINDIR}/slicerun-refclass.txt")
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
                "${MCC}" -c "${_s}" -o "${BINDIR}/slicerun-refclass.o" -O1
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
endforeach()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/refusal-classes: MCC_ARENA_DUMP produced nothing")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --refusals --quiet
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "slice/refusal-classes: the refusal run failed")
endif()

if(NOT _out MATCHES "ref-not-local +nodes=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: no ref-not-local bucket in the "
                        "output; there is nothing to attribute")
endif()
set(_bucket "${CMAKE_MATCH_1}")
if(_bucket LESS 100)
    message(FATAL_ERROR "slice/refusal-classes: only ${_bucket} refused global "
                        "refs over 60 sources; the corpus stopped exercising the "
                        "cause and a green result here would mean nothing")
endif()

if(NOT _out MATCHES "ref-not-local-classes-sum=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: slicerun printed no partition "
                        "check, so the sub-attribution was never verified. The "
                        "classes are a partition of the bucket, not a sample of "
                        "it, and a classifier that drops a referent shape into no "
                        "class at all would quietly shrink the table docs/TODO.md "
                        "publishes")
endif()
if(NOT CMAKE_MATCH_1 EQUAL _bucket)
    message(FATAL_ERROR "slice/refusal-classes: classes sum to ${CMAKE_MATCH_1} "
                        "against a bucket of ${_bucket}")
endif()

foreach(_cls "func-symbol" "global-aggregate" "global-array" "global-scalar-int")
    if(NOT _out MATCHES "ref-not-local/${_cls} +nodes=([1-9][0-9]*)")
        message(FATAL_ERROR "slice/refusal-classes: class ${_cls} is empty over "
                            "this corpus; either the classifier stopped firing or "
                            "the corpus no longer contains the shape. The sum "
                            "check above cannot catch this on its own -- a "
                            "classifier collapsing everything into `other` still "
                            "partitions the bucket exactly")
    endif()
endforeach()

if(NOT _out MATCHES "ref-not-local/func-symbol +nodes=([0-9]+) [^\n]*callee=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: no callee attribution")
endif()
if(CMAKE_MATCH_2 LESS 1)
    message(FATAL_ERROR "slice/refusal-classes: no func-symbol ref was in callee "
                        "position, which contradicts the finding that made the "
                        "bucket worth splitting -- 47% of it is the callee Ref of "
                        "an Invoke that kind-invoke refuses regardless")
endif()

if(NOT _out MATCHES "ref-accepted/local-lvalue +nodes=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/refusal-classes: no accepted local lvalue Refs; "
                        "the accepting-side census is not running, and it is the "
                        "half that measures non-LVAL local Refs read as values")
endif()

message("slice/refusal-classes: partition holds over ${_bucket} refused global refs; "
        "counts are deliberately not banked, they move whenever the predicate widens")
