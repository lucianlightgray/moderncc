set(_src "${BINDIR}/slicerun-interncap.c")
set(_dump "${BINDIR}/slicerun-interncap.txt")
file(REMOVE "${_dump}")
file(WRITE "${_src}"
     "static int g0 = 1;\nstatic int g1 = 2;\nstatic int g2 = 3;\n"
     "static int g3 = 4;\nstatic int g4 = 5;\nstatic int g5 = 6;\n"
     "static int g6 = 7;\nstatic int g7 = 8;\nstatic int g8 = 9;\n"
     "static int g9 = 10;\nstatic int ga = 11;\nstatic int gb = 12;\n"
     "int f(int x) {\n"
     "\treturn g0 + g1 + g2 + g3 + g4 + g5 + g6 + g7 + g8 + g9 + ga + gb + x;\n"
     "}\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
            "MCC_ARENA_DUMP_ICAP=16"
            "${MCC}" -c "${_src}" -o "${BINDIR}/slicerun-interncap.o" -O1
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "slice/arena-intern-cap: the compile itself failed; the "
                        "interning table running out is a dump problem and must "
                        "not take the compile down with it")
endif()
if(NOT _out MATCHES "identity intern table could not grow")
    message(FATAL_ERROR "slice/arena-intern-cap: the interning table saturated "
                        "and said nothing. Returning 0 for a new symbol reads as "
                        "'no symbol' downstream, and a consumer that names an "
                        "object by that id would merge two distinct globals into "
                        "one -- silence here is how missing output becomes wrong "
                        "output")
endif()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/arena-intern-cap: no dump was written at all")
endif()
file(READ "${_dump}" _txt)
if(NOT _txt MATCHES "\\[intern-overflow\\]")
    message(FATAL_ERROR "slice/arena-intern-cap: the dump carries no overflow "
                        "marker, so a consumer reading the file alone cannot "
                        "tell a truncated dump from a complete one")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet
                RESULT_VARIABLE _rc2 OUTPUT_VARIABLE _out2 ERROR_VARIABLE _out2)
message("${_out2}")
if(_rc2 EQUAL 0)
    message(FATAL_ERROR "slice/arena-intern-cap: slicerun consumed a dump that "
                        "declares its own identities unreliable and reported "
                        "success. Column 11 is what a global is relocated and "
                        "named by; a run over it proves nothing")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${BINDIR}/slicerun-interncap-ok.txt"
            "${MCC}" -c "${_src}" -o "${BINDIR}/slicerun-interncap.o" -O1
    RESULT_VARIABLE _rc3 OUTPUT_VARIABLE _out3 ERROR_VARIABLE _out3)
if(_out3 MATCHES "identity intern table could not grow")
    message(FATAL_ERROR "slice/arena-intern-cap: the table failed at its default "
                        "capacity over six globals, so the growth path is not "
                        "working and every real dump is truncated")
endif()

message("slice/arena-intern-cap: saturation is loud, marked in the dump, and "
        "refused by the consumer; the default capacity grows instead")
