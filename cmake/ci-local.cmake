# cmake/ci-local.cmake — local reproduction of the full CI + release matrix.
#
# Driven by the `test` target (see CMakeLists.txt, option MCC_LOCAL_CI_AS_TEST)
# and run with `cmake -P`. It probes this machine for every toolchain and
# emulator the CI/release workflows can use, then runs each preset the host is
# actually able to run:
#
#   native   — linux-gcc / linux-clang / macos / msvc         (host compiler)
#   cross    — *-cross presets (host-runnable cross compilers, incl. win32)
#   wine     — exercised automatically by the *-cross presets' pe-wine-conformance
#   qemu     — qemu-<arch> presets, one per qemu-user binary found
#   mingw    — mingw / dist-mingw (Windows host) — reported elsewhere
#   release  — dist-* presets: configure+build+install+bench+package-dist
#
# The test matrix is run through the same `ci run-preset` host tool the CI
# workflow uses (configure -> build -> ctest), so a green run here means the
# same steps that run in .github/workflows/ci.yml passed locally.
#
# Structural inputs (-D, supplied by the target):
#   CI_TOOL       path to the built `ci` host tool (tools/ci.c)
#   SRCDIR        repo root (working dir for every sub-build)
#   DIST_VERSION  version tag stamped into the release bundles
#   DIST_DIR      dist/ output directory
#   HOST_CPU      MCC_CPU of the configuring build (x86_64/arm64/i386/arm/riscv64)
#
# Runtime knobs (environment variables — no reconfigure needed):
#   LOCAL_CI_ONLY=<regex>   only run presets whose name matches the regex
#   LOCAL_CI_SKIP_QEMU=1    skip the qemu-user matrix (slow; downloads rootfs)
#   LOCAL_CI_SKIP_RELEASE=1 skip the dist/release bundles
#   LOCAL_CI_LIST=1         print the resolved plan and exit without running
#   LOCAL_CI_KEEP_GOING=0   stop at the first failure (default: run all, as CI
#                           does with fail-fast:false)

cmake_minimum_required(VERSION 3.22)

if(NOT CI_TOOL OR NOT SRCDIR)
    message(FATAL_ERROR "ci-local: CI_TOOL and SRCDIR are required (-D)")
endif()
if(NOT DIST_DIR)
    set(DIST_DIR "${SRCDIR}/dist")
endif()
if(NOT DIST_VERSION)
    set(DIST_VERSION "v0.0.0-local")
endif()

# Runtime knobs come from the environment so the same configured target can be
# scoped ad hoc (e.g. `LOCAL_CI_ONLY=qemu ninja test`).
macro(_env_default var default)
    if(DEFINED ENV{${var}})
        set(${var} "$ENV{${var}}")
    else()
        set(${var} "${default}")
    endif()
endmacro()
_env_default(LOCAL_CI_ONLY "")
_env_default(LOCAL_CI_SKIP_QEMU "")
_env_default(LOCAL_CI_SKIP_RELEASE "")
_env_default(LOCAL_CI_LIST "")
_env_default(LOCAL_CI_KEEP_GOING "1")

set(_os "${CMAKE_HOST_SYSTEM_NAME}")            # Linux / Darwin / Windows
if(NOT HOST_CPU)
    execute_process(COMMAND uname -m
        OUTPUT_VARIABLE HOST_CPU OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

# ---------------------------------------------------------------------------
# 1. Probe the host.
# ---------------------------------------------------------------------------
find_program(_GCC   gcc)
find_program(_CLANG clang)
find_program(_CL    cl)                           # MSVC
find_program(_WINE  NAMES wine wine64)
find_program(_MINGW NAMES x86_64-w64-mingw32-gcc) # POSIX mingw cross gcc
find_program(_DOCKER docker)

set(_qemu_archs x86_64 i386 arm arm64 riscv64)
set(_qbin_x86_64 qemu-x86_64)
set(_qbin_i386   qemu-i386)
set(_qbin_arm    qemu-arm)
set(_qbin_arm64  qemu-aarch64)
set(_qbin_riscv64 qemu-riscv64)
set(_qemu_found "")
foreach(_a IN LISTS _qemu_archs)
    find_program(_Q_${_a} ${_qbin_${_a}})
    if(_Q_${_a})
        list(APPEND _qemu_found "${_a}")
    endif()
endforeach()

set(_cc_any OFF)
if(_GCC OR _CLANG)
    set(_cc_any ON)
endif()

# ---------------------------------------------------------------------------
# 2. Build the plan (host-faithful: exactly the CI jobs this OS runs).
#    Each entry is "preset|cc" — cc overrides $env{CC} for presets that need it
#    (empty otherwise). Skips are recorded with a reason for the report.
# ---------------------------------------------------------------------------
set(_plan "")     # test-matrix presets   ("preset|cc")
set(_dist "")     # release bundles       ("preset|cc|plat")
set(_skips "")    # "preset — reason"

macro(_test_job preset gate why)
    if(${gate})
        list(APPEND _plan "${preset}|")
    else()
        list(APPEND _skips "${preset} — ${why}")
    endif()
endmacro()

if(_os STREQUAL "Linux")
    foreach(_p linux-gcc linux-gcc-cross linux-gcc-release linux-gcc-musl
               linux-gcc-static linux-gcc-multisource linux-gcc-asm-off
               linux-gcc-predefs-off linux-gcc-pie linux-gcc-dwarf
               linux-gcc-diagnostics linux-gcc-sanitize)
        _test_job("${_p}" _GCC "gcc not found")
    endforeach()
    foreach(_p linux-clang linux-clang-cross linux-clang-release)
        _test_job("${_p}" _CLANG "clang not found")
    endforeach()
elseif(_os STREQUAL "Darwin")
    # The macos presets take the compiler from $env{CC}; run every (preset x cc)
    # combination the host has, mirroring the CI macos matrix.
    foreach(_p macos macos-cross)
        if(_CLANG)
            list(APPEND _plan "${_p}|clang")
        endif()
        if(_GCC)
            list(APPEND _plan "${_p}|gcc")
        endif()
        if(NOT _cc_any)
            list(APPEND _skips "${_p} — no C compiler (need clang or gcc)")
        endif()
    endforeach()
elseif(_os STREQUAL "Windows")
    _test_job("msvc"  _CL    "cl (MSVC) not found — run from a VS dev shell")
    # mingw preset fetches its own winlibs GCC via the superbuild; it only needs
    # ninja + a VS dev environment for the host tools, so always attempt it.
    list(APPEND _plan "mingw|")
else()
    message(WARNING "ci-local: unrecognised host '${_os}'; only qemu jobs (if any) will run")
endif()

# qemu-user matrix — one preset per emulator binary present. Runs wherever the
# user-mode qemu binaries exist (Linux); silently absent elsewhere.
if(NOT LOCAL_CI_SKIP_QEMU)
    foreach(_a IN LISTS _qemu_archs)
        if(_Q_${_a})
            list(APPEND _plan "qemu-${_a}|")
        else()
            list(APPEND _skips "qemu-${_a} — ${_qbin_${_a}} not found (install qemu-user)")
        endif()
    endforeach()
else()
    list(APPEND _skips "qemu-* — skipped (LOCAL_CI_SKIP_QEMU)")
endif()

# release / dist bundles — the dist-* presets this OS ships in release.yml.
if(NOT LOCAL_CI_SKIP_RELEASE)
    if(_os STREQUAL "Linux")
        if(_GCC)
            list(APPEND _dist "dist-linux-gcc||linux-${HOST_CPU}-gcc")
        endif()
        if(_CLANG)
            list(APPEND _dist "dist-linux-clang||linux-${HOST_CPU}-clang")
        endif()
    elseif(_os STREQUAL "Darwin")
        list(APPEND _dist "dist-macos|clang|macos-${HOST_CPU}-clang")
    elseif(_os STREQUAL "Windows")
        if(_CL)
            list(APPEND _dist "dist-msvc||windows-${HOST_CPU}-msvc")
        endif()
        list(APPEND _dist "dist-mingw||windows-${HOST_CPU}-mingw")
    endif()
else()
    list(APPEND _skips "dist-* — skipped (LOCAL_CI_SKIP_RELEASE)")
endif()

# ---------------------------------------------------------------------------
# 3. Apply the optional name filter.
# ---------------------------------------------------------------------------
if(NOT LOCAL_CI_ONLY STREQUAL "")
    set(_f "")
    foreach(_j IN LISTS _plan)
        string(REGEX REPLACE "\\|.*" "" _pn "${_j}")
        if(_pn MATCHES "${LOCAL_CI_ONLY}")
            list(APPEND _f "${_j}")
        endif()
    endforeach()
    set(_plan "${_f}")
    set(_f "")
    foreach(_j IN LISTS _dist)
        string(REGEX REPLACE "\\|.*" "" _pn "${_j}")
        if(_pn MATCHES "${LOCAL_CI_ONLY}")
            list(APPEND _f "${_j}")
        endif()
    endforeach()
    set(_dist "${_f}")
endif()

# ---------------------------------------------------------------------------
# 4. Report the probe + plan.
# ---------------------------------------------------------------------------
list(LENGTH _plan _n_test)
list(LENGTH _dist _n_dist)
list(LENGTH _qemu_found _n_qemu)
string(REPLACE ";" " " _qemu_str "${_qemu_found}")
macro(_probe_line label var extra)
    if(${var})
        set(_v "yes (${${var}})")
    else()
        set(_v "no")
    endif()
    message("  ${label}: ${_v}${extra}")
endmacro()
message("")
message("==================== local CI: host probe ====================")
message("  host            : ${_os} / ${HOST_CPU}")
_probe_line("gcc            " _GCC "")
_probe_line("clang          " _CLANG "")
_probe_line("msvc (cl)      " _CL "")
_probe_line("wine           " _WINE "   (drives pe-wine-conformance in *-cross)")
_probe_line("mingw cross gcc" _MINGW "")
_probe_line("docker         " _DOCKER "")
message("  qemu-user       : ${_n_qemu} arch(es): ${_qemu_str}")
message("  --------------------------------------------------------")
message("  test presets    : ${_n_test}")
foreach(_j IN LISTS _plan)
    string(REGEX REPLACE "\\|$" "" _disp "${_j}")
    string(REPLACE "|" " CC=" _disp "${_disp}")
    message("      run   ${_disp}")
endforeach()
message("  release bundles : ${_n_dist}")
foreach(_j IN LISTS _dist)
    string(REGEX REPLACE "\\|.*" "" _pn "${_j}")
    message("      dist  ${_pn}")
endforeach()
list(LENGTH _skips _n_skip)
message("  skipped         : ${_n_skip}")
foreach(_s IN LISTS _skips)
    message("      skip  ${_s}")
endforeach()
message("==============================================================")
message("")

if(LOCAL_CI_LIST)
    message("LOCAL_CI_LIST set — plan only, nothing run.")
    return()
endif()

# ---------------------------------------------------------------------------
# 5. Run it.  Collect results; keep going past failures unless told not to
#    (CI runs the whole matrix with fail-fast:false).
# ---------------------------------------------------------------------------
set(_results "")
set(_failed "")
set(_stop OFF)

function(_env_wrap out cc)
    # Build a command prefix that overrides CC for presets that read $env{CC}.
    if(cc STREQUAL "")
        set(${out} "" PARENT_SCOPE)
    else()
        set(${out} "${CMAKE_COMMAND};-E;env;CC=${cc}" PARENT_SCOPE)
    endif()
endfunction()

foreach(_j IN LISTS _plan)
    if(_stop)
        break()
    endif()
    string(REGEX REPLACE "\\|.*" "" _preset "${_j}")
    string(REGEX REPLACE "^[^|]*\\|" "" _cc "${_j}")
    _env_wrap(_pre "${_cc}")
    set(_label "${_preset}")
    if(NOT _cc STREQUAL "")
        set(_label "${_preset} (CC=${_cc})")
    endif()
    message("")
    message(">>>> [test] ${_label}")
    execute_process(
        COMMAND ${_pre} "${CI_TOOL}" run-preset "${_preset}"
        WORKING_DIRECTORY "${SRCDIR}"
        RESULT_VARIABLE _rc)
    if(_rc EQUAL 0)
        list(APPEND _results "PASS  test ${_label}")
    else()
        list(APPEND _results "FAIL  test ${_label} (exit ${_rc})")
        list(APPEND _failed "${_label}")
        if(NOT LOCAL_CI_KEEP_GOING)
            set(_stop ON)
        endif()
    endif()
endforeach()

foreach(_j IN LISTS _dist)
    if(_stop)
        break()
    endif()
    string(REPLACE "|" ";" _parts "${_j}")
    list(GET _parts 0 _preset)
    list(GET _parts 1 _cc)
    list(GET _parts 2 _plat)
    _env_wrap(_pre "${_cc}")
    set(_bdir "${SRCDIR}/cmake-${_preset}")
    set(_cfg "")
    if(_preset MATCHES "msvc")
        set(_cfg --config Release)   # multi-config generator needs it at install
    endif()
    message("")
    message(">>>> [dist] ${_preset} -> ${_plat}")
    set(_drc 0)
    # configure -> build -> install -> bench -> package-dist  (== release.yml)
    execute_process(COMMAND ${_pre} "${CMAKE_COMMAND}" --preset "${_preset}"
            "-DMCC_DIST_VERSION=${DIST_VERSION}" "-DMCC_DIST_PLAT=${_plat}" -DMCC_BENCH=ON
        WORKING_DIRECTORY "${SRCDIR}" RESULT_VARIABLE _r)
    if(NOT _r EQUAL 0)
        set(_drc ${_r})
    endif()
    if(_drc EQUAL 0)
        execute_process(COMMAND "${CMAKE_COMMAND}" --build --preset "${_preset}" -j
            WORKING_DIRECTORY "${SRCDIR}" RESULT_VARIABLE _r)
        if(NOT _r EQUAL 0)
            set(_drc ${_r})
        endif()
    endif()
    if(_drc EQUAL 0)
        execute_process(COMMAND "${CMAKE_COMMAND}" --install "${_bdir}" ${_cfg}
            WORKING_DIRECTORY "${SRCDIR}" RESULT_VARIABLE _r)
        if(NOT _r EQUAL 0)
            set(_drc ${_r})
        endif()
    endif()
    if(_drc EQUAL 0)
        execute_process(COMMAND "${CMAKE_COMMAND}" --build --preset "${_preset}" --target bench
            WORKING_DIRECTORY "${SRCDIR}" RESULT_VARIABLE _r)
        if(NOT _r EQUAL 0)
            set(_drc ${_r})
        endif()
    endif()
    if(_drc EQUAL 0)
        execute_process(COMMAND "${CMAKE_COMMAND}" --build --preset "${_preset}" --target package-dist
            WORKING_DIRECTORY "${SRCDIR}" RESULT_VARIABLE _r)
        if(NOT _r EQUAL 0)
            set(_drc ${_r})
        endif()
    endif()
    if(_drc EQUAL 0)
        list(APPEND _results "PASS  dist ${_preset} -> ${_plat}")
    else()
        list(APPEND _results "FAIL  dist ${_preset} -> ${_plat} (exit ${_drc})")
        list(APPEND _failed "dist:${_preset}")
        if(NOT LOCAL_CI_KEEP_GOING)
            set(_stop ON)
        endif()
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 6. Summary + exit status.
# ---------------------------------------------------------------------------
message("")
message("==================== local CI: summary =======================")
foreach(_r IN LISTS _results)
    message("  ${_r}")
endforeach()
foreach(_s IN LISTS _skips)
    message("  SKIP  ${_s}")
endforeach()
message("==============================================================")
list(LENGTH _results _n_run)
list(LENGTH _failed _n_fail)
if(_stop)
    message("  (stopped early: LOCAL_CI_KEEP_GOING=0)")
endif()
if(_n_fail GREATER 0)
    string(REPLACE ";" ", " _fl "${_failed}")
    message(FATAL_ERROR "local CI: ${_n_fail}/${_n_run} job(s) FAILED: ${_fl}")
endif()
message("local CI: all ${_n_run} job(s) passed.")
