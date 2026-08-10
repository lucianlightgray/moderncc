# Real libc, not a hand-written device one.
#
# The device libc phase does not have to mean writing memcpy in SPIR-V by hand.
# musl is clean portable C, mcc compiles C, and the frame runner lowers arenas —
# so a device libc is the existing lowering applied to musl's own source, and
# every function it covers is one nobody had to reimplement or re-verify.
#
# This cell is the ratchet on that. It compiles musl/src/string with mcc, runs
# every lowerable slice and frame run through both executors, and requires them
# to agree. The mutation arm proves the comparison is not blind.
#
# The pointer-walking functions (memcmp, strcmp, strncmp, memchr) used to produce
# zero frame runs. They now lower: a frame slot holding a host address inside
# binding 2 is dereferenced by both executors over the same physical bytes. The
# frame-mem tooth below is what keeps that honest -- a run that lowers `*p` but
# dereferences nothing would still be counted by frame-compared.

set(_dump "${BINDIR}/slicerun-musl.txt")
file(REMOVE "${_dump}")

set(_src "${SRCDIR}/vendor/musl-src/src/string")
if(NOT EXISTS "${_src}")
    message("slice/musl: vendor/musl-src absent, skipping")
    cmake_language(EXIT 77)
endif()
if(NOT EXISTS "${SRCDIR}/vendor/musl-sysroot/include/bits/alltypes.h")
    message("slice/musl: vendor/musl-sysroot has no generated headers, skipping")
    cmake_language(EXIT 77)
endif()

set(_inc
    -I${SRCDIR}/vendor/musl-src/arch/x86_64
    -I${SRCDIR}/vendor/musl-src/arch/generic
    -I${SRCDIR}/vendor/musl-src/src/include
    -I${SRCDIR}/vendor/musl-src/src/internal
    -I${SRCDIR}/vendor/musl-sysroot/include)

file(GLOB _srcs "${_src}/*.c")
list(SORT _srcs)
list(LENGTH _srcs _ntu)
set(_ok 0)
set(_bad "")
foreach(_f IN LISTS _srcs)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "MCC_ARENA_DUMP=${_dump}"
                "${MCC}" -w -c "${_f}" -o "${BINDIR}/slicerun-musl.o" -O1 ${_inc}
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    if(_rc EQUAL 0)
        math(EXPR _ok "${_ok} + 1")
    else()
        get_filename_component(_bn "${_f}" NAME)
        list(APPEND _bad "${_bn}")
    endif()
endforeach()

message("slice/musl: ${_ok} of ${_ntu} musl string TUs compiled by mcc")
if(_ntu LESS 74)
    message(FATAL_ERROR "slice/musl: the corpus is only ${_ntu} TUs; musl 1.2.5's "
                        "src/string has 74. A shrunken corpus would make the "
                        "differential below pass vacuously")
endif()
if(NOT _ok EQUAL _ntu)
    message(FATAL_ERROR "slice/musl: ${_ok} of ${_ntu} musl TUs compiled; mcc "
                        "compiles all of musl/src/string, so any refusal is a front "
                        "end regression or a wrong include order -- musl's internal "
                        "src/include must precede the installed sysroot headers, or "
                        "weak_alias/hidden go undefined. Refused: ${_bad}")
endif()
if(NOT EXISTS "${_dump}")
    message(FATAL_ERROR "slice/musl: no arenas dumped; MCC_ARENA_DUMP is not "
                        "firing and this cell would measure nothing")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
message("${_out}")
if(_clean EQUAL 77)
    if(MCC_GPU_REQUIRED)
        message(FATAL_ERROR "slice/musl: no usable device, but MCC_GPU_REQUIRED is set")
    endif()
    message("slice/musl: no usable device, skipping the device half")
    cmake_language(EXIT 77)
endif()
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "slice/musl: musl slices disagree between the CPU and "
                        "device runners")
endif()
if(NOT _out MATCHES "slices=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/musl: zero musl slices lowered; a clean result "
                        "here would mean nothing ran")
endif()
if(_out MATCHES "available=1" AND NOT _out MATCHES "frame-compared=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/musl: no musl frame run was compared on the "
                        "device; accepted counts runs that were never built")
endif()
if(_out MATCHES "available=1" AND NOT _out MATCHES "frame-mem=([1-9][0-9]*)")
    message(FATAL_ERROR "slice/musl: no musl frame run dereferenced the shared "
                        "address space, so the byte-for-byte comparison of "
                        "binding 2 compared nothing")
endif()

execute_process(COMMAND "${RUNNER}" --arenas "${_dump}" --quiet --mutate
                RESULT_VARIABLE _mut OUTPUT_VARIABLE _mout ERROR_VARIABLE _mout)
if(_mut EQUAL 0)
    message(FATAL_ERROR "slice/musl: every kernel was perturbed and the "
                        "differential still reported clean, so it is blind")
endif()
message("slice/musl: clean OK, mutation detected")
