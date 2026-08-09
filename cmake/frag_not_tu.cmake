cmake_minimum_required(VERSION 3.22)

if(NOT SRCDIR OR NOT BINDIR)
    message(FATAL_ERROR "frag_not_tu: SRCDIR and BINDIR are required")
endif()

set(_cdb "${BINDIR}/compile_commands.json")
if(NOT EXISTS "${_cdb}")
    message(STATUS "frag_not_tu: no compile_commands.json in ${BINDIR}; SKIP")
    cmake_language(EXIT 77)
endif()

file(READ "${_cdb}" _json)
string(JSON _n LENGTH "${_json}")
set(_cmd "")
math(EXPR _last "${_n} - 1")
foreach(_i RANGE 0 ${_last})
    string(JSON _f GET "${_json}" ${_i} "file")
    if(_f MATCHES "/src/mcc\\.c$")
        string(JSON _cmd GET "${_json}" ${_i} "command")
        break()
    endif()
endforeach()
if(NOT _cmd)
    message(FATAL_ERROR "frag_not_tu: no src/mcc.c entry in ${_cdb}, so the "
                        "build's own -D/-I cannot be read back and this check "
                        "would be testing flags nobody uses")
endif()

separate_arguments(_argv UNIX_COMMAND "${_cmd}")
list(GET _argv 0 _cc)
set(_flags "")
foreach(_a IN LISTS _argv)
    if(_a MATCHES "^-[DI]" AND NOT _a MATCHES "\\.c$")
        list(APPEND _flags "${_a}")
    endif()
endforeach()
if(MUTATE)
    list(APPEND _flags "-DMCC_AMALGAMATED=0")
endif()

file(GLOB _srcs "${SRCDIR}/src/*.c")
list(SORT _srcs)
list(LENGTH _srcs _nsrc)
if(_nsrc LESS 12)
    message(FATAL_ERROR "frag_not_tu: ${_nsrc} file(s) matched ${SRCDIR}/src/*.c "
                        "-- the glob found (almost) nothing, so an empty "
                        "failing set would agree with an empty pin")
endif()

set(_obj "${BINDIR}/frag-not-tu.o")
set(_failed "")
foreach(_s IN LISTS _srcs)
    get_filename_component(_b "${_s}" NAME)
    execute_process(COMMAND "${_cc}" -w -c ${_flags} "${_s}" -o "${_obj}"
                    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        list(APPEND _failed "${_b}")
        string(REGEX MATCH "[^\n]*error:[^\n]*" _first "${_err}")
        message(STATUS "  standalone rc=${_rc} ${_b}: ${_first}")
    endif()
endforeach()
file(REMOVE "${_obj}")

list(SORT _failed)
set(_want "${PINNED}")
list(SORT _want)
string(REPLACE ";" " " _fs "${_failed}")
string(REPLACE ";" " " _ws "${_want}")
message(STATUS "frag_not_tu: ${_nsrc} file(s) under src/, ${_cc}, build's own "
               "-D/-I; standalone failures: ${_fs}")

if(NOT _failed STREQUAL _want)
    message(FATAL_ERROR
        "frag_not_tu: the set of src/*.c that do NOT compile as their own "
        "translation unit is now [${_fs}], pinned [${_ws}].\n"
        "src/*.c is not a set of translation units. Seventeen of the eighteen "
        "exist only to be #include'd, the build compiles one object from src/, "
        "and the pinned three carry their whole body behind "
        "#if (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED)) without "
        "including src/mcc.h, so outside that include context they do not even "
        "parse. A tool that loops over src/*.c and does not check the exit "
        "status counts records from compiles that failed -- which is exactly "
        "what the --arenas census did before it was re-taken over the one real "
        "TU. If this moved because a fragment gained or lost #include "
        "\"mcc.h\", re-pin it here and say so; do not delete the check.")
endif()
message("frag_not_tu: OK -- ${_fs} do not stand alone; the rest compile only by "
        "accident of which headers they include, and none of them is a "
        "translation unit the build ever produces")
