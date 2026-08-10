# The largest named drop between "the engine accepted this block" and "a device
# ran it and agreed" is funnel-lower-no-live-in: mcc_slice_frame_kernel_build
# refuses a frame whose live-in vector is empty. Measured over gcc c-torture at
# 9fe32126 it is 242 blocks and every one of them is `return <constant>`, so the
# refusal is correct and the bucket is closed. This cell is what keeps that true:
# it asserts the classes partition the bucket exactly, and that the six classes
# naming storage in an EVALUATED position stay empty. Those six are an
# invariant, not a corpus fact -- a block that names a local, a global, a member,
# an arrow, a load or a bailout in code either executor actually evaluates, and
# gets no slot for it, is a live-in the kernel cannot read.
#
# The counts are deliberately not banked. A floor on the bucket would go red
# when somebody narrows the acceptance predicate, which is a fix and not a
# regression.

set(_dump "${BINDIR}/slicerun-noslot.txt")
file(REMOVE "${_dump}")

file(GLOB_RECURSE _srcs "${SRCDIR}/tests/exec/*.c")
list(SORT _srcs)
if(NOT _srcs)
    message(FATAL_ERROR "slice/noslot-classes: no corpus under ${SRCDIR}/tests/exec")
endif()
list(LENGTH _srcs _n)
if(_n GREATER 60)
    list(SUBLIST _srcs 0 60 _srcs)
endif()
list(LENGTH _srcs _n)

foreach(_s IN LISTS _srcs)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
                "${MCC}" -c "${_s}" -o "${BINDIR}/slicerun-noslot.o" -O1
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
endforeach()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/noslot-classes: MCC_ARENA_DUMP produced nothing")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")

if(_rc EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/noslot-classes: the runner compares nothing on "
                            "this backend, but MCC_GPU_REQUIRED is set")
    endif()
    message("slice/noslot-classes: no frame kernels on this backend, skipping")
    cmake_language(EXIT 77)
endif()
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "slice/noslot-classes: the arena run failed")
endif()

# Without a device every accepted block stops at funnel-no-device, which is
# upstream of the kernel builder, so the bucket this cell classifies is empty by
# construction and every assertion below would pass having measured nothing.
if(NOT _out MATCHES "available=1")
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/noslot-classes: no device, but MCC_GPU_REQUIRED "
                            "is set")
    endif()
    message("slice/noslot-classes: no device, so no block reaches the kernel "
            "builder and there is no bucket to classify; skipping")
    cmake_language(EXIT 77)
endif()

if(NOT _out MATCHES "funnel-seen=([0-9]+)")
    message(FATAL_ERROR "slice/noslot-classes: no frame-funnel line")
endif()
set(_seen "${CMAKE_MATCH_1}")
if(NOT _out MATCHES "funnel-drops-sum=([0-9]+)")
    message(FATAL_ERROR "slice/noslot-classes: the funnel printed no partition "
                        "check")
endif()
if(NOT CMAKE_MATCH_1 EQUAL _seen)
    message(FATAL_ERROR "slice/noslot-classes: the funnel drops sum to "
                        "${CMAKE_MATCH_1} against ${_seen} blocks seen, so it is "
                        "no longer a partition and the bucket below could be "
                        "short by blocks that fell out of it entirely")
endif()

if(NOT _out MATCHES "funnel-lower-no-live-in=([0-9]+)")
    message(FATAL_ERROR "slice/noslot-classes: no funnel-lower-no-live-in bucket")
endif()
set(_bucket "${CMAKE_MATCH_1}")
if(_bucket LESS 10)
    message(FATAL_ERROR "slice/noslot-classes: only ${_bucket} blocks reach the "
                        "kernel builder with an empty live-in vector over this "
                        "corpus. The corpus stopped exercising the shape, so the "
                        "empty-class assertions below would be green having "
                        "classified nothing")
endif()

if(NOT _out MATCHES "noslot-sum=([0-9]+)")
    message(FATAL_ERROR "slice/noslot-classes: slicerun printed no noslot "
                        "partition check")
endif()
if(NOT CMAKE_MATCH_1 EQUAL _bucket)
    message(FATAL_ERROR "slice/noslot-classes: the noslot classes sum to "
                        "${CMAKE_MATCH_1} against a bucket of ${_bucket}")
endif()

foreach(_cls "local-ref-unseen" "global-ref-unseen" "member-unseen"
             "arrow-unseen" "load-unseen" "bailout")
    if(NOT _out MATCHES "noslot/${_cls}=([0-9]+)")
        message(FATAL_ERROR "slice/noslot-classes: class ${_cls} is not printed")
    endif()
    if(NOT CMAKE_MATCH_1 EQUAL 0)
        message(FATAL_ERROR "slice/noslot-classes: ${CMAKE_MATCH_1} blocks are in "
                            "noslot/${_cls}. That is a block whose kernel would "
                            "read nothing while its own code names storage in a "
                            "position both executors evaluate -- a live-in the "
                            "collector did not recognise, which is a coverage bug "
                            "and not a correct refusal. Classify it before "
                            "widening this assertion")
    endif()
endforeach()

if(NOT _out MATCHES "noslot/return-literal=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/noslot-classes: not one block in the bucket is a "
                        "`return <literal>`. That is the whole content of the "
                        "bucket as measured, so either the classifier stopped "
                        "firing or the bucket now holds something nobody has "
                        "looked at")
endif()

# The empty-class assertions above cannot fail if the walk never reaches a
# statement subtree at all -- a classifier that only ever looks at the return
# value would satisfy every one of them. This probe is a block whose sole
# statement names two locals and whose return value is a literal, and it must
# land in a class that only the statement walk can produce.
set(_probe "${BINDIR}/slicerun-noslot-probe.c")
set(_pdump "${BINDIR}/slicerun-noslot-probe.txt")
file(REMOVE "${_pdump}")
file(WRITE "${_probe}"
"int noslot_probe(int x, int y) {\n"
"	x ? y : 1;\n"
"	return 7;\n"
"}\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_pdump}"
            "${MCC}" -c "${_probe}" -o "${BINDIR}/slicerun-noslot-probe.o" -O1
    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_pdump}")
    message(FATAL_ERROR "slice/noslot-classes: the positive control did not "
                        "compile")
endif()
execute_process(COMMAND "${RUNNER}" --arenas "${_pdump}" --quiet
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _pout ERROR_VARIABLE _pout)
message("${_pout}")
if(NOT _pout MATCHES "noslot/discarded-operand=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/noslot-classes: the positive control produced no "
                        "noslot/discarded-operand. Its one statement is a "
                        "discarded ternary over two locals and its return value "
                        "is a literal, so the walk that classifies this bucket is "
                        "not entering statements and the six empty classes above "
                        "prove nothing")
endif()

message("slice/noslot-classes: ${_bucket} blocks reach the kernel builder with no "
        "live-in over ${_n} sources; the classes partition it exactly, the six "
        "classes that would name an unrecognised live-in are empty, and the "
        "statement walk is exercised by the positive control")
