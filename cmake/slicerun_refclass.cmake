set(_dump "${BINDIR}/slicerun-refclass.txt")
file(REMOVE "${_dump}")

file(GLOB_RECURSE _srcs "${SRCDIR}/tests/exec/*.c")
list(SORT _srcs)
list(LENGTH _srcs _n)
if(_n GREATER 60)
    list(SUBLIST _srcs 0 60 _srcs)
endif()
file(WRITE "${BINDIR}/slicerun-thread-probe.c"
"extern int pthread_mutex_lock(void *);\n"
"extern int pthread_create(void *, void *, void *, void *);\n"
"extern int pthread_join(long, void *);\n"
"extern int mtx_lock(void *);\n"
"extern int thrd_join(long, int *);\n"
"extern int __atomic_load_4(void *, int);\n"
"extern int __sync_fetch_and_add_4(void *, int);\n"
"extern int strlen(const char *);\n"
"int g_lk[8];\n"
"int g_th[8];\n"
"char g_nm[8];\n"
"int slicerun_thread_probe(int n) {\n"
"	int s = 0;\n"
"	pthread_mutex_lock(g_lk);\n"
"	pthread_create(g_th, 0, 0, 0);\n"
"	pthread_join(0, g_th);\n"
"	mtx_lock(g_lk);\n"
"	thrd_join(0, &n);\n"
"	__atomic_load_4(g_lk, 0);\n"
"	__sync_fetch_and_add_4(g_lk, 1);\n"
"	strlen(g_nm);\n"
"	s += n;\n"
"	return s;\n"
"}\n")
list(APPEND _srcs "${BINDIR}/slicerun-thread-probe.c")

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

if(NOT _out MATCHES "kind-invoke +nodes=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: no kind-invoke bucket")
endif()
set(_inv "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "kind-invoke-thread +nodes=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: no kind-invoke-thread bucket")
endif()
set(_invthr "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "kind-invoke-partition-sum=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: slicerun printed no kind-invoke "
                        "partition check. kind-invoke-thread is carved out of "
                        "kind-invoke and must not lose or duplicate a node: a "
                        "thread call is refused today at the same node with the "
                        "same outcome, only under its own name")
endif()
math(EXPR _invsum "${_inv} + ${_invthr}")
if(NOT CMAKE_MATCH_1 EQUAL _invsum)
    message(FATAL_ERROR "slice/refusal-classes: kind-invoke ${_inv} + "
                        "kind-invoke-thread ${_invthr} = ${_invsum}, against "
                        "${CMAKE_MATCH_1} refused Invoke nodes counted by kind")
endif()

if(NOT _out MATCHES "block/stmt-invoke +n=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: no block/stmt-invoke bucket")
endif()
set(_binv "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "block/stmt-thread +n=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: no block/stmt-thread bucket")
endif()
set(_binvthr "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "stmt-invoke-partition-sum=([0-9]+)")
    message(FATAL_ERROR "slice/refusal-classes: slicerun printed no "
                        "block/stmt-invoke partition check")
endif()
math(EXPR _binvsum "${_binv} + ${_binvthr}")
if(NOT CMAKE_MATCH_1 EQUAL _binvsum)
    message(FATAL_ERROR "slice/refusal-classes: block/stmt-invoke ${_binv} + "
                        "block/stmt-thread ${_binvthr} = ${_binvsum}, against "
                        "${CMAKE_MATCH_1} blocks whose attributed statement is "
                        "an Invoke")
endif()

foreach(_tc "pthread" "c11-threads" "atomic-builtin")
    if(NOT _out MATCHES "kind-invoke-thread/${_tc} +nodes=([1-9][0-9]*)")
        message(FATAL_ERROR "slice/refusal-classes: thread class ${_tc} is empty. "
                            "slicerun-thread-probe.c calls all three families in "
                            "statement position, so an empty class means the "
                            "name classifier stopped firing, not that the corpus "
                            "moved. A partition-preserving split that classifies "
                            "nothing still sums exactly right")
    endif()
endforeach()

message("slice/refusal-classes: partition holds over ${_bucket} refused global refs; "
        "kind-invoke splits ${_inv}+${_invthr}, block/stmt-invoke splits "
        "${_binv}+${_binvthr}; counts are deliberately not banked, they move "
        "whenever the predicate widens")
