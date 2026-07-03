# HOST.md invariant check (ctest: host-gate-invariant)
#
# Outside src/mcchost.h / src/mcchost.c, no file under src/ may test raw
# host macros (_WIN32, __APPLE__, __linux__, the BSDs, _MSC_VER, ...).
# Backends may test only MCC_TARGET_*, TARGETOS_*, MCC_IS_NATIVE, CONFIG_*
# and the normalized MCC_HOST_* predicates exported by mcchost.h.
#
# Only preprocessor conditionals (#if/#ifdef/#ifndef/#elif) count: string
# literals (e.g. the target predefine table in mccpp.c) and comments that
# merely mention a macro are fine.
#
# Usage: cmake -DSRCDIR=<repo>/src -P host_gate_check.cmake

if(NOT SRCDIR)
    message(FATAL_ERROR "host_gate_check.cmake: pass -DSRCDIR=<repo>/src")
endif()

set(_host_macros
    _WIN32 _WIN64 _MSC_VER __MINGW32__ __MINGW64__ __CYGWIN__
    __APPLE__ __linux__ __FreeBSD__ __FreeBSD_kernel__ __NetBSD__
    __OpenBSD__ __DragonFly__ __ANDROID__ __dietlibc__)

file(GLOB_RECURSE _files "${SRCDIR}/*.c" "${SRCDIR}/*.h")

set(_violations "")
foreach(_f IN LISTS _files)
    get_filename_component(_base "${_f}" NAME)
    if(_base STREQUAL "mcchost.h" OR _base STREQUAL "mcchost.c")
        continue()
    endif()

    # cheap prefilter: skip files that never mention any host macro
    file(READ "${_f}" _content)
    set(_hit OFF)
    foreach(_m IN LISTS _host_macros)
        string(FIND "${_content}" "${_m}" _pos)
        if(NOT _pos EQUAL -1)
            set(_hit ON)
            break()
        endif()
    endforeach()
    if(NOT _hit)
        continue()
    endif()

    # line scan: flag host macros only inside preprocessor conditionals
    string(REPLACE ";" "\\;" _content "${_content}")
    string(REGEX REPLACE "\r?\n" ";" _lines "${_content}")
    set(_ln 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _ln "${_ln}+1")
        if(NOT _line MATCHES "^[ \t]*#[ \t]*(if|ifdef|ifndef|elif)")
            continue()
        endif()
        foreach(_m IN LISTS _host_macros)
            # no substring matches: MCC_HOST_WIN32 must not trip on _WIN32
            if(_line MATCHES "(^|[^A-Za-z0-9_])${_m}([^A-Za-z0-9_]|$)")
                file(RELATIVE_PATH _rel "${SRCDIR}" "${_f}")
                list(APPEND _violations "src/${_rel}:${_ln}: ${_line}")
            endif()
        endforeach()
    endforeach()
endforeach()

if(_violations)
    list(JOIN _violations "\n  " _msg)
    message(FATAL_ERROR
        "host-gate invariant violated - raw host macros tested outside "
        "src/mcchost.{h,c} (see HOST.md; use MCC_HOST_* or a host_* "
        "function from mcchost.h instead):\n  ${_msg}")
endif()

message(STATUS "host-gate invariant OK: no raw host-macro tests outside src/mcchost.{h,c}")
