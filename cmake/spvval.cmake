if(NOT SPIRV_VAL OR NOT EXISTS "${SPIRV_VAL}")
    message("spv-validate: spirv-val is not installed, skipping")
    cmake_language(EXIT 77)
endif()

if(CORRUPT)
    set(_tag "spv-validate-known-positive")
else()
    set(_tag "spv-validate")
endif()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")

set(_args --emit-only "${WORKDIR}")
if(CORRUPT)
    list(APPEND _args --corrupt "${CORRUPT}")
endif()

execute_process(COMMAND "${GATE}" ${_args}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _rc EQUAL 0)
    message("${_out}")
    message(FATAL_ERROR "${_tag}: the emitter produced no modules (rc=${_rc})")
endif()
if(NOT _out MATCHES "emitted=([0-9]+)")
    message("${_out}")
    message(FATAL_ERROR "${_tag}: the emitter printed no module count, so this "
                        "cell cannot tell 'all modules valid' from 'no module "
                        "was ever emitted'")
endif()
set(_claimed "${CMAKE_MATCH_1}")

file(GLOB _mods "${WORKDIR}/*.spv")
list(LENGTH _mods _n)
if(NOT _n EQUAL _claimed)
    message(FATAL_ERROR "${_tag}: the emitter claims ${_claimed} modules but "
                        "${_n} landed in ${WORKDIR}")
endif()
if(_n LESS EXPECT)
    message(FATAL_ERROR "${_tag}: only ${_n} modules to validate, and this cell "
                        "is meaningless below ${EXPECT}")
endif()

set(_bad 0)
set(_ok 0)
set(_first "")
foreach(_m IN LISTS _mods)
    execute_process(COMMAND "${SPIRV_VAL}" "${_m}"
                    RESULT_VARIABLE _vrc OUTPUT_VARIABLE _vout ERROR_VARIABLE _vout)
    if(_vrc EQUAL 0)
        math(EXPR _ok "${_ok} + 1")
    else()
        math(EXPR _bad "${_bad} + 1")
        if(_first STREQUAL "")
            get_filename_component(_fn "${_m}" NAME)
            set(_first "${_fn}: ${_vout}")
        endif()
    endif()
endforeach()

if(CORRUPT)
    if(_bad EQUAL 0)
        message(FATAL_ERROR
            "${_tag}: every one of the ${_n} modules had word ${CORRUPT} flipped "
            "and spirv-val still accepted all of them. The validator cell cannot "
            "go red, so its green means nothing.")
    endif()
    message("${_tag}: spirv-val rejected ${_bad} of ${_n} corrupted modules "
            "(first: ${_first})")
else()
    if(NOT _bad EQUAL 0)
        message(FATAL_ERROR "${_tag}: spirv-val rejected ${_bad} of ${_n} emitted "
                            "modules. First failure -- ${_first}")
    endif()
    message("${_tag}: spirv-val validated ${_ok} emitted SPIR-V modules, all clean")
endif()
