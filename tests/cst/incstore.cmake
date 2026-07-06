# CST D3 live-capture gate (docs/CST.md §D3): real included files are interned
# as hash-consed SourceFile templates and each written IncludeDirective binds to
# its file's template. driver.c pulls leaf.h in via wrap.h and directly twice —
# all collapse to ONE template, and both direct include nodes share it.
#   cmake -DMCC= -DSRC= -DOUT= -DIDIR= -P incstore.cmake
if(NOT MCC OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -P incstore.cmake")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_SELFCHECK=1 MCC_CST_STORE=1
            "${MCC}" -c "${SRC}" -o "${OUT}" "-I${IDIR}"
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
set(_all "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to compile ${SRC} (rc=${_rc}):\n${_all}")
endif()
if(NOT _all MATCHES "round-trip OK")
    message(FATAL_ERROR "CST round-trip did not hold:\n${_all}")
endif()

# Exactly two templates: wrap.h and leaf.h (leaf.h deduped across 3 references).
if(NOT _all MATCHES "CST store: 2 templates")
    message(FATAL_ERROR "expected 2 hash-consed templates:\n${_all}")
endif()
# leaf.h's template is full-concrete (its #ifndef guard is a PPConditional) and
# every template renders back to its exact bytes.
if(NOT _all MATCHES "[1-9][0-9]* PPConditional")
    message(FATAL_ERROR "leaf.h's include guard was not captured full-concrete:\n${_all}")
endif()
if(_all MATCHES "render_identity [0-9]+/[0-9]+ MISMATCH")
    message(FATAL_ERROR "a SourceFile template did not round-trip:\n${_all}")
endif()

# The two direct `#include \"leaf.h\"` nodes must bind to the SAME template id.
string(REGEX MATCHALL "include node [0-9]+ -> template ([0-9]+)" _binds "${_all}")
list(LENGTH _binds _n)
if(_n LESS 3)
    message(FATAL_ERROR "expected 3 IncludeDirective bindings, got ${_n}:\n${_all}")
endif()
# Collect the template id of each binding.
set(_ids "")
foreach(_b IN LISTS _binds)
    string(REGEX REPLACE ".*template ([0-9]+)$" "\\1" _id "${_b}")
    list(APPEND _ids "${_id}")
endforeach()
list(GET _ids 1 _id2)   # 2nd include:  #include "leaf.h"
list(GET _ids 2 _id3)   # 3rd include:  #include "leaf.h"
if(NOT _id2 STREQUAL _id3)
    message(FATAL_ERROR "the two direct leaf.h includes bound to different "
                        "templates (${_id2} vs ${_id3}):\n${_all}")
endif()
# None may be unbound (0xffffffff).
foreach(_id IN LISTS _ids)
    if(_id STREQUAL "4294967295")
        message(FATAL_ERROR "an IncludeDirective was left unbound:\n${_all}")
    endif()
endforeach()
message(STATUS "incstore OK: 2 templates; both leaf.h includes share template ${_id2}")
