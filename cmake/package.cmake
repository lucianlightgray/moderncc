# package.cmake — assemble release bundles from a staged install (stage/).
#
# Portable, single-source replacement for the old package.sh + package.ps1.
# Run in CMake script mode after `cmake --install`:
#
#   cmake -DVER=<version> -DPLAT=<platform> [-DSTAGE=<dir>] [-DOUT=<dir>] \
#         [-DFORMAT=tgz|zip] -P cmake/package.cmake
#
# VER may carry a leading 'v' (stripped). FORMAT defaults to zip on a Windows
# host, tar.gz elsewhere. Produces, in OUT (default ./out):
#   mcc-<ver>-<plat>.<ext>        host compilers (mcc, mcc-static, mcc-dynamic,
#                                 + -musl siblings) + the mcc runtime dir
#   libmcc-<ver>-<plat>.<ext>     embed API: headers, libmcc*.{a,so,…}, cmake pkg
#   mcc-cross-<ver>-<plat>.<ext>  the mcc-<arch> cross compilers + runtime archives
#   checksums-<plat>.txt          sha256sum-format lines over the archives
#
# Binary/library names follow the suffix convention in BUILD.md.

cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED VER OR NOT DEFINED PLAT)
    message(FATAL_ERROR "package.cmake: pass -DVER=<version> and -DPLAT=<platform>")
endif()
string(REGEX REPLACE "^v" "" ver "${VER}")

# In script mode CMAKE_SOURCE_DIR is the current working directory.
if(NOT DEFINED STAGE)
    set(STAGE "${CMAKE_SOURCE_DIR}/stage")
endif()
if(NOT DEFINED OUT)
    set(OUT "${CMAKE_SOURCE_DIR}/out")
endif()
set(pkg "${CMAKE_SOURCE_DIR}/pkg")

# Archive format: zip on Windows, tar.gz elsewhere (overridable via -DFORMAT).
if(NOT DEFINED FORMAT)
    if(CMAKE_HOST_WIN32)
        set(FORMAT zip)
    else()
        set(FORMAT tgz)
    endif()
endif()
if(FORMAT STREQUAL "zip")
    set(_ext "zip")
else()
    set(_ext "tar.gz")
endif()

# Executable suffix + host compiler shapes (non-cross). This list also
# partitions bin/: the cross bundle is bin/mcc-* minus these.
if(CMAKE_HOST_WIN32)
    set(_x ".exe")
else()
    set(_x "")
endif()
set(host_exes mcc mcc-static mcc-dynamic mcc-musl mcc-static-musl mcc-dynamic-musl)

# libdir: lib64 (multilib GNU installs) else lib.
if(EXISTS "${STAGE}/lib64")
    set(libdir "lib64")
else()
    set(libdir "lib")
endif()

if(NOT EXISTS "${STAGE}/bin/mcc${_x}")
    message(FATAL_ERROR "package.cmake: no staged install at '${STAGE}' (run cmake --install first)")
endif()

file(REMOVE_RECURSE "${pkg}" "${OUT}")
file(MAKE_DIRECTORY "${pkg}" "${OUT}")

set(_bin_perms OWNER_READ OWNER_WRITE OWNER_EXECUTE
               GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(_names "")

# Archive ${pkg}/${d} -> ${OUT}/${d}.${_ext} with ${d} as the top-level dir.
macro(_archive d)
    if(_ext STREQUAL "zip")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUT}/${d}.${_ext}" --format=zip "${d}"
            WORKING_DIRECTORY "${pkg}" RESULT_VARIABLE _rc)
    else()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar czf "${OUT}/${d}.${_ext}" "${d}"
            WORKING_DIRECTORY "${pkg}" RESULT_VARIABLE _rc)
    endif()
    if(_rc)
        message(FATAL_ERROR "package.cmake: archiving ${d} failed (${_rc})")
    endif()
    list(APPEND _names "${d}.${_ext}")
endmacro()


# --- mcc: host compiler binaries + the mcc runtime dir (libmcc1.a + headers) ---
set(d "mcc-${ver}-${PLAT}")
file(MAKE_DIRECTORY "${pkg}/${d}/bin" "${pkg}/${d}/lib")
foreach(b IN LISTS host_exes)
    if(EXISTS "${STAGE}/bin/${b}${_x}")
        file(COPY "${STAGE}/bin/${b}${_x}"
             DESTINATION "${pkg}/${d}/bin" FILE_PERMISSIONS ${_bin_perms})
    endif()
endforeach()
# On Windows the shared libmcc lives in bin/; mcc-dynamic.exe links it, so ship
# the runtime DLL(s) next to the executables (no rpath on Windows).
if(CMAKE_HOST_WIN32)
    file(GLOB _dlls "${STAGE}/bin/libmcc*.dll")
    foreach(f IN LISTS _dlls)
        file(COPY "${f}" DESTINATION "${pkg}/${d}/bin")
    endforeach()
endif()
if(EXISTS "${STAGE}/${libdir}/mcc")
    file(COPY "${STAGE}/${libdir}/mcc" DESTINATION "${pkg}/${d}/lib")
endif()
_archive("${d}")


# --- libmcc: embed API — headers + libmcc archives/libs + cmake package ---
set(d "libmcc-${ver}-${PLAT}")
file(MAKE_DIRECTORY "${pkg}/${d}/lib")
if(EXISTS "${STAGE}/include")
    file(COPY "${STAGE}/include" DESTINATION "${pkg}/${d}")
endif()
file(GLOB _libs
    "${STAGE}/${libdir}/libmcc*.a" "${STAGE}/${libdir}/libmcc*.so"
    "${STAGE}/${libdir}/libmcc*.dylib" "${STAGE}/${libdir}/libmcc*.lib"
    "${STAGE}/bin/libmcc*.dll")
foreach(f IN LISTS _libs)
    file(COPY "${f}" DESTINATION "${pkg}/${d}/lib")
endforeach()
if(EXISTS "${STAGE}/${libdir}/cmake")
    file(COPY "${STAGE}/${libdir}/cmake" DESTINATION "${pkg}/${d}/lib")
endif()
if(EXISTS "${STAGE}/${libdir}/mcc")
    file(COPY "${STAGE}/${libdir}/mcc" DESTINATION "${pkg}/${d}/lib")
endif()
_archive("${d}")


# --- mcc-cross: the mcc-<arch> cross compilers + their runtime archives ---
set(d "mcc-cross-${ver}-${PLAT}")
file(MAKE_DIRECTORY "${pkg}/${d}/bin" "${pkg}/${d}/lib/mcc")
file(GLOB _crossexes "${STAGE}/bin/mcc-*")
set(_found FALSE)
foreach(f IN LISTS _crossexes)
    get_filename_component(_n "${f}" NAME)
    string(REGEX REPLACE "\\.exe$" "" _base "${_n}")
    list(FIND host_exes "${_base}" _idx)
    if(_idx GREATER -1)
        continue()   # host shape (mcc-static, mcc-musl, …), not a cross compiler
    endif()
    file(COPY "${f}" DESTINATION "${pkg}/${d}/bin" FILE_PERMISSIONS ${_bin_perms})
    set(_found TRUE)
endforeach()
file(GLOB _rtarch "${STAGE}/${libdir}/mcc/*-libmcc1.a")
foreach(f IN LISTS _rtarch)
    file(COPY "${f}" DESTINATION "${pkg}/${d}/lib/mcc")
endforeach()
if(_found)
    _archive("${d}")
else()
    message(STATUS "package.cmake: no cross compilers in stage/bin; skipping cross bundle")
endif()


# --- checksums (sha256sum format: "<hash>  <filename>") ---
set(_sums "")
foreach(n IN LISTS _names)
    file(SHA256 "${OUT}/${n}" _h)
    string(APPEND _sums "${_h}  ${n}\n")
endforeach()
file(WRITE "${OUT}/checksums-${PLAT}.txt" "${_sums}")

message(STATUS "== packaged (${_ext}) ==")
foreach(n IN LISTS _names)
    message(STATUS "  ${n}")
endforeach()
