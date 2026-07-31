cmake_minimum_required(VERSION 3.22)
#
# Build a TARGET-NATIVE mcc with the in-tree cross compiler, so journal_sweep.cmake
# can run it under qemu-user or wine.
#
#   cmake -DKEY=arm64-linux-glibc -DROOT=<srcdir> -DXDIR=<crossdir> -DTMPROOT=<dir> \
#         -P tests/ast/journal_native.cmake
#
# Why this exists at all: the depth ceiling (fix=/deep=) is native-sensitive.
# Measured 2026-07-31, a cross run reads native-1 on arm64 and riscv64 and exactly
# native on i386, so a ceiling banked from a cross run is wrong on two of three
# targets. The honesty census is NOT native-sensitive -- cross and native produce
# byte-identical files -- so MODE=cross is fine for that axis alone.
#
foreach(_req KEY ROOT)
    if(NOT ${_req})
        message(FATAL_ERROR "journal_native: ${_req} is required")
    endif()
endforeach()
if(NOT XDIR)
    set(XDIR "${ROOT}/cmake-cross")
endif()
if(NOT TMPROOT)
    set(TMPROOT "${ROOT}/cmake-cross/journal-sweep")
endif()
file(MAKE_DIRECTORY "${TMPROOT}")

macro(_skip msg)
    message(STATUS "journal-native[${KEY}]: SKIP -- ${msg}")
    return()
endmacro()

set(_libc "")
set(_k "${KEY}")
foreach(_l glibc musl ucrt msvcrt)
    if(_k MATCHES "^(.+)-${_l}$")
        set(_libc "${_l}")
        set(_k "${CMAKE_MATCH_1}")
        break()
    endif()
endforeach()
if(NOT _k MATCHES "^([a-z0-9_]+)-([a-z0-9]+)$")
    message(FATAL_ERROR "journal_native: cannot parse KEY '${KEY}'")
endif()
set(_cpu "${CMAKE_MATCH_1}")
set(_os "${CMAKE_MATCH_2}")
set(_default_libc "glibc")
if(_os STREQUAL "win32")
    set(_default_libc "ucrt")
endif()
if(_libc STREQUAL "")
    set(_libc "${_default_libc}")
endif()
set(_bkey "${_cpu}-${_os}")
if(NOT _libc STREQUAL "${_default_libc}")
    set(_bkey "${_cpu}-${_os}-${_libc}")
endif()

if(NOT _cpu MATCHES "^(x86_64|arm64|i386|riscv64)$")
    _skip("cpu '${_cpu}' is outside the MCC_JOURNAL_HOOKS gate")
endif()

set(_triple "${_cpu}")
if(_os STREQUAL "win32")
    set(_triple "${_cpu}-win32")
elseif(NOT _os STREQUAL "linux")
    _skip("no native path for os '${_os}'")
endif()

set(_mcc "${XDIR}/mcc-${_triple}")
if(NOT _libc STREQUAL "${_default_libc}" AND _os STREQUAL "linux")
    set(_mcc "${XDIR}/mcc-${_triple}-${_libc}")
endif()
if(NOT EXISTS "${_mcc}")
    _skip("no cross compiler at ${_mcc}")
endif()

set(_out "${TMPROOT}/native-${_bkey}")
file(MAKE_DIRECTORY "${_out}")

# The arch source dir the backend needs on the include path. x86_64 reuses
# i386-tok.h, which is why it lists both.
set(_archinc "-I${ROOT}/src/arch/${_cpu}")
if(_cpu STREQUAL "x86_64")
    set(_archinc "-I${ROOT}/src/arch/x86_64" "-I${ROOT}/src/arch/i386")
endif()
# Project includes MUST precede any sysroot include, or the system elf.h shadows
# src/formats/elf.h and the build dies on R_RISCV_SET_ULEB128.
set(_inc "-I${ROOT}/src" "-I${ROOT}/src/formats" "-I${ROOT}/src/objfmt"
         ${_archinc} "-I${ROOT}/include")

if(_os STREQUAL "linux")
    string(TOUPPER "${_cpu}" _CPUU)
    set(_def "-DMCC_CONFIG_OPTIMIZER=1" "-DMCC_TARGET_${_CPUU}")
    set(_sysroot "${ROOT}/vendor/gentoo-stage3-${_cpu}-${_libc}")
    if(NOT IS_DIRECTORY "${_sysroot}")
        _skip("no ${_libc} sysroot at ${_sysroot}")
    endif()
    # crt1.o/crti.o live in usr/lib64 on the 64-bit stage3s; without these -L the
    # link fails with "file 'crt1.o' not found".
    set(_args "-B${XDIR}" "--sysroot=${_sysroot}"
              "-L${_sysroot}/usr/lib64" "-L${_sysroot}/lib64"
              "-L${_sysroot}/usr/lib" "-L${_sysroot}/lib"
              "-I${ROOT}/runtime/include" ${_inc} "-I${_sysroot}/usr/include" ${_def})
    set(_exe "${_out}/mcc")
else()
    set(_mingw "i686")
    if(_cpu STREQUAL "x86_64")
        set(_mingw "x86_64")
    endif()
    set(_mdir "${ROOT}/vendor/winlibs-mingw-w64-16.1.0-ucrt-${_mingw}")
    if(NOT IS_DIRECTORY "${_mdir}")
        _skip("no ucrt PE runtime at ${_mdir}")
    endif()
    set(_sub "mingw32")
    set(_trip "i686-w64-mingw32")
    if(_cpu STREQUAL "x86_64")
        set(_sub "mingw64")
        set(_trip "x86_64-w64-mingw32")
    endif()
    file(GLOB_RECURSE _libgcc "${_mdir}/${_sub}/lib/gcc/${_trip}/*/libgcc.a")
    set(_lgcc "")
    if(_libgcc)
        list(GET _libgcc 0 _lg)
        get_filename_component(_lgcc "${_lg}" DIRECTORY)
    endif()
    string(TOUPPER "${_cpu}" _CPUU)
    set(_def "-DCC_NAME=CC_clang" "-DMCC_CONFIG_CROSSPREFIX=\"${_triple}-\""
             "-DMCC_CONFIG_OPTIMIZER=1" "-DMCC_CONFIG_PREDEFS=1"
             "-DMCC_TARGET_${_CPUU}" "-DMCC_TARGET_PE")
    set(_args "-B${ROOT}/runtime/win32" "-I${ROOT}/runtime/include"
              "-I${XDIR}" ${_inc} "-I${ROOT}" ${_def}
              "-L${XDIR}" "-L${_mdir}/${_sub}/${_trip}/lib")
    if(_lgcc)
        list(APPEND _args "-L${_lgcc}")
    endif()
    set(_exe "${_out}/mcc.exe")
endif()

message(STATUS "journal-native[${KEY}]: compiling src/mcc.c with ${_mcc}")
execute_process(COMMAND "${_mcc}" -w ${_args} -c -O2 "${ROOT}/src/mcc.c" -o "${_out}/mcc.o"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "journal-native[${KEY}]: compile failed (rc=${_rc})")
endif()
message(STATUS "journal-native[${KEY}]: linking ${_exe}")
execute_process(COMMAND "${_mcc}" -w ${_args} "${_out}/mcc.o" -o "${_exe}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "journal-native[${KEY}]: link failed (rc=${_rc})")
endif()
message(STATUS "journal-native[${KEY}]: built ${_exe}")
