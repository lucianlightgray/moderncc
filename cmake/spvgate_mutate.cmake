get_filename_component(_tag "${CMD}" NAME_WE)
if(GATEARGS)
    set(_tag "${_tag}${GATEARGS}")
endif()
execute_process(COMMAND "${CMD}" ${GATEARGS} --mutate RESULT_VARIABLE rc
                OUTPUT_VARIABLE out ERROR_VARIABLE out)
message("${out}")
if(rc EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "${_tag}-known-positive: no usable device, but "
                            "MCC_GPU_REQUIRED is set -- this cell exists to "
                            "exercise a device and there is none")
    endif()
    message("${_tag}-known-positive: no usable device, skipping -- "
            "the matching differential cell reports SKIPPED for the same reason, so "
            "nothing is silently claimed here")
    cmake_language(EXIT 77)
endif()
if(rc EQUAL 0)
    message(FATAL_ERROR
        "${_tag} --mutate perturbs every emitted value and still reported OK. "
        "The differential is blind: a passing differential cell would "
        "mean nothing.")
endif()
message("known-positive detected the mutation (rc=${rc})")
