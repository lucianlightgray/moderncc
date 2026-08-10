set(_dump "${BINDIR}/slicerun-src-arenas.txt")
file(REMOVE "${_dump}")

file(GLOB _srcs "${SRCDIR}/src/*.c")
list(SORT _srcs)
if(NOT _srcs)
    message(FATAL_ERROR "slice/src: no compiler source found under ${SRCDIR}/src")
endif()

foreach(_s IN LISTS _srcs)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
                "${MCC}" -c "${_s}" -o "${BINDIR}/slicerun-src.o" -O1
                "-I${SRCDIR}/include" "-I${SRCDIR}/src"
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
endforeach()

if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/src: MCC_ARENA_DUMP produced nothing over "
                        "${SRCDIR}/src; the hook is not firing and this cell "
                        "would measure nothing")
endif()

message("slice/src: the corpus is the compiler's own sources. tests/exec and "
        "the c-torture sweeps are written to exercise a language feature; "
        "src/ is written to do a job, and it reaches shapes neither of them "
        "does. The f64 ternary with one integer arm -- "
        "src/mccforecast.h:220 `t > k1 ? t - k1 : 0` -- returned 0 on the "
        "device for 23 tuples and no cell in the tree ran a differential over "
        "src/ at all.")
message("slice/src: --no-ptr. The shared read-write region is one 1 MiB "
        "window for all 64 lanes, and an address outside it is clamped to "
        "offset 0 by both executors, so every lane of an out-of-region store "
        "writes the same byte. The CPU runs the lanes in order and the device "
        "does not, so the surviving value is whichever lane went last -- a "
        "real lane-order race, but a nondeterministic one, and a cell that "
        "fails on lane order is worse than no cell. combo_memo_init in "
        "src/mcccombo.h reaches it with a member at offset 1058832. The "
        "pointer arm stays covered deterministically by slice/real.")

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet --no-ptr
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")

if(${_clean} EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/src: the runner reports nothing to compare on "
                            "this backend, but MCC_GPU_REQUIRED is set")
    endif()
    message("slice/src: this backend emits nothing this cell compares, skipping")
    cmake_language(EXIT 77)
endif()

if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "slice/src: the CPU and device runners disagree over "
                        "slices taken from the compiler's own sources")
endif()
if(NOT _out MATCHES "slices=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/src: zero src slices became schedulable work; a "
                        "clean result here would mean the runners never ran")
endif()
if(NOT _out MATCHES "f64-slices=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/src: no f64 slice was compared. The defect this "
                        "cell was written for is an f64 one, so a run that "
                        "compared no f64 slice cannot have found it")
endif()

if(_out MATCHES "available=1")
    if(NOT _out MATCHES "dispatches=([1-9][0-9]*)")
        message(FATAL_ERROR "slice/src: a device was found and never dispatched")
    endif()
    execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet --no-ptr
                            --mutate
                    RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
    message("${_mout}")
    if(_mut EQUAL 0)
        message(FATAL_ERROR "slice/src: the mutated device kernel still agreed "
                            "with the CPU runner, so the differential is blind")
    endif()
    message("slice/src: clean OK, mutation detected")
elseif(MCC_GPU_REQUIRED)
    message(FATAL_ERROR "slice/src: no usable device, but MCC_GPU_REQUIRED is "
                        "set -- this cell exists to exercise a device and there "
                        "is none")
else()
    message("slice/src: no usable device; CPU runner verified over src slices")
endif()
