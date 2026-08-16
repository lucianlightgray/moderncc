execute_process(COMMAND "${RUNNER}" f64cross --device-or-skip
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/f64cross-known-positive: no usable device, but "
                            "MCC_GPU_REQUIRED is set")
    endif()
    message("slice/f64cross-known-positive: no usable device, skipping")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "slice/f64cross-known-positive: the unmutated cross "
                        "oracle is already failing")
endif()
if(NOT _out MATCHES "f64cross agree=")
    message(FATAL_ERROR "slice/f64cross-known-positive: the run printed no "
                        "adjudication summary, so nothing was cross-compared")
endif()

set(ENV{MCC_XCROSS_MUTATE} 1)
execute_process(COMMAND "${RUNNER}" f64cross --device-or-skip
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
unset(ENV{MCC_XCROSS_MUTATE})
message("${_mout}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "slice/f64cross-known-positive: one source was perturbed "
                        "and the majority adjudication still reported clean, so "
                        "it is blind")
endif()
if(_mut EQUAL 77)
    message("slice/f64cross-known-positive: nothing mutable here, skipping")
    cmake_language(EXIT 77)
endif()
message("slice/f64cross-known-positive: clean OK, minority source detected")
