execute_process(COMMAND "${CMAKE_COMMAND}" -DSRCDIR=${SRCDIR} -DBINDIR=${BINDIR}
                        "-DPINNED=${PINNED}" -P "${SCRIPT}"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
string(REGEX MATCH "frag_not_tu: [0-9]+ file[^\n]*" _csum "${_out}")
message("${_csum}")
if(_clean EQUAL 77)
    message(STATUS "frag_not_tu: the check itself skipped; SKIP")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "build/fragments-are-not-tus-known-positive: the "
                        "unmutated check is already failing, so this cell "
                        "cannot say anything about whether the standalone "
                        "compiles are being run at all:\n${_out}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -DSRCDIR=${SRCDIR} -DBINDIR=${BINDIR}
                        "-DPINNED=${PINNED}" -DMUTATE=1 -P "${SCRIPT}"
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
string(REGEX MATCH "translation unit is now[^\n]*" _msum "${_mout}")
message("${_msum}")
if(_mut EQUAL 0)
    message(FATAL_ERROR "build/fragments-are-not-tus-known-positive: every "
                        "src/*.c was compiled with -DMCC_AMALGAMATED=0, which "
                        "switches the three fragments' bodies off entirely and "
                        "makes them compile to an empty object, and the check "
                        "still agreed with its pin. It is not compiling "
                        "anything, or not reading the exit status -- the same "
                        "failure it exists to describe")
endif()
message("build/fragments-are-not-tus-known-positive: clean OK, guarded-off "
        "fragments detected")
