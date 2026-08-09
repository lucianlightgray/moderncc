get_filename_component(_tag "${GATE}" NAME_WE)
file(GLOB_RECURSE _srcs "${SRCDIR}/tests/exec/expressions/*.c"
                        "${SRCDIR}/tests/exec/codegen/*.c")
list(SORT _srcs)
list(LENGTH _srcs _n)
if(_n GREATER 40)
    list(SUBLIST _srcs 0 40 _srcs)
endif()
if(NOT _srcs)
    message(FATAL_ERROR "${_tag}-real: no corpus found under ${SRCDIR}/tests/exec")
endif()

set(_dump "${BINDIR}/${_tag}_real_arenas.txt")
file(REMOVE "${_dump}")
foreach(_f IN LISTS _srcs)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
                            "${MCC}" -w -O2 -c "${_f}" -o "${BINDIR}/${_tag}_real.o"
                    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET TIMEOUT 20)
endforeach()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "${_tag}-real: MCC_ARENA_DUMP produced nothing; the hook "
                        "is not firing and this cell would measure nothing")
endif()

execute_process(COMMAND "${GATE}" --arenas "${_dump}" --quiet
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "${_tag}-real: no usable device, but MCC_GPU_REQUIRED "
                            "is set -- this cell exists to exercise a device and "
                            "there is none")
    endif()
    message("${_tag}-real: no usable device, skipping")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "${_tag}-real: real slices disagree with the CPU replay")
endif()
if(NOT _out MATCHES "slices=([1-9][0-9]*)")
    message(FATAL_ERROR "${_tag}-real: zero slices lowered; a clean result here "
                        "would mean the differential never ran")
endif()

execute_process(COMMAND "${GATE}" --arenas "${_dump}" --mutate --quiet
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "${_tag}-real: the mutated build still reported OK, so the "
                        "real-slice differential is blind")
endif()
message("${_tag}-real: clean OK, mutation detected")
