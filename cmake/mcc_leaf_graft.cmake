# D4b step 2: the leaf graft during a real compile, not in the harness.
#
# slice/inline proves the graft in tools/slicerun.c, whose leaf resolver is a
# name-indexed side table built by the harness itself. This cell proves the
# other resolver: mcc's own ast_inline_pool, consulted by ast_slice_leaf_pool
# while the compiler is running. Both arms below pass --no-inline, so the
# harness never grafts anything and invoke-seen is 0 in each -- whatever an arm
# gains, it gained because mcc emitted an arena that already had no AST_Invoke
# in it.
#
# Three teeth, the same three slice/inline uses, all taken against dumps that a
# real compile produced:
#
#   1. [slice-inline] invoke-inlined > 0   the compiler-side graft fired.
#   2. frame-stmts and frame-compared      the ungrafted dump compares frame
#      strictly greater than with          runs with ZERO statements in them, so
#      MCC_AST_SLICE_INLINE=0              it is blind to any perturbation and a
#                                          green result over it means nothing.
#   3. --mutate red on frame-mismatches    and green on the ungrafted dump, so
#      specifically                        the redness is attributable to the
#                                          grafted call and not to the
#                                          expression-slice arm reddening the
#                                          process on its own.
#
# -O2, because the pool is populated by ast_inline_retain under
# -finline-functions; at -O0 and -O1 it is empty and the hook has nothing to
# resolve, which is a fact about the compiler and not about the graft.

set(_on "${BINDIR}/mcc-leaf-graft-on.txt")
set(_off "${BINDIR}/mcc-leaf-graft-off.txt")
file(REMOVE "${_on}" "${_off}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_on}"
            "${MCC}" -w -O2 -c "${SRCDIR}/tests/gpu/inline_leaf.c"
            -o "${BINDIR}/mcc-leaf-graft.o"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _log ERROR_VARIABLE _log)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_on}")
    message(FATAL_ERROR "mcc-leaf-graft: the grafting compile failed\n${_log}")
endif()
message("${_log}")
if(NOT _log MATCHES "\\[slice-inline\\] invoke-seen=[0-9]+ invoke-inlined=([1-9][0-9]*)")
    message(FATAL_ERROR "mcc-leaf-graft: mcc grafted no leaf callee, so the "
                        "compiler-side resolver never ran")
endif()
set(_grafts "${CMAKE_MATCH_1}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_off}"
            "MCC_AST_SLICE_INLINE=0"
            "${MCC}" -w -O2 -c "${SRCDIR}/tests/gpu/inline_leaf.c"
            -o "${BINDIR}/mcc-leaf-graft.o"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _log2 ERROR_VARIABLE _log2)
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_off}")
    message(FATAL_ERROR "mcc-leaf-graft: the ungrafted compile failed\n${_log2}")
endif()
if(_log2 MATCHES "\\[slice-inline\\]")
    message(FATAL_ERROR "mcc-leaf-graft: MCC_AST_SLICE_INLINE=0 still armed the "
                        "hook, so the two arms are not a real A/B")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_on}" --quiet --no-inline
                RESULT_VARIABLE _on_rc OUTPUT_VARIABLE _ontxt ERROR_VARIABLE _ontxt)
message("${_ontxt}")
execute_process(COMMAND "${RUNNER}" --arenas "${_off}" --quiet --no-inline
                RESULT_VARIABLE _off_rc OUTPUT_VARIABLE _offtxt ERROR_VARIABLE _offtxt)
message("${_offtxt}")
# 77 from the runner means this backend emits nothing the cell compares -- the
# Metal arm has no frame kernel builder (TODO.md §5 stage M2), and every
# frame-stmts assertion below is about that executor.
if(_on_rc EQUAL 77 OR _off_rc EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "mcc-leaf-graft: nothing to compare on this backend, "
                            "but MCC_GPU_REQUIRED is set")
    endif()
    message("mcc-leaf-graft: this backend emits no frame kernel, skipping")
    cmake_language(EXIT 77)
endif()
if(NOT _on_rc EQUAL 0 OR NOT _off_rc EQUAL 0)
    message(FATAL_ERROR "mcc-leaf-graft: the unmutated differential is already "
                        "failing")
endif()
foreach(_t "${_ontxt}" "${_offtxt}")
    if(NOT _t MATCHES "invoke-seen=0 invoke-inlined=0")
        message(FATAL_ERROR "mcc-leaf-graft: the harness grafted something of "
                            "its own, so this cell is not measuring mcc")
    endif()
endforeach()

if(NOT _ontxt MATCHES "frame-stmts=([0-9]+)")
    message(FATAL_ERROR "mcc-leaf-graft: no frame-stmts line in the grafted run")
endif()
set(_ons "${CMAKE_MATCH_1}")
if(NOT _offtxt MATCHES "frame-stmts=([0-9]+)")
    message(FATAL_ERROR "mcc-leaf-graft: no frame-stmts line in the ungrafted run")
endif()
set(_offs "${CMAKE_MATCH_1}")
if(NOT _ons GREATER _offs)
    message(FATAL_ERROR "mcc-leaf-graft: the compiler grafted ${_grafts} call(s) "
                        "and no new statement reached either executor "
                        "(frame-stmts ${_offs} -> ${_ons})")
endif()

if(NOT _ontxt MATCHES "available=1")
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "mcc-leaf-graft: no usable device, but "
                            "MCC_GPU_REQUIRED is set")
    endif()
    message("mcc-leaf-graft: no usable device; CPU runner verified over the "
            "compiler's graft (${_grafts} grafts, frame-stmts ${_offs} -> ${_ons})")
    return()
endif()
if(NOT _ontxt MATCHES "frame-compared=([0-9]+)")
    message(FATAL_ERROR "mcc-leaf-graft: no frame-compared line in the grafted run")
endif()
set(_onc "${CMAKE_MATCH_1}")
if(NOT _offtxt MATCHES "frame-compared=([0-9]+)")
    message(FATAL_ERROR "mcc-leaf-graft: no frame-compared line in the ungrafted run")
endif()
set(_offc "${CMAKE_MATCH_1}")
if(NOT _onc GREATER _offc)
    message(FATAL_ERROR "mcc-leaf-graft: the graft did not put a new frame run "
                        "on the device (frame-compared ${_offc} -> ${_onc})")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_on}" --quiet --no-inline --mutate
                OUTPUT_VARIABLE _mut ERROR_VARIABLE _mut)
message("${_mut}")
if(NOT _mut MATCHES "frame-mismatches=([1-9][0-9]*)")
    message(FATAL_ERROR "mcc-leaf-graft: every frame kernel was perturbed and "
                        "the frame differential still reported clean, so the "
                        "compiler's graft is not actually being compared")
endif()
execute_process(COMMAND "${RUNNER}" --arenas "${_off}" --quiet --no-inline --mutate
                OUTPUT_VARIABLE _bmut ERROR_VARIABLE _bmut)
message("${_bmut}")
if(NOT _bmut MATCHES "frame-mismatches=0")
    message(FATAL_ERROR "mcc-leaf-graft: the ungrafted arm already reddens on "
                        "mutation, so the mutation above is not attributable to "
                        "the compiler's graft")
endif()

message("mcc-leaf-graft: ${_grafts} grafts in mcc, frame-stmts ${_offs} -> ${_ons}, "
        "frame-compared ${_offc} -> ${_onc}, mutation detected, blind without it")
