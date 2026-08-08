execute_process(COMMAND "${CMD}" --mutate RESULT_VARIABLE rc OUTPUT_VARIABLE out
                ERROR_VARIABLE out)
message("${out}")
if(rc EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "spvgate-known-positive: no usable device, but "
                            "MCC_GPU_REQUIRED is set -- this cell exists to "
                            "exercise a device and there is none")
    endif()
    message("spvgate-known-positive: no usable device, skipping -- "
            "gpu/spv-slice-differential reports SKIPPED for the same reason, so "
            "nothing is silently claimed here")
    return()
endif()
if(rc EQUAL 0)
    message(FATAL_ERROR
        "spvgate --mutate rewrites every OpIAdd to OpISub and still reported OK. "
        "The differential is blind: a passing gpu/spv-slice-differential would "
        "mean nothing.")
endif()
message("known-positive detected the mutation (rc=${rc})")
