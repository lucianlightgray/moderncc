# CST slice-J macro-invocation v1 boundary driver.
#
# Pins the two accepted v1 imprecisions in CST_MacroInvocation shape (see
# macro/macro_nesting.c): (1) the trailing `)` of a function-like invocation
# splits into a sibling Paren node, and (2) an object-like macro used inside
# another macro's args stays a plain token. The byte-identical round-trip — the
# load-bearing invariant — must still hold. A future precise slice-J expander
# would change the node shape and (correctly) require updating these assertions.
if(NOT MCC OR NOT SRC)
    message(FATAL_ERROR "usage: -DMCC= -DSRC= -DOUT= -DIDIR= -P macro-nesting.cmake")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env MCC_CST_SELFCHECK=1 MCC_CST_TREE=1
            "${MCC}" -c "${SRC}" -o "${OUT}" "-I${IDIR}"
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
set(_all "${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to compile ${SRC} (rc=${_rc}):\n${_all}")
endif()

# Invariant: the written source round-trips byte-identically.
if(NOT _all MATCHES "round-trip OK")
    message(FATAL_ERROR "macro-nesting: source did not round-trip:\n${_all}")
endif()

# Imprecision 2: INNER inside OUTER(...)'s args is not itself wrapped, so there
# is exactly ONE MacroInvocation node (OUTER's). A precise expander would make it 2.
string(REGEX MATCHALL "MacroInv" _mi "${_all}")
list(LENGTH _mi _n)
if(NOT _n EQUAL 1)
    message(FATAL_ERROR
        "macro-nesting: expected exactly 1 MacroInvocation (object-like macro in "
        "args left as a plain token, v1), got ${_n}. If slice-J now wraps nested "
        "object-like macros, update this boundary and TODO 'CST slice-J'.\n${_all}")
endif()

# Imprecision 1: the trailing `)` splits into a sibling Paren node that follows
# the invocation's tokens (no 'P' appears among the MacroInvocation's leaves).
if(NOT _all MATCHES "MacroInv[^P]*Paren")
    message(FATAL_ERROR
        "macro-nesting: expected the function-like invocation's trailing ')' to "
        "split into a sibling Paren node (v1). If the MacroInvocation now spans "
        "the ')', update this boundary and TODO 'CST slice-J'.\n${_all}")
endif()

message(STATUS "macro-nesting: round-trip clean; 1 MacroInvocation with trailing "
               "')' split (documented v1 boundary)")
