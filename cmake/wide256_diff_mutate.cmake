cmake_minimum_required(VERSION 3.22)

execute_process(COMMAND "${CMAKE_COMMAND}" -DMCC=${MCC} -DSRCDIR=${SRCDIR}
                        -DBINDIR=${BINDIR} -DHOSTCC=${HOSTCC} -P "${SCRIPT}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    message(STATUS "wide256/gmp-diff-known-positive: the differential itself "
                   "skipped; SKIP")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "wide256/gmp-diff-known-positive: the unmutated "
                        "differential is already failing, so this cell cannot "
                        "say anything about whether the comparison runs:\n${_out}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -DMCC=${MCC} -DSRCDIR=${SRCDIR}
                        -DBINDIR=${BINDIR} -DHOSTCC=${HOSTCC} -DMUTATE=1
                        -P "${SCRIPT}"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
if(_mut EQUAL 0)
    message("${_mout}")
    message(FATAL_ERROR "wide256/gmp-diff-known-positive: one corpus operand and "
                        "one folded constant were perturbed and the differential "
                        "still reported agreement with GMP -- it is not running "
                        "the subject, not reading its output, or not comparing it")
endif()
message("wide256/gmp-diff-known-positive: clean OK, mutation detected")
