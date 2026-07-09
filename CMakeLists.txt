cmake_minimum_required(VERSION 3.22)

set(MCC_VERSION "20260707094756")
set(MCC_SOVERSION "1.0.0")
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" MCC_VERSION_NUMERIC "${MCC_SOVERSION}")

if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET
   AND NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
    set(_mcc_sel_cc "${CMAKE_C_COMPILER}")
    if(NOT _mcc_sel_cc)
        set(_mcc_sel_cc "$ENV{CC}")
    endif()
    if(_mcc_sel_cc MATCHES "[gG][cC][cC]")
        execute_process(COMMAND "${_mcc_sel_cc}" --version
            OUTPUT_VARIABLE _mcc_cc_ver ERROR_QUIET RESULT_VARIABLE _mcc_cc_rc)
        if(_mcc_cc_rc EQUAL 0 AND _mcc_cc_ver MATCHES "Free Software Foundation")
            execute_process(COMMAND sw_vers -productVersion
                OUTPUT_VARIABLE _mcc_osver OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET RESULT_VARIABLE _mcc_osrc)
            if(NOT _mcc_osrc EQUAL 0 OR NOT _mcc_osver MATCHES "^[0-9]+")
                set(_mcc_osver "11.0")   
            endif()
            set(CMAKE_OSX_DEPLOYMENT_TARGET "${_mcc_osver}" CACHE STRING
                "macOS deployment target (pinned for a Homebrew GNU gcc host build)")
            message(STATUS "mcc: GNU gcc on macOS -> pinned CMAKE_OSX_DEPLOYMENT_TARGET="
                           "${_mcc_osver} (avoids the stale 10.6 default that breaks gcc's linker spec)")
        endif()
    endif()
endif()

project(mcc
    VERSION "${MCC_VERSION_NUMERIC}"
    LANGUAGES C ASM
    DESCRIPTION "ModernCC"
    HOMEPAGE_URL "https://github.com/lucianlightgray/moderncc.git")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()

set(MCC_CONFIG_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/config-extra.cmake"
    CACHE FILEPATH "Optional extra CMake config, included before options")
if(EXISTS "${MCC_CONFIG_EXTRA}")
    include("${MCC_CONFIG_EXTRA}")
    message(STATUS "mcc: included ${MCC_CONFIG_EXTRA}")
endif()

function(mcc_config_node name)
    cmake_parse_arguments(PARSE_ARGV 1 N "EMPTY_OK;ADVANCED"
        "TYPE;DEFAULT;HELP;GROUP;VISIBLE_WHEN" "CHOICES")

    set(_def "${N_DEFAULT}")
    if(DEFINED MCC_SEED_${name})
        set(_def "${MCC_SEED_${name}}")
    endif()

    if(N_TYPE STREQUAL "BOOL")
        option(${name} "${N_HELP}" "${_def}")
    else()
        set(${name} "${_def}" CACHE "${N_TYPE}" "${N_HELP}")
    endif()

    if(N_CHOICES OR N_EMPTY_OK)
        if(N_EMPTY_OK)
            set_property(CACHE ${name} PROPERTY STRINGS "" ${N_CHOICES})
        else()
            set_property(CACHE ${name} PROPERTY STRINGS ${N_CHOICES})
        endif()
    endif()
    if(N_ADVANCED)
        mark_as_advanced(${name})
    endif()

    set_property(GLOBAL APPEND PROPERTY MCC_NODES "${name}")
    set_property(GLOBAL PROPERTY MCC_NODE_${name}_DEFAULT "${N_DEFAULT}")
    set_property(GLOBAL PROPERTY MCC_NODE_${name}_TYPE    "${N_TYPE}")
    set_property(GLOBAL PROPERTY MCC_NODE_${name}_GROUP   "${N_GROUP}")
    set_property(GLOBAL PROPERTY MCC_NODE_${name}_HELP    "${N_HELP}")
    set_property(GLOBAL PROPERTY MCC_NODE_${name}_CHOICES "${N_CHOICES}")
    set_property(GLOBAL PROPERTY MCC_NODE_${name}_EMPTYOK "${N_EMPTY_OK}")
    set_property(GLOBAL PROPERTY MCC_NODE_${name}_WHEN    "${N_VISIBLE_WHEN}")
endfunction()

function(mcc_apply_node_visibility)
    get_property(_nodes GLOBAL PROPERTY MCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_when GLOBAL PROPERTY MCC_NODE_${_n}_WHEN)
        get_property(_type GLOBAL PROPERTY MCC_NODE_${_n}_TYPE)
        set(_vis TRUE)
        if(NOT "${_when}" STREQUAL "")
            cmake_language(EVAL CODE "if(${_when})\nelse()\nset(_vis FALSE)\nendif()")
        endif()
        if(_vis)
            set_property(CACHE ${_n} PROPERTY TYPE "${_type}")
        else()
            set_property(CACHE ${_n} PROPERTY TYPE INTERNAL)
        endif()
    endforeach()
endfunction()

function(mcc_status_node name value)
    set_property(GLOBAL APPEND PROPERTY MCC_STATUS_NODES "${name}")
    set_property(GLOBAL PROPERTY MCC_STATUS_${name} "${value}")
endfunction()

function(mcc_snapshot_user_presets)
    if(DEFINED CACHE{_MCC_PRESET_VARS})
        return()
    endif()
    get_cmake_property(_cv CACHE_VARIABLES)
    set(_preset "")
    foreach(_v IN LISTS _cv)
        if(_v MATCHES "^MCC_" AND NOT _v MATCHES "^_MCC_")
            list(APPEND _preset "${_v}")
        endif()
    endforeach()
    set(_MCC_PRESET_VARS "${_preset}" CACHE INTERNAL
        "MCC_* cache vars set by the user (-D / config-extra), for superbuild forwarding")
endfunction()

macro(mcc_profile_seed var val)
    set(MCC_SEED_${var} "${val}")
endmacro()

function(mcc_map_toolchain_cc tc outvar)
    if(DEFINED MCC_CC_${tc})
        set(${outvar} "${MCC_CC_${tc}}" PARENT_SCOPE)
        return()
    endif()
    if(tc STREQUAL "auto")
        set(${outvar} "" PARENT_SCOPE)
        return()
    endif()
    if(tc STREQUAL "mingw")

        mcc_mingw_resolve()
        set(${outvar} "${_MCC_MINGW_X86_64_GCC}" PARENT_SCOPE)
        return()
    endif()
    if(tc STREQUAL "gcc")

        mcc_find_gnu_gcc(_gnu_gcc)
        if(_gnu_gcc)
            set(${outvar} "${_gnu_gcc}" PARENT_SCOPE)
            return()
        endif()
    endif()
    if(tc STREQUAL "clang")
        mcc_clang_resolve()
        if(_MCC_CLANG_EXE AND EXISTS "${_MCC_CLANG_EXE}")
            set(${outvar} "${_MCC_CLANG_EXE}" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(_names_auto "")
    set(_names_gcc gcc cc)
    set(_names_clang clang)
    set(_names_mcc mcc)
    set(_names_msvc cl)
    find_program(MCC_TOOLCHAIN_CC_${tc} NAMES ${_names_${tc}})
    if(MCC_TOOLCHAIN_CC_${tc})
        set(${outvar} "${MCC_TOOLCHAIN_CC_${tc}}" PARENT_SCOPE)
    else()
        set(${outvar} "" PARENT_SCOPE)
    endif()
endfunction()

function(mcc_find_gnu_gcc outvar)
    if(MCC_GNU_GCC)
        set(${outvar} "${MCC_GNU_GCC}" PARENT_SCOPE)
        return()
    endif()
    find_program(MCC_VENDOR_GCC NAMES gcc cc
        PATHS "${MCC_VENDOR_DIR}/gnu-gcc/bin" NO_DEFAULT_PATH)
    if(MCC_VENDOR_GCC)
        set(${outvar} "${MCC_VENDOR_GCC}" PARENT_SCOPE)
        return()
    endif()
    foreach(_n gcc-16 gcc-15 gcc-14 gcc-13 gcc-12 gcc-11 gcc-10 gcc cc)
        find_program(MCC_GNUGCC_${_n} NAMES ${_n})
        if(MCC_GNUGCC_${_n})
            execute_process(COMMAND "${MCC_GNUGCC_${_n}}" --version
                OUTPUT_VARIABLE _v ERROR_QUIET RESULT_VARIABLE _rc
                OUTPUT_STRIP_TRAILING_WHITESPACE)

            if(_rc EQUAL 0 AND _v MATCHES "Free Software Foundation"
               AND NOT _v MATCHES "clang")
                set(${outvar} "${MCC_GNUGCC_${_n}}" PARENT_SCOPE)
                return()
            endif()
        endif()
    endforeach()
    set(${outvar} "" PARENT_SCOPE)
endfunction()

function(mcc_map_target_args name outvar)
    if(DEFINED MCC_TARGET_ARGS_${name})
        set(${outvar} "${MCC_TARGET_ARGS_${name}}" PARENT_SCOPE)
    elseif(name STREQUAL "native")
        set(${outvar} "" PARENT_SCOPE)
    elseif(name STREQUAL "cross")
        set(${outvar} "-DMCC_ENABLE_CROSS=ON" PARENT_SCOPE)
    else()
        set(${outvar} "__UNKNOWN__" PARENT_SCOPE)
    endif()
endfunction()

function(mcc_autocorrect name value reason)
    get_property(_t GLOBAL PROPERTY MCC_NODE_${name}_TYPE)
    get_property(_h GLOBAL PROPERTY MCC_NODE_${name}_HELP)
    if(_t STREQUAL "BOOL")
        set(${name} "${value}" CACHE BOOL "${_h}" FORCE)
    else()
        set(${name} "${value}" CACHE "${_t}" "${_h}" FORCE)
    endif()
    message(STATUS "mcc config [autocorrect]: ${name} -> ${value} (${reason})")
endfunction()

function(mcc_validate_config)
    get_property(_nodes GLOBAL PROPERTY MCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_choices GLOBAL PROPERTY MCC_NODE_${_n}_CHOICES)
        get_property(_emptyok GLOBAL PROPERTY MCC_NODE_${_n}_EMPTYOK)
        if(_choices)
            set(_allowed ${_choices})
            if(_emptyok)
                list(APPEND _allowed "")
            endif()
            if(NOT "${${_n}}" IN_LIST _allowed)
                message(WARNING
                    "mcc config: ${_n}='${${_n}}' is not one of the known values "
                    "(${_allowed}); proceeding, but check for a typo.")
            endif()
        endif()
    endforeach()

    if(NOT "${MCC_CONFIG_SEMLOCK}" STREQUAL "" AND
       NOT "${MCC_CONFIG_SEMLOCK}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "mcc config: MCC_CONFIG_SEMLOCK='${MCC_CONFIG_SEMLOCK}' must be a number")
    endif()

    if(MCC_BUILD_PROFILE AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "mcc config: MCC_BUILD_PROFILE (mcc_p, -pg) needs a GCC/Clang host "
            "compiler (have '${CMAKE_C_COMPILER_ID}'); disable it or set "
            "-DCMAKE_C_COMPILER to gcc/clang.")
    endif()
    if(MCC_BUILD_SANITIZE
       AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
       AND NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        message(FATAL_ERROR
            "mcc config: MCC_BUILD_SANITIZE needs a GCC/Clang or MSVC host compiler "
            "(have '${CMAKE_C_COMPILER_ID}'); disable it with -DMCC_BUILD_SANITIZE=OFF.")
    endif()
    if(MCC_BUILD_PROFILE AND MCC_TARGETOS STREQUAL "Darwin")
        message(FATAL_ERROR
            "mcc config: MCC_BUILD_PROFILE (mcc_p, -static) is unsupported on Darwin "
            "(no static crt0.o).")
    endif()
    if(CMAKE_CROSSCOMPILING AND "${MCC_EMULATOR}" STREQUAL "")
        if(MCC_CONFIG_AUTOCORRECT)
            if(NOT MCC_MCCRT_USE_HOSTCC)
                mcc_autocorrect(MCC_MCCRT_USE_HOSTCC ON
                    "no emulator: build mccrt with the host CC, not the foreign mcc")
            endif()
            if(MCC_BUILD_TESTS)
                mcc_autocorrect(MCC_BUILD_TESTS OFF
                    "no emulator: the test suite cannot run the foreign mcc / its exes")
            endif()
            if(MCC_BUILD_COVERAGE)
                mcc_autocorrect(MCC_BUILD_COVERAGE OFF
                    "no emulator: mcc_c is compiled by running the foreign mcc")
            endif()
            if(MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_BUILD_STATIC_LIB)
                message(FATAL_ERROR
                    "mcc config: even with MCC_CONFIG_AUTOCORRECT, a WIN32 *shared* "
                    "libmcc needs `mcc -impdef` to run on this host. Add "
                    "-DCMAKE_CROSSCOMPILING_EMULATOR=<run>, or build static with "
                    "-DMCC_BUILD_STATIC_LIB=ON.")
            endif()
        else()
            message(FATAL_ERROR
                "mcc config: CMAKE_CROSSCOMPILING is set, so the just-built `mcc` is a "
                "foreign binary, but it must run on this host to build libmccrt.a (and the "
                "tests/impdef/coverage steps). Set -DCMAKE_CROSSCOMPILING_EMULATOR=<run> "
                "(e.g. wine for a WIN32 target, qemu-arm for arm) so it can be executed, "
                "build host-runnable cross compilers instead with -DMCC_ENABLE_CROSS=ON "
                "(no toolchain file), or enable non-strict -DMCC_CONFIG_AUTOCORRECT=ON to "
                "build the runtime with the host CC and skip the run-mcc steps.")
        endif()
    endif()

    if(MCC_CONFIG_BCHECK AND NOT MCC_CONFIG_BACKTRACE)
        if(MCC_CONFIG_AUTOCORRECT)
            mcc_autocorrect(MCC_CONFIG_BACKTRACE ON
                "bcheck (-b) requires the backtrace runtime to link")
        else()
            message(WARNING
                "mcc config: MCC_CONFIG_BCHECK=ON with MCC_CONFIG_BACKTRACE=OFF -- the "
                "bcheck runtime is gated behind backtrace, so `mcc -b` will fail to link. "
                "Enable backtrace or disable bcheck (or set -DMCC_CONFIG_AUTOCORRECT=ON).")
        endif()
    endif()
    if(NOT MCC_ENABLE_RPATH AND MCC_BUILD_STATIC_LIB)
        message(WARNING
            "mcc config: MCC_ENABLE_RPATH=OFF has no effect with a static libmcc "
            "(MCC_BUILD_STATIC_LIB=ON); rpath is only baked for a shared libmcc.")
    endif()
    if(MCC_BUILD_STATIC_EXE AND NOT MCC_SINGLE_SOURCE AND NOT MCC_BUILD_STATIC_LIB)
        message(WARNING
            "mcc config: MCC_BUILD_STATIC_EXE=ON with MCC_SINGLE_SOURCE=OFF links "
            "mcc-static (-static) against the libmcc archive, but "
            "MCC_BUILD_STATIC_LIB=OFF builds a shared libmcc — the static link "
            "cannot resolve libmcc.so. Enable MCC_BUILD_STATIC_LIB, or keep "
            "MCC_SINGLE_SOURCE=ON so mcc-static is self-contained.")
    endif()
    if(NOT MCC_CONFIG_ASM AND NOT MCC_MCCRT_USE_HOSTCC)
        mcc_autocorrect(MCC_MCCRT_USE_HOSTCC ON
            "asm disabled: mccrt .S sources must be built with the host CC")
    endif()
    if(NOT MCC_TOOLCHAIN_PROFILE STREQUAL "auto"
       AND NOT MCC_TOOLCHAIN_PROFILE STREQUAL "${MCC_CC_NAME}")
        message(WARNING
            "mcc config: MCC_TOOLCHAIN_PROFILE='${MCC_TOOLCHAIN_PROFILE}' but the "
            "detected host compiler is '${MCC_CC_NAME}'. The profile seeds defaults; "
            "it does NOT switch compilers (use -DCMAKE_C_COMPILER for that).")
    endif()
    if(NOT MCC_CONFIG_PREDEFS)
        message(WARNING
            "mcc config: MCC_CONFIG_PREDEFS=OFF makes mcc load <mccdefs.h> from the "
            "mccdir at runtime instead of compiling it in -- a runtime dependency.")
    endif()
    if(MCC_CONFIG_LIBC STREQUAL "uClibc")
        message(WARNING
            "mcc config: MCC_CONFIG_LIBC=uClibc is a legacy selector (emits "
            "CONFIG_MCC_UCLIBC, no path effect on modern hosts).")
    endif()
    if((MCC_TARGETOS STREQUAL "WIN32" OR MCC_TARGETOS STREQUAL "Darwin") AND
       (MCC_CONFIG_NEW_DTAGS OR MCC_CONFIG_PIE OR MCC_CONFIG_PIC
        OR NOT MCC_CONFIG_LIBC STREQUAL ""))
        message(WARNING
            "mcc config: NEW_DTAGS/PIE/PIC/LIBC are ELF-only and inert on the "
            "${MCC_TARGETOS} target.")
    endif()
    if(NOT MCC_TARGETOS STREQUAL "Darwin")
        if(NOT MCC_CONFIG_CODESIGN STREQUAL "" OR NOT MCC_CONFIG_NEW_MACHO STREQUAL "")
            message(WARNING
                "mcc config: CODESIGN/NEW_MACHO only affect Mach-O output; inert on the "
                "${MCC_TARGETOS} target.")
        endif()
    endif()
endfunction()

function(mcc_report_config)
    message(STATUS "================ mcc configuration ================")
    get_property(_st GLOBAL PROPERTY MCC_STATUS_NODES)
    foreach(_n IN LISTS _st)
        get_property(_v GLOBAL PROPERTY MCC_STATUS_${_n})
        message(STATUS "  ${_n} = ${_v}   [derived]")
    endforeach()
    get_property(_nodes GLOBAL PROPERTY MCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_type GLOBAL PROPERTY MCC_NODE_${_n}_TYPE)
        get_property(_realtype CACHE ${_n} PROPERTY TYPE)
        if(_realtype STREQUAL "INTERNAL")
            continue()
        endif()
        message(STATUS "  ${_n} = ${${_n}}")
    endforeach()
    message(STATUS "===================================================")
endfunction()

function(mcc_generate_node_doc outfile)
    set(_md "<!-- Rendered from CMakeLists.txt section 1z by mcc_generate_node_doc(). -->\n")
    string(APPEND _md "# mcc configurable nodes\n\n")
    string(APPEND _md "The user-settable build knobs, grouped, with their relevance gate.\n")
    string(APPEND _md "Source of truth: the mcc_config_node() declarations in CMakeLists.txt\n")
    string(APPEND _md "section 1z; preprocessor-flag catalog in section 3a.\n\n")
    string(APPEND _md "| Node | Group | Type | Default | Choices | Shown when | Description |\n")
    string(APPEND _md "|---|---|---|---|---|---|---|\n")
    get_property(_nodes GLOBAL PROPERTY MCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_g GLOBAL PROPERTY MCC_NODE_${_n}_GROUP)
        get_property(_t GLOBAL PROPERTY MCC_NODE_${_n}_TYPE)
        get_property(_d GLOBAL PROPERTY MCC_NODE_${_n}_DEFAULT)
        get_property(_c GLOBAL PROPERTY MCC_NODE_${_n}_CHOICES)
        get_property(_e GLOBAL PROPERTY MCC_NODE_${_n}_EMPTYOK)
        get_property(_w GLOBAL PROPERTY MCC_NODE_${_n}_WHEN)
        get_property(_h GLOBAL PROPERTY MCC_NODE_${_n}_HELP)
        if(_e AND _c)
            set(_c "'';${_c}")
        elseif(_e)
            set(_c "''")
        endif()
        string(REPLACE ";" ", " _c "${_c}")
        if(_d STREQUAL "")
            set(_d "''")
        endif()
        if(_w STREQUAL "")
            set(_w "always")
        endif()
        string(APPEND _md "| `${_n}` | ${_g} | ${_t} | `${_d}` | ${_c} | ${_w} | ${_h} |\n")
    endforeach()
    file(WRITE "${outfile}" "${_md}")
    message(STATUS "mcc: generated ${outfile}")
endfunction()

function(mcc_write_node_manifest outfile)
    set(_txt "")
    get_property(_nodes GLOBAL PROPERTY MCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_t GLOBAL PROPERTY MCC_NODE_${_n}_TYPE)
        string(APPEND _txt "${_n} ${_t}\n")
    endforeach()
    file(WRITE "${outfile}" "${_txt}")
endfunction()

set(_elf "NOT MCC_TARGETOS STREQUAL \"WIN32\" AND NOT MCC_TARGETOS STREQUAL \"Darwin\"")
set(_darwin "MCC_TARGETOS STREQUAL \"Darwin\"")

mcc_snapshot_user_presets()

mcc_config_node(MCC_TOOLCHAIN_PROFILE TYPE STRING DEFAULT auto GROUP "Build targets"
    HELP "Toolchain(s) to build with, in order (list, e.g. gcc;clang;mcc); single = profile+defaults"
    CHOICES auto gcc clang mcc msvc mingw)

set(_tc_allowed auto gcc clang mcc msvc mingw)
foreach(_tc IN LISTS MCC_TOOLCHAIN_PROFILE)
    if(NOT _tc IN_LIST _tc_allowed)
        message(FATAL_ERROR
            "mcc config: MCC_TOOLCHAIN_PROFILE entry '${_tc}' is not one of: ${_tc_allowed}")
    endif()
endforeach()

option(MCC_SUPERBUILD_TEST "Run ctest after each matrix sub-build" ON)
option(MCC_SUPERBUILD_SEQUENTIAL "Build toolchain stages in order (targets within a stage stay parallel)" ON)
option(MCC_SUPERBUILD_CHILD "Internal: set on matrix child cells to suppress superbuild recursion" OFF)
mark_as_advanced(MCC_SUPERBUILD_CHILD)
set(MCC_TARGETS "native" CACHE STRING
    "Cross-platform target(s) to build, as a list (native;cross;<custom>)")
set_property(CACHE MCC_TARGETS PROPERTY STRINGS native cross)
set(MCC_GNU_GCC "" CACHE FILEPATH
    "Pin the real GNU gcc used by the 'gcc' toolchain profile (empty = probe gcc-16..gcc-10, gcc, cc)")
mark_as_advanced(MCC_GNU_GCC)

set(MCC_VENDOR_DIR "${CMAKE_SOURCE_DIR}/vendor" CACHE PATH
    "Root directory for downloaded/vendored toolchains and externals (gcc/clang/mingw/musl)")

set(MCC_DIST_DIR "${CMAKE_SOURCE_DIR}/dist" CACHE PATH
    "Root directory for install staging and packaged release bundles (default CMAKE_INSTALL_PREFIX; docker-shared)")

mcc_config_node(MCC_MINGW_SOURCE TYPE STRING DEFAULT winlibs GROUP "Build targets" ADVANCED
    HELP "Source for the `mingw-toolchain` download: winlibs (dual-arch, current) or multilib (single gcc -m32/-m64)"
    CHOICES winlibs multilib)
set(MCC_MINGW_DIR "${MCC_VENDOR_DIR}" CACHE PATH
    "Parent directory the downloaded mingw toolchain trees live in")
set(MCC_MINGW_WINLIBS_VER "16.1.0-ucrt" CACHE STRING
    "WinLibs GCC version tag used in the vendor/winlibs-mingw-w64-<ver>-<arch> dir names")

set(_wl "https://github.com/brechtsanders/winlibs_mingw/releases/download/16.1.0posix-14.0.0-ucrt-r3")
set(MCC_MINGW_WINLIBS_X86_64_URL
    "${_wl}/winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip"
    CACHE STRING "WinLibs x86_64 (64-bit) toolchain zip URL")
set(MCC_MINGW_WINLIBS_X86_64_SHA256
    "4273565109cd8ab8ecef1b0dc2a56fd7f5c7ee0885840a1c011b9325160ec0c3"
    CACHE STRING "SHA256 of the WinLibs x86_64 zip")
set(MCC_MINGW_WINLIBS_I686_URL
    "${_wl}/winlibs-i686-posix-dwarf-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip"
    CACHE STRING "WinLibs i686 (native 32-bit) toolchain zip URL")
set(MCC_MINGW_WINLIBS_I686_SHA256
    "c4c7419f2820ac2e169dc86f1397a07deff20297a107c7a1ca486643d8d435be"
    CACHE STRING "SHA256 of the WinLibs i686 zip")
unset(_wl)

set(MCC_MINGW_MULTILIB_URL "" CACHE STRING
    "Archive URL for a single-gcc multilib toolchain (required when MCC_MINGW_SOURCE=multilib)")
set(MCC_MINGW_MULTILIB_SHA256 "" CACHE STRING "Optional SHA256 of the multilib archive")
set(MCC_MINGW_MULTILIB_SUBDIR "mingw64" CACHE STRING
    "Subdirectory inside the multilib archive that holds bin/gcc")

function(mcc_mingw_resolve)
    set(_x "")
    set(_i "")
    set(_m "")
    if(MCC_MINGW_SOURCE STREQUAL "winlibs")
        set(_x "${MCC_MINGW_DIR}/winlibs-mingw-w64-${MCC_MINGW_WINLIBS_VER}-x86_64/mingw64/bin/gcc.exe")
        set(_i "${MCC_MINGW_DIR}/winlibs-mingw-w64-${MCC_MINGW_WINLIBS_VER}-i686/mingw32/bin/gcc.exe")
    elseif(MCC_MINGW_SOURCE STREQUAL "multilib")
        set(_m "${MCC_MINGW_DIR}/mingw-w64-multilib/${MCC_MINGW_MULTILIB_SUBDIR}/bin/gcc.exe")
        set(_x "${_m}")   
    endif()
    set(_MCC_MINGW_X86_64_GCC   "${_x}" PARENT_SCOPE)
    set(_MCC_MINGW_I686_GCC     "${_i}" PARENT_SCOPE)
    set(_MCC_MINGW_MULTILIB_GCC "${_m}" PARENT_SCOPE)
endfunction()

set(_mingw_entries "")
if(MCC_MINGW_SOURCE STREQUAL "winlibs")
    list(APPEND _mingw_entries
        "${MCC_MINGW_WINLIBS_X86_64_URL}|${MCC_MINGW_WINLIBS_X86_64_SHA256}|${MCC_MINGW_DIR}/winlibs-mingw-w64-${MCC_MINGW_WINLIBS_VER}-x86_64")
    list(APPEND _mingw_entries
        "${MCC_MINGW_WINLIBS_I686_URL}|${MCC_MINGW_WINLIBS_I686_SHA256}|${MCC_MINGW_DIR}/winlibs-mingw-w64-${MCC_MINGW_WINLIBS_VER}-i686")
elseif(MCC_MINGW_SOURCE STREQUAL "multilib")
    list(APPEND _mingw_entries
        "${MCC_MINGW_MULTILIB_URL}|${MCC_MINGW_MULTILIB_SHA256}|${MCC_MINGW_DIR}/mingw-w64-multilib")
endif()

set(_mingw_fetch "${CMAKE_BINARY_DIR}/run_mingw_fetch.cmake")
file(WRITE "${_mingw_fetch}" "set(SOURCE \"${MCC_MINGW_SOURCE}\")\nset(ENTRIES\n")
foreach(_e IN LISTS _mingw_entries)
    file(APPEND "${_mingw_fetch}" "    \"${_e}\"\n")
endforeach()
file(APPEND "${_mingw_fetch}" ")\n")
file(APPEND "${_mingw_fetch}" [==[

foreach(_entry IN LISTS ENTRIES)
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _url)
    list(GET _parts 1 _sha)
    list(GET _parts 2 _dest)
    if(_url STREQUAL "")
        message(FATAL_ERROR
            "mingw-toolchain: no archive URL for source '${SOURCE}'. "
            "For multilib, set -DMCC_MINGW_MULTILIB_URL=<archive-url>.")
    endif()
    if(EXISTS "${_dest}/.mcc-mingw-stamp")
        message(STATUS "mingw-toolchain: ${_dest} already present (delete its "
                       ".mcc-mingw-stamp to refetch)")
        continue()
    endif()
    get_filename_component(_zip "${_dest}.download" ABSOLUTE)
    message(STATUS "mingw-toolchain: downloading ${_url}")
    set(_hashargs "")
    if(NOT _sha STREQUAL "")
        set(_hashargs EXPECTED_HASH SHA256=${_sha})
    endif()
    file(DOWNLOAD "${_url}" "${_zip}" STATUS _st SHOW_PROGRESS TIMEOUT 3600 ${_hashargs})
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
        file(REMOVE "${_zip}")
        message(FATAL_ERROR "mingw-toolchain: download failed (${_st}): ${_url}")
    endif()
    file(REMOVE_RECURSE "${_dest}")
    file(MAKE_DIRECTORY "${_dest}")
    message(STATUS "mingw-toolchain: extracting into ${_dest}")
    file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${_dest}")
    file(REMOVE "${_zip}")
    file(WRITE "${_dest}/.mcc-mingw-stamp" "${_url}\n")
    message(STATUS "mingw-toolchain: established ${_dest}")
endforeach()
]==])

add_custom_target(mingw-toolchain
    COMMAND "${CMAKE_COMMAND}" -P "${_mingw_fetch}"
    COMMENT "Fetching self-contained mingw toolchain (${MCC_MINGW_SOURCE}) into vendor/winlibs-mingw-w64-* (or mingw-w64-multilib)"
    USES_TERMINAL VERBATIM)

set(MCC_CLANG_DIR "${MCC_VENDOR_DIR}" CACHE PATH
    "Parent directory the downloaded LLVM/clang tree (llvm-clang) lives in")
set(_clang_url "")
set(_clang_sha "")
set(_clang_subdir "")
set(_clang_ver "22.1.8")
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_clang_subdir "clang+llvm-${_clang_ver}-x86_64-pc-windows-msvc")
    set(_clang_sha "d96c2cc1736f4eb7fa43cb9bbdf56d93551a9ae0a9aadb9c99c3c3b2b712a234")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
       AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    set(_clang_subdir "LLVM-${_clang_ver}-Linux-X64")
    set(_clang_sha "df0e1ecf16caf3489a272a5eea4eec9b0d82878f6477fa309504f918a0006384")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
       AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(_clang_subdir "LLVM-${_clang_ver}-Linux-ARM64")
    set(_clang_sha "805efad2bb91cb4967fa569e0881d10c0f69c04461cf671cccbae19f547acc34")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin"
       AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(_clang_subdir "LLVM-${_clang_ver}-macOS-ARM64")
    set(_clang_sha "f260f4f7c0d430828a81ae8a3826a1d63fc0963ec2459489308cc23b1f7eab4f")
endif()
if(NOT _clang_subdir STREQUAL "")
    set(_clang_url
        "https://github.com/llvm/llvm-project/releases/download/llvmorg-${_clang_ver}/${_clang_subdir}.tar.xz")
endif()
unset(_clang_ver)
set(MCC_CLANG_URL "${_clang_url}" CACHE STRING
    "LLVM release archive URL for the `clang-toolchain` download")
set(MCC_CLANG_SHA256 "${_clang_sha}" CACHE STRING
    "SHA256 of the LLVM release archive (empty to skip verification)")
set(MCC_CLANG_SUBDIR "${_clang_subdir}" CACHE STRING
    "Top-level directory inside the LLVM archive (the one holding bin/clang)")
unset(_clang_url)
unset(_clang_sha)
unset(_clang_subdir)

function(mcc_clang_resolve)
    set(_c "")
    if(NOT MCC_CLANG_SUBDIR STREQUAL "")
        set(_c "${MCC_CLANG_DIR}/llvm-clang/${MCC_CLANG_SUBDIR}/bin/clang${CMAKE_EXECUTABLE_SUFFIX}")
    endif()
    if(_c STREQUAL "" OR NOT EXISTS "${_c}")
        file(GLOB _g "${MCC_CLANG_DIR}/llvm-clang/*/bin/clang${CMAKE_EXECUTABLE_SUFFIX}")
        if(_g)
            list(GET _g 0 _c)
        endif()
    endif()
    set(_MCC_CLANG_EXE "${_c}" PARENT_SCOPE)
endfunction()

set(_clang_fetch "${CMAKE_BINARY_DIR}/run_clang_fetch.cmake")
file(WRITE "${_clang_fetch}"
    "set(URL \"${MCC_CLANG_URL}\")\n"
    "set(SHA \"${MCC_CLANG_SHA256}\")\n"
    "set(DEST \"${MCC_CLANG_DIR}/llvm-clang\")\n")
file(APPEND "${_clang_fetch}" [==[

if(URL STREQUAL "")
    message(FATAL_ERROR
        "clang-toolchain: no MCC_CLANG_URL for this host. Set -DMCC_CLANG_URL=<llvm-release-archive> "
        "(and -DMCC_CLANG_SUBDIR=<top-dir-in-archive>).")
endif()
if(EXISTS "${DEST}/.mcc-clang-stamp")
    message(STATUS "clang-toolchain: ${DEST} already present (delete its .mcc-clang-stamp to refetch)")
    return()
endif()
get_filename_component(_arc "${DEST}.download" ABSOLUTE)
message(STATUS "clang-toolchain: downloading ${URL}")
set(_hashargs "")
if(NOT SHA STREQUAL "")
    set(_hashargs EXPECTED_HASH SHA256=${SHA})
endif()
file(DOWNLOAD "${URL}" "${_arc}" STATUS _st SHOW_PROGRESS TIMEOUT 7200 ${_hashargs})
list(GET _st 0 _code)
if(NOT _code EQUAL 0)
    file(REMOVE "${_arc}")
    message(FATAL_ERROR "clang-toolchain: download failed (${_st}): ${URL}")
endif()
file(REMOVE_RECURSE "${DEST}")
file(MAKE_DIRECTORY "${DEST}")
message(STATUS "clang-toolchain: extracting into ${DEST} (large archive; this takes a while)")
file(ARCHIVE_EXTRACT INPUT "${_arc}" DESTINATION "${DEST}")
file(REMOVE "${_arc}")
file(WRITE "${DEST}/.mcc-clang-stamp" "${URL}\n")
message(STATUS "clang-toolchain: established ${DEST}")
]==])

add_custom_target(clang-toolchain
    COMMAND "${CMAKE_COMMAND}" -P "${_clang_fetch}"
    COMMENT "Fetching self-contained LLVM/clang toolchain into vendor/llvm-clang"
    USES_TERMINAL VERBATIM)

find_program(GIT_EXECUTABLE git)
set(MCC_MUSL_URL "https://github.com/ifduyue/musl" CACHE STRING
    "Primary musl git repository URL for the `vendor-musl` target (a GitHub mirror by default: the canonical git.musl-libc.org is frequently unreachable from CI runners and blackholes the connection for minutes before falling back)")
set(MCC_MUSL_MIRROR "https://git.musl-libc.org/git/musl" CACHE STRING
    "Fallback musl git repository for `vendor-musl` (tried if MCC_MUSL_URL is unreachable)")
set(MCC_MUSL_TARBALL "https://codeload.github.com/ifduyue/musl/tar.gz/refs/tags/@REF@" CACHE STRING
    "Last-resort musl source tarball for `vendor-musl` (plain HTTPS, no git credentials); @REF@ is replaced by MCC_MUSL_REF. Empty disables the tarball fallback.")
set(MCC_MUSL_REF "v1.2.5" CACHE STRING
    "musl git ref (tag/branch) to clone for `vendor-musl` (empty = default branch)")
set(MCC_MUSL_DIR "${MCC_VENDOR_DIR}/musl-src" CACHE PATH
    "Destination for the vendored musl source checkout")

set(_musl_fetch "${CMAKE_BINARY_DIR}/run_musl_fetch.cmake")
file(WRITE "${_musl_fetch}"
    "set(GIT \"${GIT_EXECUTABLE}\")\n"
    "set(URL \"${MCC_MUSL_URL}\")\n"
    "set(MIRROR \"${MCC_MUSL_MIRROR}\")\n"
    "set(TARBALL \"${MCC_MUSL_TARBALL}\")\n"
    "set(REF \"${MCC_MUSL_REF}\")\n"
    "set(DEST \"${MCC_MUSL_DIR}\")\n")
file(APPEND "${_musl_fetch}" [==[
if(GIT STREQUAL "" OR NOT EXISTS "${GIT}")
    message(FATAL_ERROR "vendor-musl: git not found. Install git or pass -DGIT_EXECUTABLE=<path>.")
endif()
# A git checkout has .git; a tarball-extracted tree is marked with a stamp.
if(EXISTS "${DEST}/.git" OR EXISTS "${DEST}/.vendor-musl.stamp")
    message(STATUS "vendor-musl: ${DEST} already present (delete it to re-fetch)")
    return()
endif()
get_filename_component(_parent "${DEST}" DIRECTORY)
file(MAKE_DIRECTORY "${_parent}")
set(_branchargs "")
if(NOT REF STREQUAL "")
    set(_branchargs --branch "${REF}")
endif()

# Disable interactive credential prompting for every git invocation below: CI
# runners have no TTY, so an anonymous clone that draws an auth challenge (e.g.
# a mistyped/renamed mirror, which GitHub answers with 401, not 404) would
# otherwise fail with "could not read Username ... No such device or address"
# -- or hang waiting on a credential manager. GIT_TERMINAL_PROMPT=0 turns that
# into an immediate error; the low-speed limits abort a stalled transfer. These
# go on the script's own environment (inherited by execute_process) rather than
# a `cmake -E env` wrapper, which is unreliable when re-invoked nested here.
set(ENV{GIT_TERMINAL_PROMPT} "0")
set(ENV{GIT_HTTP_LOW_SPEED_LIMIT} "1000")
set(ENV{GIT_HTTP_LOW_SPEED_TIME} "30")
set(ENV{GCM_INTERACTIVE} "never")

function(_musl_git_clone url resultvar)
    message(STATUS "vendor-musl: cloning ${url} (${REF}) into ${DEST}")
    execute_process(
        COMMAND "${GIT}" -c credential.helper=
                clone --depth 1 ${_branchargs} "${url}" "${DEST}"
        RESULT_VARIABLE _rc)
    set(${resultvar} "${_rc}" PARENT_SCOPE)
endfunction()

_musl_git_clone("${URL}" _rc)
if(NOT _rc EQUAL 0 AND NOT MIRROR STREQUAL "")
    file(REMOVE_RECURSE "${DEST}")
    message(STATUS "vendor-musl: ${URL} unreachable (${_rc}); trying mirror ${MIRROR}")
    _musl_git_clone("${MIRROR}" _rc)
endif()

# Last resort: a plain HTTPS tarball via libcurl, which bypasses git's
# credential machinery entirely (the path that works behind restricted egress
# where anonymous git-over-https is challenged). The extracted tree has no
# .git, but musl-sysroot only needs the source, so a stamp file suffices.
if(NOT _rc EQUAL 0 AND NOT TARBALL STREQUAL "")
    file(REMOVE_RECURSE "${DEST}")
    string(REPLACE "@REF@" "${REF}" _turl "${TARBALL}")
    message(STATUS "vendor-musl: git clone failed; downloading tarball ${_turl}")
    set(_tgz "${_parent}/musl-src.tar.gz")
    set(_xdir "${_parent}/musl-src.extract")
    file(REMOVE "${_tgz}")
    file(REMOVE_RECURSE "${_xdir}")
    file(DOWNLOAD "${_turl}" "${_tgz}"
        STATUS _dstat TIMEOUT 300 INACTIVITY_TIMEOUT 30 SHOW_PROGRESS)
    list(GET _dstat 0 _drc)
    if(_drc EQUAL 0)
        file(MAKE_DIRECTORY "${_xdir}")
        file(ARCHIVE_EXTRACT INPUT "${_tgz}" DESTINATION "${_xdir}")
        file(GLOB _tops LIST_DIRECTORIES true "${_xdir}/*")
        list(LENGTH _tops _ntop)
        if(_ntop EQUAL 1)
            list(GET _tops 0 _top)
            file(RENAME "${_top}" "${DEST}")
            file(WRITE "${DEST}/.vendor-musl.stamp" "musl ${REF} from ${_turl}\n")
            set(_rc 0)
        else()
            message(STATUS "vendor-musl: unexpected tarball layout (${_ntop} top-level entries)")
        endif()
        file(REMOVE "${_tgz}")
        file(REMOVE_RECURSE "${_xdir}")
    else()
        list(GET _dstat 1 _dmsg)
        message(STATUS "vendor-musl: tarball download failed: ${_dmsg}")
    endif()
endif()

if(NOT _rc EQUAL 0)
    file(REMOVE_RECURSE "${DEST}")
    message(FATAL_ERROR
        "vendor-musl: could not obtain musl ${REF}. Tried:\n"
        "  git   ${URL}\n"
        "  git   ${MIRROR}\n"
        "  https ${TARBALL}\n"
        "Point -DMCC_MUSL_URL / -DMCC_MUSL_MIRROR / -DMCC_MUSL_TARBALL at a reachable "
        "source, or pre-populate ${DEST}.")
endif()
message(STATUS "vendor-musl: established ${DEST}")
]==])

add_custom_target(vendor-musl
    COMMAND "${CMAKE_COMMAND}" -P "${_musl_fetch}"
    COMMENT "Cloning musl (${MCC_MUSL_REF}) into vendor/musl-src"
    USES_TERMINAL VERBATIM)

set(MCC_MUSL_SYSROOT "${MCC_VENDOR_DIR}/musl-sysroot" CACHE PATH
    "Sysroot the mcc*-musl variants target (built by the `musl-sysroot` target from vendor/musl-src; vendored so it is downloaded/built once and shared across build dirs). Empty = use a system musl (/lib/ld-musl-*).")

set(_musl_sysroot_build "${CMAKE_BINARY_DIR}/run_musl_sysroot.cmake")
file(WRITE "${_musl_sysroot_build}"
    "set(SRC \"${MCC_MUSL_DIR}\")\n"
    "set(BLD \"${CMAKE_BINARY_DIR}/musl-build\")\n"
    "set(SR  \"${MCC_MUSL_SYSROOT}\")\n")
file(APPEND "${_musl_sysroot_build}" [==[
if(SR STREQUAL "")
    message(FATAL_ERROR "musl-sysroot: MCC_MUSL_SYSROOT is empty (system-musl mode); nothing to build.")
endif()
if(NOT EXISTS "${SRC}/configure")
    message(FATAL_ERROR "musl-sysroot: no musl source at ${SRC}. Run the `vendor-musl` target first.")
endif()
if(EXISTS "${SR}/lib/libc.so")
    message(STATUS "musl-sysroot: ${SR} already built (delete it to rebuild)")
    return()
endif()
cmake_host_system_information(RESULT _np QUERY NUMBER_OF_LOGICAL_CORES)
file(MAKE_DIRECTORY "${BLD}")
file(MAKE_DIRECTORY "${SR}")
message(STATUS "musl-sysroot: configuring musl (${SRC}) -> ${SR}")
execute_process(COMMAND "${SRC}/configure" "--prefix=${SR}" "--syslibdir=${SR}/lib"
    WORKING_DIRECTORY "${BLD}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "musl-sysroot: configure failed (${_rc})")
endif()
message(STATUS "musl-sysroot: building (make -j${_np})")
execute_process(COMMAND make -j${_np} WORKING_DIRECTORY "${BLD}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "musl-sysroot: make failed (${_rc})")
endif()
execute_process(COMMAND make install WORKING_DIRECTORY "${BLD}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "musl-sysroot: make install failed (${_rc})")
endif()
message(STATUS "musl-sysroot: established ${SR}")
]==])

add_custom_target(musl-sysroot
    COMMAND "${CMAKE_COMMAND}" -P "${_musl_sysroot_build}"
    COMMENT "Building musl sysroot -> ${MCC_MUSL_SYSROOT}"
    USES_TERMINAL VERBATIM)
add_dependencies(musl-sysroot vendor-musl)

set(_toolchain_setup "${CMAKE_BINARY_DIR}/run_toolchain_setup.cmake")
file(WRITE "${_toolchain_setup}" [==[

cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED TOOL)
    message(FATAL_ERROR "toolchain-setup: pass -DTOOL=gcc|wine|mingw")
endif()

find_program(_BREW brew)
find_program(_APT apt-get)
find_program(_DNF dnf)
find_program(_PACMAN pacman)
find_program(_ZYPPER zypper)

set(_cmd "")
if(_BREW)
    if(TOOL STREQUAL "gcc")
        set(_cmd "${_BREW}" install gcc)
    elseif(TOOL STREQUAL "wine")
        set(_cmd "${_BREW}" install --cask --no-quarantine wine-stable)
    elseif(TOOL STREQUAL "mingw")
        set(_cmd "${_BREW}" install mingw-w64)
    endif()
else()

    set(_sudo "")
    if(NOT "$ENV{USER}" STREQUAL "root")
        find_program(_SUDO sudo)
        if(_SUDO)
            set(_sudo "${_SUDO}")
        endif()
    endif()
    if(_APT)
        if(TOOL STREQUAL "gcc")
            set(_pkgs gcc)
        elseif(TOOL STREQUAL "wine")
            set(_pkgs wine)
        elseif(TOOL STREQUAL "mingw")
            set(_pkgs gcc-mingw-w64 g++-mingw-w64)
        endif()
        set(_cmd ${_sudo} "${_APT}" install -y ${_pkgs})
    elseif(_DNF)
        if(TOOL STREQUAL "gcc")
            set(_pkgs gcc)
        elseif(TOOL STREQUAL "wine")
            set(_pkgs wine)
        elseif(TOOL STREQUAL "mingw")
            set(_pkgs mingw64-gcc)
        endif()
        set(_cmd ${_sudo} "${_DNF}" install -y ${_pkgs})
    elseif(_PACMAN)
        if(TOOL STREQUAL "gcc")
            set(_pkgs gcc)
        elseif(TOOL STREQUAL "wine")
            set(_pkgs wine)
        elseif(TOOL STREQUAL "mingw")
            set(_pkgs mingw-w64-gcc)
        endif()
        set(_cmd ${_sudo} "${_PACMAN}" -S --noconfirm ${_pkgs})
    elseif(_ZYPPER)
        if(TOOL STREQUAL "gcc")
            set(_pkgs gcc)
        elseif(TOOL STREQUAL "wine")
            set(_pkgs wine)
        elseif(TOOL STREQUAL "mingw")
            set(_pkgs mingw64-cross-gcc)
        endif()
        set(_cmd ${_sudo} "${_ZYPPER}" install -y ${_pkgs})
    endif()
endif()

if(NOT _cmd)
    message(FATAL_ERROR
        "toolchain-setup(${TOOL}): no supported package manager found "
        "(looked for brew/apt-get/dnf/pacman/zypper). Install '${TOOL}' manually.")
endif()

string(REPLACE ";" " " _pretty "${_cmd}")
message(STATUS "toolchain-setup(${TOOL}): ${_pretty}")
execute_process(COMMAND ${_cmd} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "toolchain-setup(${TOOL}): install failed (rc=${_rc})")
endif()
message(STATUS "toolchain-setup(${TOOL}): done -- re-run cmake to detect it")
]==])

add_custom_target(setup-gcc
    COMMAND "${CMAKE_COMMAND}" -DTOOL=gcc -P "${_toolchain_setup}"
    COMMENT "Installing a GNU gcc reference (widens diff3/preprocess coverage)"
    USES_TERMINAL VERBATIM)
add_custom_target(setup-wine
    COMMAND "${CMAKE_COMMAND}" -DTOOL=wine -P "${_toolchain_setup}"
    COMMENT "Installing wine (runs mcc PE output: pe-wine-conformance)"
    USES_TERMINAL VERBATIM)
add_custom_target(setup-mingw
    COMMAND "${CMAKE_COMMAND}" -DTOOL=mingw -P "${_toolchain_setup}"
    COMMENT "Installing a host mingw-w64 cross gcc (PE reference compiler)"
    USES_TERMINAL VERBATIM)
add_custom_target(setup-toolchains
    COMMENT "Installing all optional host toolchains (gcc, wine, mingw)")
add_dependencies(setup-toolchains setup-gcc setup-wine setup-mingw)

list(LENGTH MCC_TOOLCHAIN_PROFILE _tc_count)
list(LENGTH MCC_TARGETS _tg_count)
set(_is_matrix FALSE)

if(_tc_count GREATER 1 OR _tg_count GREATER 1 OR NOT MCC_TARGETS STREQUAL "native"
   OR "mingw" IN_LIST MCC_TOOLCHAIN_PROFILE)
    set(_is_matrix TRUE)
endif()
if(_is_matrix AND NOT MCC_SUPERBUILD_CHILD)
    include(ExternalProject)
    set(_order "parallel")
    if(MCC_SUPERBUILD_SEQUENTIAL)
        set(_order "toolchains in order, targets parallel")
    endif()
    message(STATUS "mcc: superbuild matrix toolchains=[${MCC_TOOLCHAIN_PROFILE}] x "
                   "targets=[${MCC_TARGETS}] (${_order})")

    set(MCC_MSVC_GENERATOR "" CACHE STRING
        "CMake generator for 'msvc' toolchain cells (empty = auto-detect)")
    set(MCC_MSVC_PLATFORM "x64" CACHE STRING
        "Generator platform (-A) for 'msvc' toolchain cells")
    if(WIN32 AND MCC_MSVC_GENERATOR STREQUAL "" AND "msvc" IN_LIST MCC_TOOLCHAIN_PROFILE)
        if(CMAKE_GENERATOR MATCHES "Visual Studio")
            set(MCC_MSVC_GENERATOR "${CMAKE_GENERATOR}")
        else()
            find_program(MCC_VSWHERE vswhere PATHS
                "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer")
            if(MCC_VSWHERE)
                execute_process(COMMAND "${MCC_VSWHERE}" -latest -property installationVersion
                    OUTPUT_VARIABLE _vsver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
                string(REGEX MATCH "^[0-9]+" _vsmaj "${_vsver}")
                if(_vsmaj STREQUAL "18")
                    set(MCC_MSVC_GENERATOR "Visual Studio 18 2026")
                elseif(_vsmaj STREQUAL "17")
                    set(MCC_MSVC_GENERATOR "Visual Studio 17 2022")
                elseif(_vsmaj STREQUAL "16")
                    set(MCC_MSVC_GENERATOR "Visual Studio 16 2019")
                endif()
            endif()
        endif()
        if(MCC_MSVC_GENERATOR STREQUAL "")
            message(FATAL_ERROR
                "mcc config: toolchain 'msvc' requested but no Visual Studio "
                "generator could be detected; pass -DMCC_MSVC_GENERATOR=\"Visual "
                "Studio NN YYYY\" (and optionally -DMCC_MSVC_PLATFORM).")
        endif()
        message(STATUS "mcc: msvc cells use generator '${MCC_MSVC_GENERATOR}' (-A ${MCC_MSVC_PLATFORM})")
    endif()

    set(_forward "")
    foreach(_v IN LISTS _MCC_PRESET_VARS)
        if(_v MATCHES "^MCC_(TOOLCHAIN_PROFILE|TARGETS|SUPERBUILD_TEST|SUPERBUILD_SEQUENTIAL)$")
            continue()
        endif()
        list(APPEND _forward "-D${_v}=${${_v}}")
    endforeach()
    set(_prev_stage "")
    set(_bench_cmds "")
    set(_pkg_cmds "")
    foreach(_tc IN LISTS MCC_TOOLCHAIN_PROFILE)
        mcc_map_toolchain_cc("${_tc}" _cc)
        set(_cell_is_msvc OFF)
        if(_tc STREQUAL "msvc")
            set(_cell_is_msvc ON)   
        endif()
        if(NOT _tc STREQUAL "auto" AND _cc STREQUAL "" AND NOT _cell_is_msvc)
            message(FATAL_ERROR
                "mcc config: toolchain '${_tc}' requested but its compiler was not "
                "found; install it or pass -DMCC_CC_${_tc}=<path>.")
        endif()
        set(_this_stage "")
        foreach(_tg IN LISTS MCC_TARGETS)
            mcc_map_target_args("${_tg}" _targs)
            if(_targs STREQUAL "__UNKNOWN__")
                message(FATAL_ERROR
                    "mcc config: target '${_tg}' is unknown; use native/cross or "
                    "define -DMCC_TARGET_ARGS_${_tg}=\"-Dk=v;...\".")
            endif()
            set(_cell "${_tc}-${_tg}")
            set(_cargs -DMCC_TOOLCHAIN_PROFILE=${_tc} -DMCC_SUPERBUILD_CHILD=ON
                       ${_forward} ${_targs})
            if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
                list(APPEND _cargs "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
            endif()
            if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
                list(APPEND _cargs "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")
            endif()
            if(NOT _cc STREQUAL "" AND NOT _cell_is_msvc)
                list(APPEND _cargs "-DCMAKE_C_COMPILER=${_cc}")
                if(WIN32)
                    get_filename_component(_ccdir "${_cc}" DIRECTORY)
                    if(EXISTS "${_ccdir}/llvm-rc.exe")
                        list(APPEND _cargs "-DCMAKE_RC_COMPILER=${_ccdir}/llvm-rc.exe")
                    endif()
                endif()
            endif()

            set(_cell_gen_args "")
            if(_cell_is_msvc AND NOT MCC_MSVC_GENERATOR STREQUAL "")
                set(_cell_gen_args CMAKE_GENERATOR "${MCC_MSVC_GENERATOR}"
                                   CMAKE_GENERATOR_PLATFORM "${MCC_MSVC_PLATFORM}")
            endif()
            set(_dep_list "")
            if(MCC_SUPERBUILD_SEQUENTIAL AND _prev_stage)
                list(APPEND _dep_list ${_prev_stage})
            endif()
            if(_tc STREQUAL "mingw")

                list(APPEND _dep_list mingw-toolchain)
            endif()
            set(_dep "")
            if(_dep_list)
                set(_dep DEPENDS ${_dep_list})
            endif()
            ExternalProject_Add(${_cell}
                SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}"
                BINARY_DIR  "${CMAKE_BINARY_DIR}/${_cell}"
                CMAKE_ARGS  ${_cargs}
                ${_cell_gen_args}
                CONFIGURE_HANDLED_BY_BUILD ON
                BUILD_ALWAYS ON
                INSTALL_COMMAND ""
                ${_dep})
            if(MCC_SUPERBUILD_TEST)

                set(_cell_ctest_cfg "")
                if(_cell_is_msvc AND NOT MCC_MSVC_GENERATOR STREQUAL "")
                    if(CMAKE_BUILD_TYPE STREQUAL "")
                        set(_cell_ctest_cfg -C Debug)
                    else()
                        set(_cell_ctest_cfg -C "${CMAKE_BUILD_TYPE}")
                    endif()
                endif()
                ExternalProject_Add_Step(${_cell} ctest
                    COMMAND "${CMAKE_CTEST_COMMAND}" ${_cell_ctest_cfg} --output-on-failure
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${_cell}"
                    DEPENDEES build ALWAYS ON USES_TERMINAL ON)
            endif()
            set(_cell_cfg "")
            if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
                set(_cell_cfg " --config ${CMAKE_BUILD_TYPE}")
            endif()
            install(CODE "
                execute_process(COMMAND \"${CMAKE_COMMAND}\" --install \"${CMAKE_BINARY_DIR}/${_cell}\"${_cell_cfg}
                                RESULT_VARIABLE _rc)
                if(_rc)
                    message(FATAL_ERROR \"mcc superbuild: install of cell ${_cell} failed (\${_rc})\")
                endif()")
            if(MCC_BENCH)
                set(_cellbench_cfg "")
                if(_cell_is_msvc AND NOT MCC_MSVC_GENERATOR STREQUAL "")
                    if(CMAKE_BUILD_TYPE STREQUAL "")
                        set(_cellbench_cfg --config Debug)
                    else()
                        set(_cellbench_cfg --config "${CMAKE_BUILD_TYPE}")
                    endif()
                endif()
                list(APPEND _bench_cmds
                     COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/${_cell}"
                             ${_cellbench_cfg} --target bench)
            endif()
            set(_cellpkg_cfg "")
            if(_cell_is_msvc AND NOT MCC_MSVC_GENERATOR STREQUAL "")
                if(CMAKE_BUILD_TYPE STREQUAL "")
                    set(_cellpkg_cfg --config Debug)
                else()
                    set(_cellpkg_cfg --config "${CMAKE_BUILD_TYPE}")
                endif()
            endif()
            set(_pkg_cmds
                COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/${_cell}"
                        ${_cellpkg_cfg} --target package-dist)
            list(APPEND _this_stage ${_cell})
        endforeach()
        set(_prev_stage "${_this_stage}")
    endforeach()
    if(MCC_BENCH AND _bench_cmds)
        add_custom_target(bench ${_bench_cmds}
            USES_TERMINAL VERBATIM
            COMMENT "Benchmarking (superbuild: forwarding `bench` to each leaf cell)")
    endif()
    if(_pkg_cmds)
        add_custom_target(package-dist ${_pkg_cmds}
            USES_TERMINAL VERBATIM
            COMMENT "Packaging dist bundle (superbuild: forwarding `package-dist` to a leaf cell)")
    endif()
    return()
endif()

if(MCC_TOOLCHAIN_PROFILE STREQUAL "mcc")
    mcc_profile_seed(MCC_SINGLE_SOURCE ON)
elseif(MCC_TOOLCHAIN_PROFILE STREQUAL "msvc")

endif()

mcc_config_node(MCC_ENABLE_CROSS  TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build all cross compilers (Makefile: make cross)")

mcc_config_node(MCC_BUILD_STATIC_LIB TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build libmcc.a (ON) instead of shared libmcc (OFF)")
mcc_config_node(MCC_BUILD_STATIC_EXE TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Link the mcc executable(s) fully static (-static); -run resolves libc via the built-in table (common symbols only)")
mcc_config_node(MCC_BUILD_DYNAMIC_LIB TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Also build a shared libmcc (libmcc.so) alongside the static libmcc.a")
mcc_config_node(MCC_BUILD_DYNAMIC_EXE TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build mcc-dynamic: a single-source mcc executable dynamically linked against libc (the dynamic counterpart to mcc-static; not linked against libmcc)")
mcc_config_node(MCC_BUILD_MUSL       TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Also build musl-targeting compiler variants (mcc*-musl) beside the glibc ones")
if(APPLE AND MCC_BUILD_STATIC_EXE)
    set(MCC_BUILD_STATIC_EXE OFF)
    message(STATUS "mcc: macOS has no fully-static libc; forcing MCC_BUILD_STATIC_EXE=OFF")
endif()
set(_mcc_strip_default OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(_mcc_strip_default ON)
endif()
mcc_config_node(MCC_BUILD_STRIP      TYPE BOOL DEFAULT ${_mcc_strip_default} GROUP "Build targets"
    HELP "Strip symbols from the mcc executable(s) at link (-s); defaults ON for Release/MinSizeRel builds")
mcc_config_node(MCC_ENABLE_RPATH TYPE BOOL DEFAULT ON GROUP "Build targets"
    HELP "Bake -rpath into binaries linking libmcc.so so they find the shared lib at runtime"
    VISIBLE_WHEN "NOT MCC_BUILD_STATIC_LIB")
mcc_config_node(MCC_SINGLE_SOURCE    TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build libmcc from a single TU (Makefile: SINGLE_SOURCE)")
mcc_config_node(MCC_BUILD_TESTS   TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build/enable the test suite (tests/Makefile)")
mcc_config_node(MCC_BENCH         TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build the mccbench tool and the `bench` target (compile-speed report). Off by default; CI enables it")
mcc_config_node(MCC_CST TYPE BOOL DEFAULT ON GROUP "Advanced" ADVANCED
    HELP "Build the concrete syntax tree subsystem")
mcc_config_node(MCC_AST TYPE BOOL DEFAULT ON GROUP "Advanced" ADVANCED
    HELP "Build the AST intention-IR subsystem (side-channel; -O1 replay driver)")
mcc_config_node(MCC_ALL_DIAGNOSTICS TYPE BOOL DEFAULT OFF GROUP "Diagnostics" ADVANCED
    HELP "Everything-on diagnostics: verbose warnings + debug info + build mcc_s/mcc_p/mcc_c")
if(MCC_ALL_DIAGNOSTICS)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

        if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            message(STATUS "mcc diagnostics: skipping mcc_s (-fsanitize) with GNU gcc on "
                           "macOS (Homebrew gcc ships no linkable libasan/libubsan here)")
        else()
            mcc_profile_seed(MCC_BUILD_SANITIZE ON)
        endif()
        mcc_profile_seed(MCC_BUILD_COVERAGE ON)

        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            message(STATUS "mcc diagnostics: skipping mcc_p (-static profiling) on Darwin (no static crt0.o)")
        else()
            mcc_profile_seed(MCC_BUILD_PROFILE  ON)
        endif()
    else()
        message(STATUS "mcc diagnostics: host CC '${CMAKE_C_COMPILER_ID}' is not GCC/Clang; "
                       "skipping mcc_s/mcc_c/mcc_p (sanitize/coverage/profile need GCC/Clang)")
    endif()
endif()

mcc_config_node(MCC_BUILD_PROFILE  TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_p (profiling: -pg -static)")
mcc_config_node(MCC_BUILD_COVERAGE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_c (coverage instrumentation)")
mcc_config_node(MCC_BUILD_SANITIZE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_s (sanitized: GCC/Clang ASan+UBSan, MSVC ASan, mingw trap-UBSan)")
mcc_config_node(MCC_MCCRT_USE_HOSTCC TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build the native mccrt with the host CC instead of mcc (faster bcheck)")
mcc_config_node(MCC_EMBED_MCCRT TYPE BOOL DEFAULT ON GROUP "Build targets"
    HELP "Bake libmccrt.a into the mcc binary (self-contained; no sidecar .a needed). Forces the native mccrt to be host-CC built to break the build cycle. ELF/Mach-O only.")
mcc_config_node(MCC_CONFIG_AUTOCORRECT TYPE BOOL DEFAULT OFF GROUP "Build targets" ADVANCED
    HELP "Non-strict mode: auto-correct inert/non-runnable config combinations instead of only warning")

mcc_config_node(MCC_CONFIG_MINGW  TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Build a WIN32/mingw target (input: forces MCC_TARGETOS=WIN32)")
set(_mcc_bt_default OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_mcc_bt_default ON)
endif()
mcc_config_node(MCC_CONFIG_BACKTRACE TYPE BOOL DEFAULT ${_mcc_bt_default} GROUP "mcc features"
    HELP "Enable stack backtraces (-bt / -run); defaults ON for Debug builds")
mcc_config_node(MCC_CONFIG_BCHECK   TYPE BOOL DEFAULT ${_mcc_bt_default} GROUP "mcc features"
    HELP "Enable bounds checker (-b); defaults ON for Debug builds"
    VISIBLE_WHEN "MCC_CONFIG_BACKTRACE")
mcc_config_node(MCC_CONFIG_ASM      TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "Enable the integrated assembler (inline asm, global asm, .s files, asm labels)")
mcc_config_node(MCC_CONFIG_PREDEFS  TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "Compile mccdefs.h into the binary (c2str)")
mcc_config_node(MCC_CONFIG_PIE      TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Let mcc generate position independent executables"
    VISIBLE_WHEN "${_elf}")
mcc_config_node(MCC_CONFIG_PIC      TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Position independent code"
    VISIBLE_WHEN "${_elf}")
mcc_config_node(MCC_RUN_MMAP_EXEC    TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Allocate -run executable memory via mmap (separate RW/RX) instead of mprotect; needed on SELinux/PaX hardened (W^X) kernels")
mcc_config_node(MCC_CONFIG_NEW_DTAGS TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Use DT_RUNPATH instead of DT_RPATH (mcc-emitted; always relevant on ELF)"
    VISIBLE_WHEN "${_elf}")

mcc_config_node(MCC_AUTO_MCCDIR     TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "Build-tree mcc auto-discovers libmccrt.a + headers locally, falling back to system CONFIG_MCCDIR")

mcc_config_node(MCC_CONFIG_LIBC    TYPE STRING DEFAULT "" GROUP "mcc features"
    HELP "Target libc: '', uClibc, or musl" CHOICES uClibc musl EMPTY_OK
    VISIBLE_WHEN "${_elf}")
mcc_config_node(MCC_CONFIG_DWARF   TYPE STRING DEFAULT "" GROUP "mcc features"
    HELP "DWARF debug version (2..5); empty = stabs" CHOICES 0 2 3 4 5 EMPTY_OK)
mcc_config_node(MCC_CONFIG_SEMLOCK TYPE STRING DEFAULT "" GROUP "mcc features"
    HELP "CONFIG_MCC_SEMLOCK value (free-form numeric; empty = mcc.h default 1)")
mcc_config_node(MCC_CONFIG_NEW_MACHO TYPE STRING DEFAULT "" GROUP "Darwin"
    HELP "Force apple object format: yes|no|auto" CHOICES yes no auto EMPTY_OK
    VISIBLE_WHEN "${_darwin}")
mcc_config_node(MCC_CONFIG_CODESIGN TYPE STRING DEFAULT "" GROUP "Darwin"
    HELP "Use codesign on apple to sign executables: yes|no|auto" CHOICES yes no auto EMPTY_OK
    VISIBLE_WHEN "${_darwin}")

mcc_config_node(MCC_SYSROOT         TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --sysroot")
mcc_config_node(MCC_TRIPLET         TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --triplet")
mcc_config_node(MCC_SYSINCLUDEPATHS TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --sysincludepaths (colon sep)")
mcc_config_node(MCC_LIBPATHS        TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --libpaths (colon sep)")
mcc_config_node(MCC_CRTPREFIX       TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --crtprefix (colon sep)")
mcc_config_node(MCC_ELFINTERP       TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --elfinterp")
mcc_config_node(MCC_SWITCHES        TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --mcc-switches")
mcc_config_node(MCC_OS_RELEASE      TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --os-release")
mcc_config_node(MCC_EXTRA_CFLAGS  TYPE STRING DEFAULT "" GROUP "Build flags" ADVANCED
    HELP "configure --extra-cflags (space separated)")
mcc_config_node(MCC_EXTRA_LDFLAGS TYPE STRING DEFAULT "" GROUP "Build flags" ADVANCED
    HELP "configure --extra-ldflags (space separated)")
mcc_config_node(MCC_EXTRA_LIBS    TYPE STRING DEFAULT "" GROUP "Build flags" ADVANCED
    HELP "configure --extra-libs (space separated)")
if(NOT "${MCC_EXTRA_CFLAGS}" STREQUAL "")
    separate_arguments(_mcc_extra_cflags NATIVE_COMMAND "${MCC_EXTRA_CFLAGS}")
    add_compile_options(${_mcc_extra_cflags})
endif()
if(NOT "${MCC_EXTRA_LDFLAGS}" STREQUAL "")
    separate_arguments(_mcc_extra_ldflags NATIVE_COMMAND "${MCC_EXTRA_LDFLAGS}")
    add_link_options(${_mcc_extra_ldflags})
endif()

set(_proc "${CMAKE_SYSTEM_PROCESSOR}")
if(MCC_TARGET_ARCH)
    set(_proc "${MCC_TARGET_ARCH}")
endif()
set(_arm_cpuver "")
if(_proc MATCHES "^(x86|i[3-6]86|i86pc|BePC)$")
    set(MCC_CPU "i386")
elseif(_proc MATCHES "^(x86_64|amd64|x86-64|AMD64)$")
    set(MCC_CPU "x86_64")
elseif(_proc MATCHES "^(aarch64|arm64|ARM64)$")
    set(MCC_CPU "arm64")
elseif(_proc MATCHES "^arm")
    set(MCC_CPU "arm")
elseif(_proc MATCHES "^riscv64$")
    set(MCC_CPU "riscv64")
else()
    message(FATAL_ERROR "Unsupported CPU '${_proc}' (configure: Unsupported CPU)")
endif()

if(WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Windows|MINGW|MSYS|CYGWIN" OR MCC_CONFIG_MINGW)
    set(MCC_TARGETOS "WIN32")
    set(MCC_LIBSUF ".lib")
    set(MCC_EXESUF ".exe")
    set(MCC_DLLSUF ".dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MCC_TARGETOS "Darwin")
    set(MCC_LIBSUF ".a")
    set(MCC_EXESUF "")
    set(MCC_DLLSUF ".dylib")
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    set(MCC_TARGETOS "${CMAKE_SYSTEM_NAME}")
    set(MCC_LIBSUF ".a")
    set(MCC_EXESUF "")
    set(MCC_DLLSUF ".so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(MCC_TARGETOS "Android")
    set(MCC_LIBSUF ".a")
    set(MCC_EXESUF "")
    set(MCC_DLLSUF ".so")
else()
    set(MCC_TARGETOS "${CMAKE_SYSTEM_NAME}")
    set(MCC_LIBSUF ".a")
    set(MCC_EXESUF "")
    set(MCC_DLLSUF ".so")
endif()

if(MCC_TARGETOS STREQUAL "Darwin")
    if("${MCC_CONFIG_DWARF}" STREQUAL "")
        set(MCC_CONFIG_DWARF 4)
    endif()
    if("${MCC_CONFIG_CODESIGN}" STREQUAL "")
        set(MCC_CONFIG_CODESIGN yes)
    endif()
    if("${MCC_CONFIG_NEW_MACHO}" STREQUAL "" AND NOT CMAKE_CROSSCOMPILING)
        execute_process(COMMAND sw_vers -productVersion
            OUTPUT_VARIABLE _osxver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_osxver MATCHES "^([0-9]+)" AND CMAKE_MATCH_1 LESS 11)
            set(MCC_CONFIG_NEW_MACHO no)
        endif()
    endif()
elseif(MCC_TARGETOS MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    if("${MCC_ELFINTERP}" STREQUAL "")
        if(MCC_TARGETOS STREQUAL "FreeBSD")
            set(MCC_ELFINTERP "/libexec/ld-elf.so.1")
        elseif(MCC_TARGETOS STREQUAL "DragonFly")
            set(MCC_ELFINTERP "/usr/libexec/ld-elf.so.2")
        elseif(MCC_TARGETOS STREQUAL "NetBSD")
            set(MCC_ELFINTERP "/usr/libexec/ld.elf_so")
        elseif(MCC_TARGETOS STREQUAL "OpenBSD")
            set(MCC_ELFINTERP "/usr/libexec/ld.so")
        endif()
    endif()
elseif(MCC_TARGETOS STREQUAL "Android")
    if(DEFINED ENV{TERMUX_VERSION})
        set(_android_sysroot "/data/data/com.termux/files/usr")
    else()
        set(_android_sysroot "/usr")
    endif()
    if("${MCC_SYSROOT}" STREQUAL "")
        set(MCC_SYSROOT "${_android_sysroot}")
    endif()
    set(MCC_CONFIG_NEW_DTAGS ON)
    set(MCC_ENABLE_RPATH OFF)
    if("${MCC_CONFIG_DWARF}" STREQUAL "")
        set(MCC_CONFIG_DWARF 4)
    endif()
    if(NOT MCC_CPU STREQUAL "i386")
        set(MCC_CONFIG_PIE ON)
    endif()
    set(MCC_BUILD_STATIC_LIB OFF)
    if("${MCC_TRIPLET}" STREQUAL "")
        if(MCC_CPU STREQUAL "arm")
            set(MCC_TRIPLET "arm-linux-androideabi")
            set(_arm_cpuver 7)
        elseif(MCC_CPU STREQUAL "arm64")
            set(MCC_TRIPLET "aarch64-linux-android")
        elseif(MCC_CPU STREQUAL "x86_64")
            set(MCC_TRIPLET "x86_64-linux-android")
        elseif(MCC_CPU STREQUAL "i386")
            set(MCC_TRIPLET "i686-linux-android")
        endif()
    endif()
    if(MCC_CPU MATCHES "64$")
        set(_android_s "64")
    else()
        set(_android_s "")
    endif()
    if("${MCC_SYSINCLUDEPATHS}" STREQUAL "")
        set(MCC_SYSINCLUDEPATHS "{B}/include:{R}/include:{R}/include/${MCC_TRIPLET}")
    endif()
    if("${MCC_LIBPATHS}" STREQUAL "")
        set(MCC_LIBPATHS "{B}:{R}/lib:/system/lib${_android_s}")
    endif()
    if("${MCC_CRTPREFIX}" STREQUAL "")
        set(MCC_CRTPREFIX "{R}/lib")
    endif()
    if("${MCC_ELFINTERP}" STREQUAL "")
        set(MCC_ELFINTERP "/system/bin/linker${_android_s}")
    endif()
    if("${MCC_SWITCHES}" STREQUAL "")
        set(MCC_SWITCHES "-Wl,-rpath=${MCC_SYSROOT}/lib")
    endif()
endif()

if("${MCC_OS_RELEASE}" STREQUAL "" AND NOT CMAKE_CROSSCOMPILING)
    set(MCC_OS_RELEASE "${CMAKE_HOST_SYSTEM_VERSION}")
endif()

if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    set(MCC_BIGENDIAN ON)
else()
    set(MCC_BIGENDIAN OFF)
endif()

message(STATUS "mcc target: CPU=${MCC_CPU} OS=${MCC_TARGETOS} bigendian=${MCC_BIGENDIAN}")

set(_mcc_cpu_define_i386    MCC_TARGET_I386)
set(_mcc_cpu_define_x86_64  MCC_TARGET_X86_64)
set(_mcc_cpu_define_arm     MCC_TARGET_ARM)
set(_mcc_cpu_define_arm64   MCC_TARGET_ARM64)
set(_mcc_cpu_define_riscv64 MCC_TARGET_RISCV64)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(MCC_CC_NAME clang)
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    set(MCC_CC_NAME msvc)
elseif(CMAKE_C_COMPILER_ID MATCHES "ModernCC|TinyCC")
    set(MCC_CC_NAME mcc)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(MCC_CC_NAME gcc)
else()
    set(MCC_CC_NAME unknown)
endif()
if("${CMAKE_C_COMPILER_VERSION}" MATCHES "^([0-9]+)\\.([0-9]+)")
    set(MCC_GCC_MAJOR "${CMAKE_MATCH_1}")
    set(MCC_GCC_MINOR "${CMAKE_MATCH_2}")
else()
    set(MCC_GCC_MAJOR 0)
    set(MCC_GCC_MINOR 0)
endif()

set(_arm_eabi OFF)
set(_arm_vfp OFF)
set(_arm_hardfloat OFF)
set(_arm_idiv OFF)
if(MCC_CPU STREQUAL "arm" AND NOT _arm_cpuver)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_empty.c" "")
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -dM -E -
        INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/_empty.c"
        OUTPUT_VARIABLE _cc_defs ERROR_QUIET RESULT_VARIABLE _cc_defs_rc)
    if(_cc_defs_rc EQUAL 0)
        if(_cc_defs MATCHES "__ARM_ARCH[ \t]+([0-9]+)")
            set(_arm_cpuver "${CMAKE_MATCH_1}")
        endif()
        if(_cc_defs MATCHES "__ARM_EABI__")
            set(_arm_eabi ON)
        endif()
        if(_cc_defs MATCHES "(__VFP_FP__|__ARM_FP[ \t])")
            set(_arm_vfp ON)
        endif()
        if(_cc_defs MATCHES "__ARM_PCS_VFP")
            set(_arm_hardfloat ON)
        endif()
        if(_cc_defs MATCHES "__ARM_FEATURE_IDIV")
            set(_arm_idiv ON)
        endif()
    endif()
endif()

mcc_profile_seed(MCC_ARM_EABI      "${_arm_eabi}")
mcc_profile_seed(MCC_ARM_VFP       "${_arm_vfp}")
mcc_profile_seed(MCC_ARM_HARDFLOAT "${_arm_hardfloat}")
mcc_profile_seed(MCC_ARM_IDIV      "${_arm_idiv}")
mcc_profile_seed(MCC_CPUVER        "${_arm_cpuver}")
mcc_config_node(MCC_ARM_EABI TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "MCC_CPU STREQUAL arm"
    HELP "ARM EABI calling convention (MCC_ARM_EABI); autodetected from the host cc")
mcc_config_node(MCC_ARM_VFP TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "MCC_CPU STREQUAL arm"
    HELP "ARM VFP floating-point unit (MCC_ARM_VFP); autodetected from the host cc")
mcc_config_node(MCC_ARM_HARDFLOAT TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "MCC_CPU STREQUAL arm"
    HELP "ARM hard-float PCS (MCC_ARM_HARDFLOAT); autodetected from the host cc")
mcc_config_node(MCC_ARM_IDIV TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "MCC_CPU STREQUAL arm"
    HELP "ARM hardware integer divide (__ARM_FEATURE_IDIV); autodetected from the host cc")
mcc_config_node(MCC_CPUVER TYPE STRING DEFAULT "" EMPTY_OK GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "MCC_CPU STREQUAL arm"
    HELP "ARM architecture version 4/5/6/7 (CONFIG_MCC_CPUVER); autodetected (__ARM_ARCH)")

if(NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT CMAKE_CROSSCOMPILING)
    if("${MCC_TRIPLET}" STREQUAL "")
        execute_process(COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
            OUTPUT_VARIABLE _dumpmachine OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

        set(_triplet_cands "")
        if(_dumpmachine)
            list(APPEND _triplet_cands "${_dumpmachine}")
            if(_dumpmachine MATCHES "^([^-]+)-(pc|unknown)-(.+)$")
                list(APPEND _triplet_cands "${CMAKE_MATCH_1}-${CMAKE_MATCH_3}")
            endif()
        endif()
        foreach(_cand IN LISTS _triplet_cands)
            if(EXISTS "/usr/lib/${_cand}/crti.o" OR IS_DIRECTORY "/usr/include/${_cand}")
                set(MCC_TRIPLET "${_cand}")
                break()
            endif()
        endforeach()
    endif()

    message(STATUS "mcc paths: triplet='${MCC_TRIPLET}' libpaths='${MCC_LIBPATHS}' crtprefix='${MCC_CRTPREFIX}'")
    if("${MCC_CONFIG_LIBC}" STREQUAL "")
        if(EXISTS "/lib/ld-uClibc.so.0")
            message(STATUS "Perhaps you want -DMCC_CONFIG_LIBC=uClibc")
        endif()
        if(EXISTS "/lib/ld-musl-${MCC_CPU}.so.1")
            message(STATUS "Perhaps you want -DMCC_CONFIG_LIBC=musl")
        endif()
    endif()
endif()

include(CheckCCompilerFlag)
if(NOT MCC_CC_NAME STREQUAL "mcc" AND NOT MSVC)
    if(NOT MCC_CC_NAME STREQUAL "clang")
        check_c_compiler_flag("-Wno-unused-result" MCC_HAS_WNO_UNUSED_RESULT)
        if(MCC_HAS_WNO_UNUSED_RESULT)
            add_compile_options(-Wno-unused-result)
        endif()
    endif()
endif()
if(MCC_CC_NAME STREQUAL "gcc" AND MCC_GCC_MAJOR EQUAL 4
   AND NOT MCC_GCC_MINOR MATCHES "^[5-9]$")
    add_compile_options(-D_FORTIFY_SOURCE=0)
endif()

set(MCC_DIAG_FLAGS "")
if(MCC_ALL_DIAGNOSTICS)
    if(MSVC)

        foreach(_f /W4 /Zi)
            string(MAKE_C_IDENTIFIER "MCC_HAS_DIAGFLAG_${_f}" _v)
            check_c_compiler_flag("${_f}" ${_v})
            if(${_v})
                list(APPEND MCC_DIAG_FLAGS "${_f}")
            endif()
        endforeach()
    else()

        foreach(_f -Wall -Wextra -Wnull-dereference
                   -Wno-sign-compare -Wno-unused-parameter -Wno-type-limits
                   -Wno-unterminated-string-initialization -Wno-implicit-fallthrough
                   -g3 -fno-omit-frame-pointer)
            string(MAKE_C_IDENTIFIER "MCC_HAS_DIAGFLAG_${_f}" _v)
            check_c_compiler_flag("${_f}" ${_v})
            if(${_v})
                list(APPEND MCC_DIAG_FLAGS "${_f}")
            endif()
        endforeach()
    endif()
    message(STATUS "mcc diagnostics: core warning/debug flags =${MCC_DIAG_FLAGS}")
endif()

if(MCC_TARGETOS STREQUAL "Darwin" AND NOT MCC_CC_NAME STREQUAL "mcc")
    add_link_options(-flat_namespace)
    if(MCC_CC_NAME STREQUAL "clang" AND MCC_GCC_MAJOR LESS 15)
        add_link_options(-undefined warning)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6" CACHE STRING
            "Makefile: MACOSX_DEPLOYMENT_TARGET" FORCE)
    endif()
    if(MCC_CPU STREQUAL "x86_64" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        add_compile_options(-arch x86_64)
        add_link_options(-arch x86_64)
    endif()
endif()

if(MCC_TARGETOS STREQUAL "WIN32")
    if(MCC_CC_NAME STREQUAL "gcc")
        add_link_options(-static)
    endif()
    if(NOT MCC_CC_NAME STREQUAL "clang" AND NOT MCC_CC_NAME STREQUAL "msvc"
       AND NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")

        add_link_options(-s)
    endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${MCC_DIST_DIR}"
        CACHE PATH "Install prefix (default: the shared dist/ staging tree)" FORCE)
    message(STATUS "mcc install prefix: defaulted to the shared dist tree '${CMAKE_INSTALL_PREFIX}' "
                   "(pass -DCMAKE_INSTALL_PREFIX=/usr/local for a system install)")
endif()

include(GNUInstallDirs)
mcc_config_node(MCC_INSTALL_MCCDIR TYPE STRING DEFAULT "" GROUP "Install" ADVANCED
    HELP "Runtime mccdir (CONFIG_MCCDIR); empty = <prefix>/<libdir>/mcc")

if(MCC_CONFIG_PREDEFS)
    set(_mcc_predefs_value 1)
else()
    set(_mcc_predefs_value 0)
endif()

mcc_status_node(MCC_CPU         "${MCC_CPU}")
mcc_status_node(MCC_TARGETOS    "${MCC_TARGETOS}")
mcc_status_node(host_compiler   "${MCC_CC_NAME} (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION})")
mcc_status_node(cross_compiling "${CMAKE_CROSSCOMPILING}")

set(MCC_EMULATOR "${CMAKE_CROSSCOMPILING_EMULATOR}")
if(CMAKE_CROSSCOMPILING)
    mcc_status_node(emulator "${MCC_EMULATOR}")
endif()

if(MCC_CPU STREQUAL "arm")
    mcc_status_node(arm_abi "eabi=${MCC_ARM_EABI} vfp=${MCC_ARM_VFP} hardfloat=${MCC_ARM_HARDFLOAT} idiv=${MCC_ARM_IDIV} cpuver=${MCC_CPUVER}")
endif()

mcc_apply_node_visibility()
mcc_validate_config()
mcc_report_config()
if(MCC_REGEN_DOCS)
    mcc_generate_node_doc("${CMAKE_BINARY_DIR}/config-nodes.md")
endif()
mcc_write_node_manifest("${CMAKE_BINARY_DIR}/config-nodes.tsv")

macro(mcc_def_str _var _name _val)
    if(NOT "${_val}" STREQUAL "")
        list(APPEND ${_var} "${_name}=\"${_val}\"")
    endif()
endmacro()

set(_mccdefs "${_mcc_cpu_define_${MCC_CPU}}=1")

if(MCC_CPU STREQUAL "arm")
    if(MCC_CPUVER)
        list(APPEND _mccdefs "CONFIG_MCC_CPUVER=${MCC_CPUVER}")
    endif()
    if(MCC_ARM_EABI)
        list(APPEND _mccdefs "MCC_ARM_EABI=1")
    endif()
    if(MCC_ARM_VFP)
        list(APPEND _mccdefs "MCC_ARM_VFP=1")
    endif()
    if(MCC_ARM_HARDFLOAT)
        list(APPEND _mccdefs "MCC_ARM_HARDFLOAT=1")
    endif()
    if(MCC_ARM_IDIV)
        list(APPEND _mccdefs "__ARM_FEATURE_IDIV=1")
    endif()
endif()

if(MCC_TARGETOS STREQUAL "WIN32")
    list(APPEND _mccdefs "MCC_TARGET_PE=1")
elseif(MCC_TARGETOS STREQUAL "Darwin")
    list(APPEND _mccdefs "MCC_TARGET_MACHO=1")
elseif(MCC_TARGETOS STREQUAL "Android")
    list(APPEND _mccdefs "TARGETOS_ANDROID=1")
elseif(MCC_TARGETOS MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    list(APPEND _mccdefs "TARGETOS_${MCC_TARGETOS}=1")
endif()

if(MCC_CONFIG_LIBC STREQUAL "" AND NOT MSVC AND NOT CMAKE_CROSSCOMPILING
   AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TARGETOS STREQUAL "Darwin")
    execute_process(COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
        OUTPUT_VARIABLE _host_mach ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_host_mach MATCHES "musl")
        set(MCC_CONFIG_LIBC "musl")
        message(STATUS "mcc: musl host toolchain (${_host_mach}); defaulting MCC_CONFIG_LIBC=musl")
    endif()
endif()
if(MCC_CONFIG_LIBC STREQUAL "musl" AND MCC_ELFINTERP STREQUAL "")
    if(MCC_CPU STREQUAL "arm64")
        set(_muslarch aarch64)
    elseif(MCC_CPU STREQUAL "arm")
        if(MCC_ARM_HARDFLOAT)
            set(_muslarch armhf)
        else()
            set(_muslarch arm)
        endif()
    else()
        set(_muslarch "${MCC_CPU}")
    endif()
    set(MCC_ELFINTERP "/lib/ld-musl-${_muslarch}.so.1")
endif()

if(MCC_CONFIG_LIBC STREQUAL "musl")
    list(APPEND _mccdefs "CONFIG_MCC_MUSL=1")
elseif(MCC_CONFIG_LIBC STREQUAL "uClibc")
    list(APPEND _mccdefs "CONFIG_MCC_UCLIBC=1")
endif()

if(MCC_RUN_MMAP_EXEC)
    list(APPEND _mccdefs "CONFIG_RUN_MMAP_EXEC=1")
endif()
if(MCC_CONFIG_PIE)
    list(APPEND _mccdefs "CONFIG_MCC_PIE=1")
endif()
if(MCC_CONFIG_PIC)
    list(APPEND _mccdefs "CONFIG_MCC_PIC=1")
endif()
if(MCC_CONFIG_NEW_DTAGS)
    list(APPEND _mccdefs "CONFIG_NEW_DTAGS=1")
endif()
if(MCC_CONFIG_CODESIGN)
    list(APPEND _mccdefs "CONFIG_CODESIGN=1")
endif()

if(NOT MCC_CONFIG_BCHECK)
    list(APPEND _mccdefs "CONFIG_MCC_BCHECK=0")
endif()
if(NOT MCC_CONFIG_ASM)
    list(APPEND _mccdefs "CONFIG_MCC_ASM=0")
endif()
if(NOT MCC_CONFIG_BACKTRACE)
    list(APPEND _mccdefs "CONFIG_MCC_BACKTRACE=0")
endif()
if(MCC_CST)
    list(APPEND _mccdefs "CONFIG_MCC_CST=1")
endif()
if(MCC_AST)
    list(APPEND _mccdefs "CONFIG_AST=1")
endif()
if(MCC_CONFIG_NEW_MACHO STREQUAL "no")
    list(APPEND _mccdefs "CONFIG_NEW_MACHO=0")
endif()

if(NOT "${MCC_CONFIG_DWARF}" STREQUAL "")
    list(APPEND _mccdefs "CONFIG_DWARF_VERSION=${MCC_CONFIG_DWARF}")
endif()
if(NOT "${MCC_CONFIG_SEMLOCK}" STREQUAL "")
    list(APPEND _mccdefs "CONFIG_MCC_SEMLOCK=${MCC_CONFIG_SEMLOCK}")
endif()

mcc_def_str(_mccdefs CONFIG_MCC_SYSINCLUDEPATHS "${MCC_SYSINCLUDEPATHS}")
mcc_def_str(_mccdefs CONFIG_MCC_LIBPATHS        "${MCC_LIBPATHS}")
mcc_def_str(_mccdefs CONFIG_MCC_CRTPREFIX       "${MCC_CRTPREFIX}")
mcc_def_str(_mccdefs CONFIG_MCC_ELFINTERP       "${MCC_ELFINTERP}")
mcc_def_str(_mccdefs CONFIG_MCC_SWITCHES        "${MCC_SWITCHES}")
mcc_def_str(_mccdefs CONFIG_TRIPLET             "${MCC_TRIPLET}")
mcc_def_str(_mccdefs CONFIG_OS_RELEASE          "${MCC_OS_RELEASE}")

if(NOT "${MCC_SYSROOT}" STREQUAL "")
    list(APPEND _mccdefs "CONFIG_SYSROOT=\"${MCC_SYSROOT}\"")
endif()
if(NOT MCC_TARGETOS STREQUAL "WIN32")
    if("${MCC_INSTALL_MCCDIR}" STREQUAL "")
        set(_mccdir "${CMAKE_INSTALL_FULL_LIBDIR}/mcc")
    else()
        set(_mccdir "${MCC_INSTALL_MCCDIR}")
    endif()
    list(APPEND _mccdefs "CONFIG_MCCDIR=\"${_mccdir}\"")
endif()
set(MCC_RESOLVED_MCCDIR "${_mccdir}")

string(REPLACE ";" "\n" _mccdefs_text "${_mccdefs}")
file(WRITE "${CMAKE_BINARY_DIR}/config-defines.txt" "${_mccdefs_text}\n")

set(_mccdefs_common
    "MCC_VERSION=${MCC_VERSION}"
    "CC_NAME=CC_${MCC_CC_NAME}"
    "GCC_MAJOR=${MCC_GCC_MAJOR}"
    "GCC_MINOR=${MCC_GCC_MINOR}"
    "CONFIG_MCC_PREDEFS=${_mcc_predefs_value}")
add_compile_definitions(${_mccdefs_common})

set(_mccdefs_flags "")
foreach(_d IN LISTS _mccdefs)
    if(NOT _d MATCHES "\"")
        list(APPEND _mccdefs_flags "-D${_d}")
    endif()
endforeach()

message(STATUS "mcc build defines: CC_NAME=CC_${MCC_CC_NAME} predefs=${_mcc_predefs_value} "
               "bcheck=${MCC_CONFIG_BCHECK} backtrace=${MCC_CONFIG_BACKTRACE}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm64"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/riscv64"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/formats"
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MCC_AUTO_MCCDIR)
    add_compile_definitions(CONFIG_MCC_AUTO_MCCDIR=1)
endif()

find_package(Git QUIET)
set(MCC_GITHASH "")
if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_branch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_git_branch)
        execute_process(COMMAND "${GIT_EXECUTABLE}" log -1 --date=short
            "--pretty=format:%cd ${_git_branch}@%h"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE MCC_GITHASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND "${GIT_EXECUTABLE}" diff --quiet
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESULT_VARIABLE _git_dirty)
        if(_git_dirty)
            set(MCC_GITHASH "${MCC_GITHASH}*")
        endif()
    endif()
endif()
if(MCC_GITHASH)
    message(STATUS "mcc githash: ${MCC_GITHASH}")
endif()

if(MCC_CONFIG_PREDEFS)
    if(CMAKE_CROSSCOMPILING)
        find_program(MCC_HOSMCC
            NAMES "$ENV{CC_FOR_BUILD}" "$ENV{HOSMCC}" cc gcc clang
            DOC "Host C compiler used to build the c2str codegen tool")
        if(NOT MCC_HOSMCC)
            message(FATAL_ERROR
                "MCC_CONFIG_PREDEFS with CMAKE_CROSSCOMPILING needs a host C compiler "
                "to build c2str. Set -DMCC_HOSMCC=<cc> (or CC_FOR_BUILD), or pass "
                "-DMCC_CONFIG_PREDEFS=OFF to load <mccdefs.h> at runtime instead.")
        endif()
        set(_c2str_exe "${CMAKE_CURRENT_BINARY_DIR}/c2str_host")
        execute_process(
            COMMAND "${MCC_HOSMCC}"
                    -o "${_c2str_exe}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/c2str.c"
            RESULT_VARIABLE _c2str_rc)
        if(NOT _c2str_rc EQUAL 0)
            message(FATAL_ERROR "Host compiler '${MCC_HOSMCC}' failed to build c2str")
        endif()
        set(_c2str_dep "${_c2str_exe}")
    else()
        add_executable(c2str tools/c2str.c)
        set(_c2str_exe "$<TARGET_FILE:c2str>")
        set(_c2str_dep c2str)
    endif()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mccdefs_.h"
        COMMAND "${_c2str_exe}"
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/mccdefs.h"
                "${CMAKE_CURRENT_BINARY_DIR}/mccdefs_.h"
        DEPENDS ${_c2str_dep} "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/mccdefs.h"
        COMMENT "Generating mccdefs_.h (c2str include/mccdefs.h)"
        VERBATIM)
    add_custom_target(mcc_mccdefs
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/mccdefs_.h")
endif()

file(GLOB MCC_CORE_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/mccelf.c")
list(REMOVE_ITEM MCC_CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c")

file(GLOB MCC_BACKEND_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/${MCC_CPU}/*.c")
if(NOT MCC_BACKEND_SRC)
    message(FATAL_ERROR "No backend sources for CPU '${MCC_CPU}' (no src/arch/${MCC_CPU}/ directory)")
endif()
if(MCC_CPU STREQUAL "x86_64")
    list(APPEND MCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386/i386-asm.c")
endif()

if(MCC_TARGETOS STREQUAL "WIN32")
    list(APPEND MCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/mccpe.c")
elseif(MCC_TARGETOS STREQUAL "Darwin")
    list(APPEND MCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/mccmacho.c")
endif()

function(mcc_musl_target cpu armhf out_interp out_triplet)
    if(cpu STREQUAL "x86_64")
        set(_i "/lib/ld-musl-x86_64.so.1")
        set(_t "x86_64-linux-musl")
    elseif(cpu STREQUAL "i386")
        set(_i "/lib/ld-musl-i386.so.1")
        set(_t "i386-linux-musl")
    elseif(cpu STREQUAL "arm64")
        set(_i "/lib/ld-musl-aarch64.so.1")
        set(_t "aarch64-linux-musl")
    elseif(cpu STREQUAL "riscv64")
        set(_i "/lib/ld-musl-riscv64.so.1")
        set(_t "riscv64-linux-musl")
    elseif(cpu STREQUAL "arm")
        if(armhf)
            set(_i "/lib/ld-musl-armhf.so.1")
            set(_t "arm-linux-musleabihf")
        else()
            set(_i "/lib/ld-musl-arm.so.1")
            set(_t "arm-linux-musleabi")
        endif()
    else()
        set(_i "")
        set(_t "")
    endif()
    set(${out_interp} "${_i}" PARENT_SCOPE)
    set(${out_triplet} "${_t}" PARENT_SCOPE)
endfunction()

function(mcc_libmcc_output kind is_musl outvar)
    set(_n "mcc")
    if(kind STREQUAL "STATIC")
        set(_n "${_n}-static")
    elseif(MCC_BUILD_STATIC_LIB)
        set(_n "${_n}-dynamic")
    endif()
    if(is_musl)
        set(_n "${_n}-musl")
    endif()
    set(${outvar} "${_n}" PARENT_SCOPE)
endfunction()

function(mcc_add_libmcc tgt kind is_musl)
    add_library(${tgt} ${kind} ${LIBMCC_SRC})
    if(kind STREQUAL "SHARED")
        target_compile_definitions(${tgt} PRIVATE LIBMCC_AS_DLL)
        set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        if(APPLE)
            set_target_properties(${tgt} PROPERTIES
                MACOSX_RPATH ON VERSION "${MCC_VERSION_NUMERIC}")
        endif()
    elseif(MCC_CC_NAME STREQUAL "clang" AND MCC_CPU STREQUAL "x86_64")
        set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    mcc_libmcc_output("${kind}" "${is_musl}" _oname)
    set_target_properties(${tgt} PROPERTIES OUTPUT_NAME "${_oname}" PREFIX "lib")
    if(NOT MCC_SINGLE_SOURCE)
        target_compile_definitions(${tgt} PRIVATE SINGLE_SOURCE=0)
    else()
        set_source_files_properties(src/libmcc.c PROPERTIES OBJECT_DEPENDS "${_one_src_deps}")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(${tgt} mcc_mccdefs)
    endif()
    target_include_directories(${tgt} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    if(NOT MCC_TARGETOS STREQUAL "WIN32")
        target_link_libraries(${tgt} PUBLIC m)
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(${tgt} PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
        endif()
    endif()
    if(_mcc_extra_libs)
        target_link_libraries(${tgt} PUBLIC ${_mcc_extra_libs})
    endif()
    if(MCC_ALL_DIAGNOSTICS)
        target_compile_options(${tgt} PRIVATE ${MCC_DIAG_FLAGS})
    endif()
    if(ARGN)
        target_compile_definitions(${tgt} PRIVATE ${ARGN})
    endif()
endfunction()

set(LIBMCC_ALL_SRC ${MCC_CORE_SRC} ${MCC_BACKEND_SRC})
list(REMOVE_ITEM LIBMCC_ALL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")

if(MCC_SINGLE_SOURCE)
    set(LIBMCC_SRC src/libmcc.c)
else()
    set(LIBMCC_SRC ${LIBMCC_ALL_SRC})
endif()

set(_one_src_deps "")
if(MCC_SINGLE_SOURCE)
    foreach(_s IN LISTS LIBMCC_ALL_SRC)
        if(NOT _s STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/src/libmcc.c")
            list(APPEND _one_src_deps "${_s}")
        endif()
    endforeach()
endif()
set(_mcc_extra_libs "")
if(NOT "${MCC_EXTRA_LIBS}" STREQUAL "")
    separate_arguments(_mcc_extra_libs NATIVE_COMMAND "${MCC_EXTRA_LIBS}")
endif()

if(MCC_BUILD_STATIC_LIB)
    mcc_add_libmcc(libmcc STATIC FALSE)
else()
    mcc_add_libmcc(libmcc SHARED FALSE)
endif()

if(MCC_BUILD_STATIC_LIB)
    set(_libmcc_shared "")
else()
    set(_libmcc_shared libmcc)
endif()
if(MCC_BUILD_STATIC_LIB AND MCC_BUILD_DYNAMIC_LIB)
    mcc_add_libmcc(libmcc_shared SHARED FALSE)
    set(_libmcc_shared libmcc_shared)
endif()

add_executable(mcc src/mcc.c)
set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
target_include_directories(mcc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT MCC_TARGETOS STREQUAL "WIN32")
    target_link_libraries(mcc PRIVATE m)
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(mcc PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
endif()
if(NOT "${MCC_EXTRA_LIBS}" STREQUAL "")
    target_link_libraries(mcc PRIVATE ${_mcc_extra_libs})
endif()
if(MCC_GITHASH)
    target_compile_definitions(mcc PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
endif()
if(TARGET mcc_mccdefs)
    add_dependencies(mcc mcc_mccdefs)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MCC_STRIP_LDFLAG -Wl,-x)
else()
    set(MCC_STRIP_LDFLAG -s)
endif()
if(MCC_BUILD_STRIP AND NOT MCC_CC_NAME STREQUAL "msvc")
    target_link_options(mcc PRIVATE ${MCC_STRIP_LDFLAG})
endif()
if(MCC_ALL_DIAGNOSTICS)
    target_compile_options(mcc PRIVATE ${MCC_DIAG_FLAGS})
endif()

if(MCC_BUILD_STATIC_EXE)
    add_executable(mcc_static src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    set_target_properties(mcc_static PROPERTIES OUTPUT_NAME mcc-static)
    target_include_directories(mcc_static PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT MCC_SINGLE_SOURCE)
        target_compile_definitions(mcc_static PRIVATE SINGLE_SOURCE=0)
        target_link_libraries(mcc_static PRIVATE libmcc)
    endif()
    if(NOT MCC_TARGETOS STREQUAL "WIN32")
        target_link_libraries(mcc_static PRIVATE m)
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(mcc_static PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
        endif()
    endif()
    if(NOT "${MCC_EXTRA_LIBS}" STREQUAL "")
        target_link_libraries(mcc_static PRIVATE ${_mcc_extra_libs})
    endif()
    if(NOT MCC_CC_NAME STREQUAL "msvc")
        target_link_options(mcc_static PRIVATE -static)
        if(MCC_BUILD_STRIP)
            target_link_options(mcc_static PRIVATE ${MCC_STRIP_LDFLAG})
        endif()
    endif()
    if(MCC_GITHASH)
        target_compile_definitions(mcc_static PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(mcc_static mcc_mccdefs)
    endif()
    if(MCC_ALL_DIAGNOSTICS)
        target_compile_options(mcc_static PRIVATE ${MCC_DIAG_FLAGS})
    endif()
endif()

if(MCC_BUILD_DYNAMIC_EXE)
    add_executable(mcc_dynamic src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    set_target_properties(mcc_dynamic PROPERTIES OUTPUT_NAME mcc-dynamic)
    target_include_directories(mcc_dynamic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT MCC_TARGETOS STREQUAL "WIN32")
        target_link_libraries(mcc_dynamic PRIVATE m)
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(mcc_dynamic PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
        endif()
    endif()
    if(NOT "${MCC_EXTRA_LIBS}" STREQUAL "")
        target_link_libraries(mcc_dynamic PRIVATE ${_mcc_extra_libs})
    endif()
    if(MCC_GITHASH)
        target_compile_definitions(mcc_dynamic PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(mcc_dynamic mcc_mccdefs)
    endif()
    if(MCC_BUILD_STRIP AND NOT MCC_CC_NAME STREQUAL "msvc")
        target_link_options(mcc_dynamic PRIVATE ${MCC_STRIP_LDFLAG})
    endif()
    if(MCC_ALL_DIAGNOSTICS)
        target_compile_options(mcc_dynamic PRIVATE ${MCC_DIAG_FLAGS})
    endif()
endif()

if(MCC_BUILD_MUSL AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT APPLE)
    mcc_musl_target("${MCC_CPU}" "${MCC_ARM_HARDFLOAT}" _mi _mt)

    set(_musl_vendored OFF)
    if(NOT MCC_MUSL_SYSROOT STREQUAL "" AND NOT _mi STREQUAL "")
        set(_musl_vendored ON)
        get_filename_component(_musl_loader "${_mi}" NAME)
        set(_mi "${MCC_MUSL_SYSROOT}/lib/${_musl_loader}")
    endif()

    set(_mccdefs_musl "")
    foreach(_d IN LISTS _mccdefs)
        if(_d MATCHES "^CONFIG_MCC_ELFINTERP="
           OR _d MATCHES "^CONFIG_TRIPLET="
           OR _d MATCHES "^CONFIG_MCC_MUSL=")
        elseif(_musl_vendored AND (_d MATCHES "^CONFIG_SYSROOT="
               OR _d MATCHES "^CONFIG_MCC_SYSINCLUDEPATHS="
               OR _d MATCHES "^CONFIG_MCC_LIBPATHS="
               OR _d MATCHES "^CONFIG_MCC_CRTPREFIX="))
        else()
            list(APPEND _mccdefs_musl "${_d}")
        endif()
    endforeach()
    list(APPEND _mccdefs_musl "CONFIG_MCC_MUSL=1")
    if(NOT _mi STREQUAL "")
        list(APPEND _mccdefs_musl "CONFIG_MCC_ELFINTERP=\"${_mi}\"")
    endif()
    if(NOT _mt STREQUAL "")
        list(APPEND _mccdefs_musl "CONFIG_TRIPLET=\"${_mt}\"")
    endif()
    if(_musl_vendored)
        list(APPEND _mccdefs_musl
            "CONFIG_SYSROOT=\"${MCC_MUSL_SYSROOT}\""
            "CONFIG_MCC_SYSINCLUDEPATHS=\"{B}/include:{R}/include\""
            "CONFIG_MCC_LIBPATHS=\"{B}:{R}/lib\""
            "CONFIG_MCC_CRTPREFIX=\"{R}/lib\"")
        message(STATUS "mcc: musl variants target vendored sysroot ${MCC_MUSL_SYSROOT} (build it: cmake --build . --target musl-sysroot)")
    endif()

    if(MCC_BUILD_STATIC_LIB)
        mcc_add_libmcc(libmcc_musl STATIC TRUE ${_mccdefs_musl})
    else()
        mcc_add_libmcc(libmcc_musl SHARED TRUE ${_mccdefs_musl})
    endif()
    set(_libmcc_musl libmcc_musl)
    if(MCC_BUILD_STATIC_LIB)
        set(_libmcc_musl_shared "")
    else()
        set(_libmcc_musl_shared libmcc_musl)
    endif()
    if(MCC_BUILD_STATIC_LIB AND MCC_BUILD_DYNAMIC_LIB)
        mcc_add_libmcc(libmcc_musl_shared SHARED TRUE ${_mccdefs_musl})
        set(_libmcc_musl_shared libmcc_musl_shared)
    endif()

    add_executable(mcc_musl src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    target_compile_definitions(mcc_musl PRIVATE ${_mccdefs_musl})
    target_include_directories(mcc_musl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(mcc_musl PROPERTIES OUTPUT_NAME mcc-musl)
    target_link_libraries(mcc_musl PRIVATE m)
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(mcc_musl PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    if(MCC_GITHASH)
        target_compile_definitions(mcc_musl PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(mcc_musl mcc_mccdefs)
    endif()
    if(MCC_BUILD_STRIP AND NOT MCC_CC_NAME STREQUAL "msvc")
        target_link_options(mcc_musl PRIVATE ${MCC_STRIP_LDFLAG})
    endif()
    install(TARGETS mcc_musl RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(_musl_vendored)
        add_dependencies(mcc_musl musl-sysroot)
    endif()

    if(MCC_BUILD_STATIC_EXE)
        add_executable(mcc_static_musl src/mcc.c)
        set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
        target_compile_definitions(mcc_static_musl PRIVATE ${_mccdefs_musl})
        target_include_directories(mcc_static_musl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        set_target_properties(mcc_static_musl PROPERTIES OUTPUT_NAME mcc-static-musl)
        if(NOT MCC_SINGLE_SOURCE)
            target_compile_definitions(mcc_static_musl PRIVATE SINGLE_SOURCE=0)
            target_link_libraries(mcc_static_musl PRIVATE ${_libmcc_musl})
        endif()
        target_link_libraries(mcc_static_musl PRIVATE m)
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(mcc_static_musl PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
        endif()
        if(NOT MCC_CC_NAME STREQUAL "msvc")
            target_link_options(mcc_static_musl PRIVATE -static)
            if(MCC_BUILD_STRIP)
                target_link_options(mcc_static_musl PRIVATE ${MCC_STRIP_LDFLAG})
            endif()
        endif()
        if(MCC_GITHASH)
            target_compile_definitions(mcc_static_musl PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
        endif()
        if(TARGET mcc_mccdefs)
            add_dependencies(mcc_static_musl mcc_mccdefs)
        endif()
        install(TARGETS mcc_static_musl RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        if(_musl_vendored)
            add_dependencies(mcc_static_musl musl-sysroot)
        endif()
    endif()

    if(MCC_BUILD_DYNAMIC_EXE)
        add_executable(mcc_dynamic_musl src/mcc.c)
        set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
        target_compile_definitions(mcc_dynamic_musl PRIVATE ${_mccdefs_musl})
        target_include_directories(mcc_dynamic_musl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        set_target_properties(mcc_dynamic_musl PROPERTIES OUTPUT_NAME mcc-dynamic-musl)
        target_link_libraries(mcc_dynamic_musl PRIVATE m)
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(mcc_dynamic_musl PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
        endif()
        if(MCC_GITHASH)
            target_compile_definitions(mcc_dynamic_musl PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
        endif()
        if(TARGET mcc_mccdefs)
            add_dependencies(mcc_dynamic_musl mcc_mccdefs)
        endif()
        if(MCC_BUILD_STRIP AND NOT MCC_CC_NAME STREQUAL "msvc")
            target_link_options(mcc_dynamic_musl PRIVATE ${MCC_STRIP_LDFLAG})
        endif()
        if(_musl_vendored)
            add_dependencies(mcc_dynamic_musl musl-sysroot)
        endif()
        install(TARGETS mcc_dynamic_musl RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()

    install(TARGETS libmcc_musl
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    if(TARGET libmcc_musl_shared)
        install(TARGETS libmcc_musl_shared
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    endif()
endif()

if(MCC_BUILD_PROFILE)
    add_executable(mcc_p src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    target_include_directories(mcc_p PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(mcc_p PRIVATE -pg)
    target_compile_definitions(mcc_p PRIVATE MCC_PROFILE)
    if(NOT MCC_TARGETOS STREQUAL "WIN32")
        target_compile_definitions(mcc_p PRIVATE CONFIG_MCC_STATIC)
    endif()
    target_link_options(mcc_p PRIVATE -pg -static)
    target_link_libraries(mcc_p PRIVATE m)
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(mcc_p PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    endif()
    if(MCC_GITHASH)
        target_compile_definitions(mcc_p PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(mcc_p mcc_mccdefs)
    endif()
endif()

if(MCC_BUILD_SANITIZE)
    set(_san_cflags "")
    set(_san_lflags "")
    set(_san_asan_bindir "")
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        set(_san_cflags /fsanitize=address /Zi)
        set(_san_lflags /DEBUG)
        set(_san_desc "MSVC AddressSanitizer (/fsanitize=address)")
        get_filename_component(_san_asan_bindir "${CMAKE_C_COMPILER}" DIRECTORY)
    elseif(MCC_TARGETOS STREQUAL "WIN32")
        execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libasan.a
            OUTPUT_VARIABLE _libasan OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_libasan AND EXISTS "${_libasan}")
            set(_san_cflags -fsanitize=address,undefined -fno-sanitize=alignment)
            set(_san_lflags -fsanitize=address,undefined -fno-sanitize=alignment)
            set(_san_desc "ASan+UBSan (mingw libasan/libubsan present)")
        else()
            set(_san_cflags -fsanitize=undefined -fno-sanitize=alignment -fsanitize-trap=undefined)
            set(_san_lflags -fsanitize=undefined -fno-sanitize=alignment -fsanitize-trap=undefined)
            set(_san_desc "trap-mode UBSan minus alignment (mingw ships no ASan/UBSan runtime)")
        endif()
    else()
        set(_san_cflags -fsanitize=address,undefined -fno-sanitize=alignment)
        set(_san_lflags -fsanitize=address,undefined -fno-sanitize=alignment)
        set(_san_desc "ASan+UBSan (minus alignment)")
    endif()
    message(STATUS "mcc sanitizer (mcc_s): ${_san_desc}")

    add_executable(mcc_s src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    target_include_directories(mcc_s PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(mcc_s PRIVATE ${_san_cflags})
    if(_san_lflags)
        target_link_options(mcc_s PRIVATE ${_san_lflags})
    endif()
    if(NOT MCC_TARGETOS STREQUAL "WIN32")
        target_link_libraries(mcc_s PRIVATE m)
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(mcc_s PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
        endif()
    endif()
    if(MCC_GITHASH)
        target_compile_definitions(mcc_s PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(mcc_s mcc_mccdefs)
    endif()
endif()

foreach(_nt mcc mcc_static mcc_dynamic libmcc libmcc_shared mcc_p mcc_s)
    if(TARGET ${_nt})
        target_compile_definitions(${_nt} PRIVATE ${_mccdefs})
    endif()
endforeach()

if(MCC_BUILD_STATIC_EXE AND NOT MSVC AND NOT APPLE
   AND NOT MCC_TARGETOS STREQUAL "WIN32")
    foreach(_st mcc_static mcc_static_musl)
        if(TARGET ${_st})
            target_compile_definitions(${_st} PRIVATE CONFIG_MCC_STATIC)
        endif()
    endforeach()
endif()

set(MCC_MCCRT_EXTRA_O runmain bt-exe bt-dll bt-log bcheck)

function(_mcc_mccrt_src obj outvar)
    set(_lib "${CMAKE_CURRENT_SOURCE_DIR}/runtime/lib")
    set(_win "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib")
    if(obj STREQUAL "crt1w")
        set(${outvar} "${_win}/crt1.c" PARENT_SCOPE)
    elseif(obj STREQUAL "wincrt1w")
        set(${outvar} "${_win}/wincrt1.c" PARENT_SCOPE)
    elseif(obj MATCHES "^(crt1|wincrt1|dllcrt1|dllmain|winex|fenv)$")
        set(${outvar} "${_win}/${obj}.c" PARENT_SCOPE)
    elseif(obj STREQUAL "chkstk")
        set(${outvar} "${_win}/chkstk.c" PARENT_SCOPE)
    else()
        set(${outvar} "${_lib}/${obj}.c" PARENT_SCOPE)
    endif()
endfunction()

function(mcc_build_mccrt tname mcctgt cpu os native archive objdir out_extra)
    set(_common stdatomic atomic builtin alloca alloca-bt complex)
    set(_rt_gcc OFF)
    if(native AND (MCC_MCCRT_USE_HOSTCC OR MCC_EMBED_MCCRT)
       AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(_rt_gcc ON)
    endif()
    if(cpu STREQUAL "i386" OR cpu STREQUAL "x86_64")
        set(_objs mccrt ${_common})
    elseif(cpu STREQUAL "arm")
        set(_objs mccrt armeabi armflush ${_common})
    elseif(cpu STREQUAL "arm64" OR cpu STREQUAL "riscv64")
        set(_objs float128 ${_common})
    else()
        message(FATAL_ERROR "mcc_build_mccrt: unknown cpu '${cpu}'")
    endif()

    set(_win OFF)
    set(_osx OFF)
    if(os STREQUAL "WIN32")
        set(_win ON)
    elseif(os STREQUAL "Darwin")
        set(_osx ON)
    endif()

    if(native)
        list(APPEND _objs runmain tcov)
        if(MCC_CONFIG_BACKTRACE)
            list(APPEND _objs bt-exe bt-log)
            if(_win)
                list(APPEND _objs bt-dll)
            endif()
            if(MCC_CONFIG_BCHECK)
                list(APPEND _objs bcheck)
            endif()
        endif()
    endif()

    if(_win)
        list(APPEND _objs crt1 crt1w wincrt1 wincrt1w dllcrt1 dllmain winex fenv)
        if(cpu STREQUAL "i386" OR cpu STREQUAL "x86_64" OR cpu STREQUAL "arm64")
            list(APPEND _objs chkstk)
        endif()
    elseif(_osx)
        if(cpu STREQUAL "x86_64")
            list(APPEND _objs va_list)
        endif()
    else()
        if(cpu STREQUAL "i386")
            list(APPEND _objs pic86)
        elseif(cpu STREQUAL "x86_64")
            list(APPEND _objs va_list)
            if(_rt_gcc)
                list(APPEND _objs float128)
            endif()
        elseif(cpu STREQUAL "arm64")
            list(APPEND _objs armflush)
        elseif(cpu STREQUAL "riscv64")
            list(APPEND _objs lib-riscv)
        endif()
        list(APPEND _objs unwind-stub)
        list(APPEND _objs dsohandle)
    endif()

    set(_arch_incs
        -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -I${CMAKE_CURRENT_SOURCE_DIR}/include
        -I${CMAKE_CURRENT_SOURCE_DIR}/src/formats
        -I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386    -I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64
        -I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm     -I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm64
        -I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/riscv64
        -I${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt)
    if(_win)
        set(_xflags ${_mccdefs_flags}
                    -B${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32
                    -I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
                    -I${CMAKE_BINARY_DIR} ${_arch_incs})
    else()

        set(_xflags ${_mccdefs_flags} -fPIC
                    -B${CMAKE_CURRENT_SOURCE_DIR}/runtime
                    -I${CMAKE_BINARY_DIR} ${_arch_incs})
    endif()

    get_filename_component(_extra_dir "${archive}" DIRECTORY)
    file(MAKE_DIRECTORY "${objdir}")

    set(_use_gcc OFF)
    if(native AND (MCC_MCCRT_USE_HOSTCC OR MCC_EMBED_MCCRT))
        set(_use_gcc ON)
        set(_gccflags ${_mccdefs_flags}
                      -fPIC -fno-omit-frame-pointer -Wno-unused-function
                      -Wno-unused-variable
                      -I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include -I${CMAKE_BINARY_DIR}
                      ${_arch_incs})
        if(MCC_CPU STREQUAL "arm64")
            include(CheckCCompilerFlag)
            check_c_compiler_flag("-mno-outline-atomics" MCC_HAVE_NO_OUTLINE_ATOMICS)
            if(MCC_HAVE_NO_OUTLINE_ATOMICS)
                list(APPEND _gccflags -mno-outline-atomics)
            endif()
        endif()
        if(MCC_CONFIG_DWARF STREQUAL "" AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang"
           AND CMAKE_C_COMPILER_VERSION VERSION_LESS 12)
            set(_bflags_gcc -gstabs)
        else()
            set(_bflags_gcc -gdwarf)
        endif()
    endif()

    set(_launch "")
    if(native)
        set(_launch ${MCC_EMULATOR})
    endif()

    set(_archived "")
    set(_extra "")
    foreach(obj IN LISTS _objs)
        _mcc_mccrt_src("${obj}" _src)
        if(_use_gcc)
            set(_cc "${CMAKE_C_COMPILER}")
            set(_flags ${_gccflags})
            set(_ccdep "")
            set(_run "")
            if(obj STREQUAL "bcheck")
                list(APPEND _flags ${_bflags_gcc})
            endif()
        else()
            set(_cc "$<TARGET_FILE:${mcctgt}>")
            set(_flags ${_xflags})
            set(_ccdep ${mcctgt})
            set(_run ${_launch})
            if(obj STREQUAL "bcheck")
                list(APPEND _flags -bt)
            endif()
        endif()
        if(obj IN_LIST MCC_MCCRT_EXTRA_O)
            set(_out "${_extra_dir}/${obj}.o")
            list(APPEND _extra "${_out}")
        else()
            set(_out "${objdir}/${obj}.o")
            list(APPEND _archived "${_out}")
        endif()
        add_custom_command(
            OUTPUT "${_out}"
            COMMAND ${_run} "${_cc}" -c "${_src}" -o "${_out}" ${_flags}
            DEPENDS ${_ccdep} "${_src}"
            COMMENT "${tname} -c ${obj}"
            VERBATIM)
    endforeach()

    if(_use_gcc)
        add_custom_command(
            OUTPUT "${archive}"
            COMMAND "${CMAKE_AR}" rcs "${archive}" ${_archived}
            DEPENDS ${_archived}
            COMMENT "ar ${tname}"
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT "${archive}"
            COMMAND ${_launch} "$<TARGET_FILE:${mcctgt}>" -ar rcs "${archive}" ${_archived}
            DEPENDS ${_archived} ${mcctgt}
            COMMENT "mcc -ar ${tname}"
            VERBATIM)
    endif()
    add_custom_target(${tname} ALL DEPENDS "${archive}" ${_extra})
    set(${out_extra} "${_extra}" PARENT_SCOPE)
endfunction()

if(MCC_EMBED_MCCRT AND MCC_TARGETOS STREQUAL "WIN32")
    set(MCC_EMBED_MCCRT OFF)
    message(STATUS "mcc: MCC_EMBED_MCCRT not supported on WIN32; using the sidecar libmccrt.a")
endif()

if(MCC_EMBED_MCCRT AND MCC_TARGETOS STREQUAL "Darwin" AND NOT MCC_MCCRT_USE_HOSTCC)
    set(MCC_EMBED_MCCRT OFF)
    message(STATUS "mcc: MCC_EMBED_MCCRT unsupported on Darwin (mcc reads ELF objects only); using the sidecar libmccrt.a")
endif()

if(MCC_TARGETOS STREQUAL "WIN32")
    set(MCC_MCCRT_ARCHIVE "${CMAKE_BINARY_DIR}/lib/libmccrt.a")
else()
    set(MCC_MCCRT_ARCHIVE "${CMAKE_BINARY_DIR}/libmccrt.a")
endif()
mcc_build_mccrt(mccrt mcc "${MCC_CPU}" "${MCC_TARGETOS}" ON
    "${MCC_MCCRT_ARCHIVE}" "${CMAKE_BINARY_DIR}/lib"
    MCCRT_EXTRA_OBJS)

if(MCC_EMBED_MCCRT)
    if(CMAKE_CROSSCOMPILING)
        find_program(MCC_HOSMCC
            NAMES "$ENV{CC_FOR_BUILD}" "$ENV{HOSMCC}" cc gcc clang
            DOC "Host C compiler used to build the codegen tools (c2str/bin2c)")
        if(NOT MCC_HOSMCC)
            message(FATAL_ERROR
                "MCC_EMBED_MCCRT with CMAKE_CROSSCOMPILING needs a host C compiler "
                "to build bin2c. Set -DMCC_HOSMCC=<cc> (or CC_FOR_BUILD), or pass "
                "-DMCC_EMBED_MCCRT=OFF to keep the on-disk sidecar libmccrt.a.")
        endif()
        set(_bin2c_exe "${CMAKE_CURRENT_BINARY_DIR}/bin2c_host")
        execute_process(
            COMMAND "${MCC_HOSMCC}" -o "${_bin2c_exe}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/bin2c.c"
            RESULT_VARIABLE _bin2c_rc)
        if(NOT _bin2c_rc EQUAL 0)
            message(FATAL_ERROR "Host compiler '${MCC_HOSMCC}' failed to build bin2c")
        endif()
        set(_bin2c_dep "${_bin2c_exe}")
    else()
        add_executable(bin2c tools/bin2c.c)
        set(_bin2c_exe "$<TARGET_FILE:bin2c>")
        set(_bin2c_dep bin2c)
    endif()
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/mccrt_blob.c"
        COMMAND "${_bin2c_exe}" "${MCC_MCCRT_ARCHIVE}"
                "${CMAKE_BINARY_DIR}/mccrt_blob.c" mccrt_blob
        DEPENDS ${_bin2c_dep} mccrt "${MCC_MCCRT_ARCHIVE}"
        COMMENT "Embedding ${MCC_MCCRT_ARCHIVE} into mcc (mccrt_blob.c)"
        VERBATIM)
    target_sources(mcc PRIVATE "${CMAKE_BINARY_DIR}/mccrt_blob.c")
    target_compile_definitions(mcc PRIVATE MCC_EMBED_MCCRT=1)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${CMAKE_BINARY_DIR}/include" FILES_MATCHING PATTERN "*.h")

if(MCC_TARGETOS STREQUAL "WIN32")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/include/"
        DESTINATION "${CMAKE_BINARY_DIR}/include")
    file(GLOB _win_build_defs "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib/*.def")
    file(COPY ${_win_build_defs} DESTINATION "${CMAKE_BINARY_DIR}/lib")
endif()

if(MCC_BUILD_COVERAGE)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/mcc_c"
        COMMAND ${MCC_EMULATOR} "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
                -o "${CMAKE_BINARY_DIR}/mcc_c" -ftest-coverage
                "-I${CMAKE_BINARY_DIR}" "-I${CMAKE_CURRENT_SOURCE_DIR}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/src"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/src/formats"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/${MCC_CPU}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
                "-B${CMAKE_CURRENT_SOURCE_DIR}" "-B${CMAKE_BINARY_DIR}"
        DEPENDS mcc mccrt "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
        COMMENT "mcc -ftest-coverage mcc.c -> mcc_c" VERBATIM)
    add_custom_target(mcc_cov ALL DEPENDS "${CMAKE_BINARY_DIR}/mcc_c")
endif()

function(mcc_add_cross_mcc base_tgt base_out static_tgt static_out defs)
    set(_variants "${base_tgt}|${base_out}|dynamic")
    if(MCC_BUILD_STATIC_EXE AND NOT MCC_CC_NAME STREQUAL "msvc")
        list(APPEND _variants "${static_tgt}|${static_out}|static")
    endif()
    foreach(_v IN LISTS _variants)
        string(REPLACE "|" ";" _p "${_v}")
        list(GET _p 0 _vt)
        list(GET _p 1 _vo)
        list(GET _p 2 _vk)
        add_executable(${_vt} src/mcc.c)
        set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
        set_target_properties(${_vt} PROPERTIES OUTPUT_NAME "${_vo}")
        target_include_directories(${_vt} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_definitions(${_vt} PRIVATE ${${defs}})
        if(NOT MCC_TARGETOS STREQUAL "WIN32")
            target_link_libraries(${_vt} PRIVATE m)
            find_package(Threads)
            if(Threads_FOUND)
                target_link_libraries(${_vt} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
            endif()
        endif()
        if(_vk STREQUAL "static")
            if(NOT MCC_TARGETOS STREQUAL "WIN32")
                target_compile_definitions(${_vt} PRIVATE CONFIG_MCC_STATIC)
            endif()
            target_link_options(${_vt} PRIVATE -static)
        endif()
        if(MCC_ALL_DIAGNOSTICS)
            target_compile_options(${_vt} PRIVATE ${MCC_DIAG_FLAGS})
        endif()
        if(MCC_GITHASH)
            target_compile_definitions(${_vt} PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
        endif()
        if(TARGET mcc_mccdefs)
            add_dependencies(${_vt} mcc_mccdefs)
        endif()
        if(MCC_BUILD_STRIP AND NOT MCC_CC_NAME STREQUAL "msvc")
            target_link_options(${_vt} PRIVATE ${MCC_STRIP_LDFLAG})
        endif()
        install(TARGETS ${_vt} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endforeach()
endfunction()

if(MCC_ENABLE_CROSS)
    if("${MCC_INSTALL_MCCDIR}" STREQUAL "")
        set(_mccdir_install "${CMAKE_INSTALL_FULL_LIBDIR}/mcc")
    else()
        set(_mccdir_install "${MCC_INSTALL_MCCDIR}")
    endif()

    set(MCC_X i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64
              arm64-win32 arm-wince riscv64 arm64-osx)
    foreach(_t IN LISTS MCC_X)
        set(_cdefs "")
        set(_cos "Linux")
        if(_t STREQUAL "i386")
            set(_ccpu i386)
            set(_cdefs MCC_TARGET_I386)
        elseif(_t STREQUAL "x86_64")
            set(_ccpu x86_64)
            set(_cdefs MCC_TARGET_X86_64)
        elseif(_t STREQUAL "i386-win32")
            set(_ccpu i386)
            set(_cos WIN32)
            set(_cdefs MCC_TARGET_I386 MCC_TARGET_PE)
        elseif(_t STREQUAL "x86_64-win32")
            set(_ccpu x86_64)
            set(_cos WIN32)
            set(_cdefs MCC_TARGET_X86_64 MCC_TARGET_PE)
        elseif(_t STREQUAL "x86_64-osx")
            set(_ccpu x86_64)
            set(_cos Darwin)
            set(_cdefs MCC_TARGET_X86_64 MCC_TARGET_MACHO)
        elseif(_t STREQUAL "arm")
            set(_ccpu arm)
            set(_cdefs MCC_TARGET_ARM MCC_ARM_VFP MCC_ARM_EABI MCC_ARM_HARDFLOAT)
        elseif(_t STREQUAL "arm64")
            set(_ccpu arm64)
            set(_cdefs MCC_TARGET_ARM64)
        elseif(_t STREQUAL "arm64-win32")
            set(_ccpu arm64)
            set(_cos WIN32)
            set(_cdefs MCC_TARGET_ARM64 MCC_TARGET_PE)
        elseif(_t STREQUAL "arm-wince")
            set(_ccpu arm)
            set(_cos WIN32)
            set(_cdefs MCC_TARGET_ARM MCC_ARM_VFP MCC_ARM_EABI MCC_ARM_HARDFLOAT MCC_TARGET_PE)
        elseif(_t STREQUAL "riscv64")
            set(_ccpu riscv64)
            set(_cdefs MCC_TARGET_RISCV64)
        elseif(_t STREQUAL "arm64-osx")
            set(_ccpu arm64)
            set(_cos Darwin)
            set(_cdefs MCC_TARGET_ARM64 MCC_TARGET_MACHO)
        endif()

        set(_ctriplet "")
        if(_t STREQUAL "i386")
            set(_ctriplet "i686-linux-gnu")
        elseif(_t STREQUAL "x86_64")
            set(_ctriplet "x86_64-linux-gnu")
        elseif(_t STREQUAL "arm")
            set(_ctriplet "arm-linux-gnueabihf")
        elseif(_t STREQUAL "arm64")
            set(_ctriplet "aarch64-linux-gnu")
        elseif(_t STREQUAL "riscv64")
            set(_ctriplet "riscv64-linux-gnu")
        endif()
        set(_cpath_defs "")
        if(NOT _ctriplet STREQUAL "")
            list(APPEND _cpath_defs
                "CONFIG_SYSROOT=\"/usr/${_ctriplet}\""
                "CONFIG_MCC_CRTPREFIX=\"{R}/lib\""
                "CONFIG_MCC_LIBPATHS=\"{R}/lib:{B}\""
                "CONFIG_MCC_SYSINCLUDEPATHS=\"{B}/include:{R}/include\"")
        endif()

        set(_cross_defs ${_cdefs} ${_cpath_defs}
            "CONFIG_MCC_CROSSPREFIX=\"${_t}-\"")
        if(_cos STREQUAL "WIN32")
            list(APPEND _cross_defs "CONFIG_MCCDIR=\"${_mccdir_install}/win32\"")
        endif()
        mcc_add_cross_mcc(mcc-${_t} "mcc-${_t}"
                          mcc-${_t}-static "mcc-${_t}-static" _cross_defs)

        mcc_build_mccrt(${_t}-mccrt mcc-${_t} "${_ccpu}" "${_cos}" OFF
            "${CMAKE_BINARY_DIR}/${_t}-libmccrt.a"
            "${CMAKE_BINARY_DIR}/lib-${_t}" _cross_extra)
        install(FILES "${CMAKE_BINARY_DIR}/${_t}-libmccrt.a"
            DESTINATION "${_mccdir_install}")

        if(MCC_BUILD_MUSL AND NOT _ctriplet STREQUAL "" AND _cos STREQUAL "Linux")
            mcc_musl_target("${_ccpu}" ON _cmi _cmt)
            set(_cpath_defs_musl
                "CONFIG_SYSROOT=\"/usr/${_cmt}\""
                "CONFIG_MCC_CRTPREFIX=\"{R}/lib\""
                "CONFIG_MCC_LIBPATHS=\"{R}/lib:{B}\""
                "CONFIG_MCC_SYSINCLUDEPATHS=\"{B}/include:{R}/include\"")
            if(NOT _cmi STREQUAL "")
                list(APPEND _cpath_defs_musl "CONFIG_MCC_ELFINTERP=\"${_cmi}\"")
            endif()
            set(_cross_defs_musl ${_cdefs} ${_cpath_defs_musl}
                "CONFIG_MCC_MUSL=1" "CONFIG_MCC_CROSSPREFIX=\"${_t}-\"")
            mcc_add_cross_mcc(mcc-${_t}-musl "mcc-${_t}-musl"
                              mcc-${_t}-static-musl "mcc-${_t}-static-musl" _cross_defs_musl)
        endif()
    endforeach()
endif()

if(MCC_TARGETOS STREQUAL "WIN32" AND _libmcc_shared)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/libmcc.def"
        COMMAND ${MCC_EMULATOR} "$<TARGET_FILE:mcc>" -impdef "$<TARGET_FILE:${_libmcc_shared}>"
                -o "${CMAKE_BINARY_DIR}/libmcc.def"
        DEPENDS mcc ${_libmcc_shared}
        COMMENT "mcc -impdef libmcc.dll -> libmcc.def" VERBATIM)
    add_custom_target(libmcc_def ALL DEPENDS "${CMAKE_BINARY_DIR}/libmcc.def")
endif()

include(GNUInstallDirs)
if("${MCC_INSTALL_MCCDIR}" STREQUAL "")
    set(_mccdir "${CMAKE_INSTALL_FULL_LIBDIR}/mcc")
else()
    set(_mccdir "${MCC_INSTALL_MCCDIR}")
endif()

install(TARGETS mcc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(TARGET mcc_static)
    install(TARGETS mcc_static RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
if(TARGET mcc_dynamic)
    install(TARGETS mcc_dynamic RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

install(TARGETS libmcc EXPORT mccTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
if(TARGET libmcc_shared)
    install(TARGETS libmcc_shared EXPORT mccTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif()
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/libmcc.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
set(_mcc_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/mcc")
install(EXPORT mccTargets
    NAMESPACE mcc::
    FILE mccTargets.cmake
    DESTINATION "${_mcc_cmakedir}")
file(WRITE "${CMAKE_BINARY_DIR}/mccConfig.cmake.in"
"@PACKAGE_INIT@\ninclude(CMakeFindDependencyMacro)\nfind_dependency(Threads)\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/mccTargets.cmake\")\ncheck_required_components(mcc)\n")
configure_package_config_file(
    "${CMAKE_BINARY_DIR}/mccConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/mccConfig.cmake"
    INSTALL_DESTINATION "${_mcc_cmakedir}")
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/mccConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_BINARY_DIR}/mccConfig.cmake"
    "${CMAKE_BINARY_DIR}/mccConfigVersion.cmake"
    DESTINATION "${_mcc_cmakedir}")

if(MCC_TARGETOS STREQUAL "WIN32")
    install(FILES "${MCC_MCCRT_ARCHIVE}" ${MCCRT_EXTRA_OBJS}
        DESTINATION "${_mccdir}/lib")
else()
    install(FILES "${MCC_MCCRT_ARCHIVE}" ${MCCRT_EXTRA_OBJS}
        DESTINATION "${_mccdir}")
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${_mccdir}/include" FILES_MATCHING PATTERN "*.h")

if(MCC_TARGETOS STREQUAL "WIN32")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/include/"
        DESTINATION "${_mccdir}/include")
    file(GLOB _win_defs "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib/*.def")
    install(FILES ${_win_defs} DESTINATION "${_mccdir}/lib")
    if(_libmcc_shared)
        install(FILES "${CMAKE_BINARY_DIR}/libmcc.def"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL)
    endif()
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/"
        DESTINATION examples FILES_MATCHING PATTERN "*.c")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/api_basic.c"
        DESTINATION examples)
endif()

if(NOT CMAKE_CROSSCOMPILING)
    add_executable(mccbuild tools/build.c tools/toolsupport.c)
    target_link_libraries(mccbuild ${CMAKE_DL_LIBS})
    add_executable(ci tools/ci.c tools/toolsupport.c)
    target_link_libraries(ci ${CMAKE_DL_LIBS})

    set(MCC_DIST_VERSION "v0.0.0" CACHE STRING
        "Version tag for the `package-dist` target (leading 'v' is stripped)")
    set(MCC_DIST_PLAT "${MCC_CPU}-${MCC_TARGETOS}" CACHE STRING
        "Platform tag embedded in release bundle names (mcc-<ver>-<plat>.<ext>)")
    add_custom_target(package-dist
        COMMAND "$<TARGET_FILE:ci>" pkg
                --ver "${MCC_DIST_VERSION}" --plat "${MCC_DIST_PLAT}"
                --stage "${CMAKE_INSTALL_PREFIX}" --out "${MCC_DIST_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        VERBATIM USES_TERMINAL
        COMMENT "Packaging release bundles (mcc/libmcc/mcc-cross) via `ci pkg`")
    add_dependencies(package-dist ci)

    if(MCC_BENCH)
        add_executable(mccbench tools/bench.c tools/toolsupport.c)
        target_link_libraries(mccbench ${CMAKE_DL_LIBS})
        if(WIN32)
            target_link_libraries(mccbench psapi advapi32)
        endif()
        add_custom_target(bench
            COMMAND "$<TARGET_FILE:mccbench>"
                    --mcc "$<TARGET_FILE:mcc>"
                    --srcroot "${CMAKE_SOURCE_DIR}"
                    --builddir "${CMAKE_BINARY_DIR}"
                    --plat "${MCC_DIST_PLAT}"
                    --junit "${CMAKE_BINARY_DIR}/ctest-junit.xml"
                    --out "${MCC_DIST_DIR}/bench-${MCC_DIST_PLAT}.txt"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            VERBATIM USES_TERMINAL
            COMMENT "Benchmarking compilers (mcc/gcc/clang/mingw/msvc) via mccbench")
        add_dependencies(bench mccbench mcc)
    endif()

    option(MCC_LOCAL_CI_AS_TEST
        "Make `test` run the full local-CI matrix (disables this build's own ctest suite; see the `local-ci` preset)." OFF)
    if(MCC_LOCAL_CI_AS_TEST)
        set(MCC_BUILD_TESTS OFF)
        set(_local_ci_name test)
    else()
        set(_local_ci_name ci-local)
    endif()
    add_custom_target(${_local_ci_name}
        COMMAND "$<TARGET_FILE:ci>" local
            --version "${MCC_DIST_VERSION}"
            --host-cpu "${MCC_CPU}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL VERBATIM
        COMMENT "Local CI: probe host + run every applicable CI/release preset")
    add_dependencies(${_local_ci_name} ci)
endif()

if(MCC_BUILD_TESTS)
    enable_testing()
    set(_mcc_exe "$<TARGET_FILE:mcc>")
    set(_bdir "${CMAKE_BINARY_DIR}")
    set(_idir "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")

    function(mcc_skip_test _name _reason)
        add_test(NAME ${_name} COMMAND "${CMAKE_COMMAND}" -E echo "SKIP: ${_reason}")
        set_tests_properties(${_name} PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:" ${ARGN})
    endfunction()

    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(mccharness tools/mccharness.c tools/toolsupport.c)
        target_link_libraries(mccharness ${CMAKE_DL_LIBS})

        if(NOT WIN32)
            add_executable(machofat tools/machofat.c)
        endif()
    endif()

    add_test(NAME mcc_build
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}")
    set_tests_properties(mcc_build PROPERTIES FIXTURES_SETUP MCC_BUILT)

    if(TARGET mcc_s)
        add_test(NAME sanitize-smoke COMMAND "${CMAKE_COMMAND}"
            "-DMCC=$<TARGET_FILE:mcc_s>" "-DBDIR=${_bdir}"
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/sanitize/smoke.c"
            "-DOUT=${CMAKE_BINARY_DIR}/sanitize-smoke${MCC_EXESUF}"
            "-DASAN_BINDIR=${_san_asan_bindir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/sanitize/run_sanitize.cmake")
        set_tests_properties(sanitize-smoke PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(hostgate tools/hostgate.c tools/toolsupport.c)
        target_link_libraries(hostgate ${CMAKE_DL_LIBS})
        add_test(NAME host-gate-invariant
            COMMAND "$<TARGET_FILE:hostgate>" src tools
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(host-gate-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_executable(ckconfig tools/ckconfig.c tools/toolsupport.c)
        target_link_libraries(ckconfig ${CMAKE_DL_LIBS})
        add_test(NAME config-drift-invariant
            COMMAND "$<TARGET_FILE:ckconfig>" src CMakeLists.txt
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(config-drift-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME preset-parity-invariant
            COMMAND "$<TARGET_FILE:ci>" parity --srcroot "${CMAKE_CURRENT_SOURCE_DIR}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(preset-parity-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(MCC_AST AND NOT CMAKE_CROSSCOMPILING)
        add_executable(asttool tools/asttool.c)
        target_include_directories(asttool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_definitions(asttool PRIVATE CONFIG_AST=1)
        foreach(_an arena validate dump cfg provenance template)
            add_test(NAME "ast/${_an}" COMMAND asttool "${_an}")
            set_tests_properties("ast/${_an}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()

        if(NOT CMAKE_CROSSCOMPILING AND NOT MCC_TARGETOS STREQUAL "WIN32")
            foreach(_case "ret42;42" "ret0;0" "retexpr;14" "mainempty;0" "argc_expr;42"
                          "local_ret;42" "local_two;42" "neg_fallback;42" "global_ref;42"
                          "cast_expr;41" "call_printf;3" "if_else;42" "if_return;42"
                          "while_sum;36" "for_sum;36" "for_infinite;28" "ptr_deref;42" "do_while;36"
                          "array_sum;42" "ternary;42" "logand;42" "array_init;42" "float_ops;15"
                          "call_store;42" "struct_member;42" "struct_copy;42"
                          "switch_dispatch;17" "goto_dispatch;42" "struct_byval_arg;42"
                          "bitfield;42" "struct_ret_caller;42" "struct_ret_sret;42"
                          "short_circuit;42")
                list(GET _case 0 _rn)
                list(GET _case 1 _rc)
                add_test(NAME "ast/replay-${_rn}" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>"
                    "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/${_rn}.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                    "-DEXPECT_RC=${_rc}" "-DREPLAYED=main"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-${_rn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            endforeach()
            if(NOT (MCC_TARGETOS STREQUAL "Darwin" AND MCC_CPU STREQUAL "arm64"))
                add_test(NAME "ast/replay-ld_fallback" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/ld_fallback.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                    "-DEXPECT_RC=42" "-DNOREPLAY=main"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-ld_fallback" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            endif()

            add_test(NAME "ast/replay-struct_return" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/struct_return.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=make"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/replay-struct_return" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_test(NAME "ast/replay-struct_ret_variadic" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/struct_ret_variadic.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=use"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/replay-struct_ret_variadic" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_test(NAME "ast/replay-complex_arith" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/complex_arith.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=addre"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/replay-complex_arith" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_test(NAME "ast/replay-complex_imag" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/complex_imag.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=mk"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/replay-complex_imag" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_test(NAME "ast/replay-complex_ctor" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/complex_ctor.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=mk"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/replay-complex_ctor" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_test(NAME "ast/replay-vla" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/vla.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=sum"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/replay-vla" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            if(MCC_CPU STREQUAL "x86_64")
                add_test(NAME "ast/replay-promote" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/promote.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                    "-DEXPECT_RC=42" "-DREPLAYED=loopy" "-DPROMOTE=1" "-DPROMOTES=loopy,callful,sumptr,fdot"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-promote" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test("ast/replay-promote"
                    "Tier-3 register promotion is x86_64-only (caller-saved R10/R9/R8 + callee-saved RBX/R12–R15 pools); on ${MCC_CPU} it is a no-op — coverage stays via exec-replay-promote")
            endif()

            if(MCC_CPU STREQUAL "x86_64" OR MCC_CPU STREQUAL "arm64")
                # sumbig takes a 32-byte struct by value. On x86_64 the argument
                # lands in the caller's stack frame and the Tier-4 capture grafts
                # it; on arm64 (AAPCS64) aggregates >16 bytes are passed indirectly
                # via a hidden pointer (VT_LLOCAL), which the capture does not model
                # yet, so sumbig desyncs and stays retained-only. Every other case,
                # including the 8-byte struct sumpt/addpt, grafts on both arches.
                set(_inline_fns "add,scale,madd,clamp,sgn,area,quad,pick,firsthit,mkpair,gsum,sumpt,sumbig,addpt")
                if(MCC_CPU STREQUAL "arm64")
                    set(_inline_fns "add,scale,madd,clamp,sgn,area,quad,pick,firsthit,mkpair,gsum,sumpt,addpt")
                endif()
                add_test(NAME "ast/replay-inline" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/inline.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                    "-DEXPECT_RC=42" "-DREPLAYED=main" "-DINLINE=1" "-DINLINES=${_inline_fns}" "-DREEMITS=fwd_sum,fwd_boxed"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-inline" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

                add_test(NAME "ast/replay-inline-spec" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/inline_spec.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                    "-DEXPECT_RC=42" "-DREPLAYED=main" "-DINLINE=1" "-DTEMPLATES=1"
                    "-DINLINES=choose,clampk,mul,addk" "-DSPECIALIZES=choose,clampk,mul,addk"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-inline-spec" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test("ast/replay-inline"
                    "virtual-inline is arch-independent and compiles on ${MCC_CPU}, but the fixture assertion is only verified on x86_64/arm64 so far")
                mcc_skip_test("ast/replay-inline-spec"
                    "per-site specialization is arch-independent but its fixture assertion is only verified on x86_64 so far")
            endif()

            add_test(NAME "ast/template-constfold" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/constfold.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=main" "-DTEMPLATES=1" "-DFOLDS=main"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/template-constfold" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endif()
    endif()

    if(MCC_CST AND NOT CMAKE_CROSSCOMPILING)
        add_executable(csttool tools/csttool.c)
        target_include_directories(csttool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_definitions(csttool PRIVATE CONFIG_MCC_CST=1)
        foreach(_cn store hash geom serial sym comment template)
            add_test(NAME "cst/${_cn}" COMMAND csttool "${_cn}")
            set_tests_properties("cst/${_cn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()

        file(GLOB _cst_fx RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/fixtures"
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/fixtures/*.c")
        foreach(_fx IN LISTS _cst_fx)
            string(REGEX REPLACE "\\.c$" "" _fn "${_fx}")
            add_test(NAME "cst/roundtrip-${_fn}" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=${_mcc_exe}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/fixtures/${_fx}"
                "-DOUT=${CMAKE_BINARY_DIR}/cst-rt-${_fn}.o"
                "-DIDIR=${_idir}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/roundtrip.cmake")
            set_tests_properties("cst/roundtrip-${_fn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()

        add_test(NAME "cst/symref" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}"
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/symref/refs.c"
            "-DOUT=${CMAKE_BINARY_DIR}/cst-symref.o"
            "-DIDIR=${_idir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/symref.cmake")
        set_tests_properties("cst/symref" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME "cst/symref-shadow" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}"
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/symref/shadow.c"
            "-DOUT=${CMAKE_BINARY_DIR}/cst-symref-shadow.o"
            "-DIDIR=${_idir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/symref-shadow.cmake")
        set_tests_properties("cst/symref-shadow" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME "cst/hashinv" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}"
            "-DDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/hashinv"
            "-DOUT=${CMAKE_BINARY_DIR}/cst-hashinv.o"
            "-DIDIR=${_idir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/hashinv.cmake")
        set_tests_properties("cst/hashinv" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME "cst/macro" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}"
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/macro/macros.c"
            "-DOUT=${CMAKE_BINARY_DIR}/cst-macro.o"
            "-DIDIR=${_idir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/macro.cmake")
        set_tests_properties("cst/macro" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME "cst/macro-nesting" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}"
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/macro/macro_nesting.c"
            "-DOUT=${CMAKE_BINARY_DIR}/cst-macro-nesting.o"
            "-DIDIR=${_idir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/macro-nesting.cmake")
        set_tests_properties("cst/macro-nesting" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        function(cst_kinds_test name src kinds)
            add_test(NAME "cst/kinds-${name}" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=${_mcc_exe}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/kinds/${src}"
                "-DOUT=${CMAKE_BINARY_DIR}/cst-kinds-${name}.o"
                "-DIDIR=${_idir}"
                "-DKINDS=${kinds}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/kinds.cmake")
            set_tests_properties("cst/kinds-${name}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endfunction()
        cst_kinds_test(expr "expr.c" "Unary;Cast;Paren;Primary")
        cst_kinds_test(decl "decl.c" "Decl;Func;ParamList;Enum;TypeName;Init;Label")
        cst_kinds_test(pp "pp.c" "Include;PPDirective;PPCond")
        cst_kinds_test(comment "comment.c" "Comment")

        add_test(NAME "cst/incstore" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}"
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/incstore/driver.c"
            "-DOUT=${CMAKE_BINARY_DIR}/cst-incstore.o"
            "-DIDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/incstore"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/incstore.cmake")
        set_tests_properties("cst/incstore" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME "cst/increment" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}"
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/incstore/increment_driver.c"
            "-DOUT=${CMAKE_BINARY_DIR}/cst-increment.o"
            "-DIDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/incstore"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cst/increment.cmake")
        set_tests_properties("cst/increment" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    add_executable(exec_runner tests/exec/runner.c)
    target_include_directories(exec_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec")
    set(_exec_env "MCC_TEST_EMU=${MCC_EMULATOR};MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_ASM=$<IF:$<BOOL:${MCC_CONFIG_ASM}>,1,0>;MCC_TEST_BCHECK=$<IF:$<BOOL:${MCC_CONFIG_BCHECK}>,1,0>;MCC_TEST_BACKTRACE=$<IF:$<BOOL:${MCC_CONFIG_BACKTRACE}>,1,0>")

    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/goldens.h" _exec_glines
         REGEX "^[ \t]*{[ \t]*\"[A-Za-z0-9_]+\"")
    foreach(_gl IN LISTS _exec_glines)
        string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
        set(_gn "${CMAKE_MATCH_1}")
        add_test(NAME "exec/${_gn}"
            COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                    "${CMAKE_BINARY_DIR}/exec-gran-${_gn}" --only "${_gn}")
        set_tests_properties("exec/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env}")
    endforeach()

    if(MCC_AST)
        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-replay/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-replay-${_gn}" --only "${_gn}")
            set_tests_properties("exec-replay/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O1;MCC_AST_TEMPLATES=0;MCC_AST_PROMOTE=0;MCC_AST_INLINE=0")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-replay-tmpl/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-replay-tmpl-${_gn}" --only "${_gn}")
            set_tests_properties("exec-replay-tmpl/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O1;MCC_AST_TEMPLATES=1;MCC_AST_PROMOTE=0;MCC_AST_INLINE=0")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-replay-promote/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-replay-promote-${_gn}" --only "${_gn}")
            set_tests_properties("exec-replay-promote/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O1;MCC_AST_TEMPLATES=0;MCC_AST_PROMOTE=1;MCC_AST_INLINE=0")
        endforeach()
    endif()

    if(MCC_EMULATOR)
        mcc_skip_test(cli-suite "structural readelf/nm suite runs on native host only")
    else()

        set(_cli_sh "")
        set(_win_sh_pathmod "")
        if(MCC_TARGETOS STREQUAL "WIN32")
            find_program(MCC_TEST_SH NAMES sh bash
                DOC "POSIX shell used to run the cli test pipelines on Windows")
            set(_cli_sh "${MCC_TEST_SH}")
            if(MCC_TEST_SH)
                get_filename_component(_win_sh_dir "${MCC_TEST_SH}" DIRECTORY)
                file(TO_NATIVE_PATH "${_win_sh_dir}" _win_sh_dir)
                set(_win_sh_pathmod "PATH=path_list_prepend:${_win_sh_dir}")
            endif()
            find_program(MCC_TEST_NM NAMES nm
                DOC "binutils nm/readelf directory for the structural cli/diff3 cases")
            if(NOT MCC_TEST_NM)
                mcc_mingw_resolve()
                if(EXISTS "${_MCC_MINGW_X86_64_GCC}")
                    get_filename_component(_win_mingw_bin "${_MCC_MINGW_X86_64_GCC}" DIRECTORY)
                    find_program(MCC_TEST_NM NAMES nm PATHS "${_win_mingw_bin}" NO_DEFAULT_PATH)
                endif()
            endif()
            if(MCC_TEST_NM)
                get_filename_component(_win_nm_dir "${MCC_TEST_NM}" DIRECTORY)
                file(TO_NATIVE_PATH "${_win_nm_dir}" _win_nm_dir)
                list(APPEND _win_sh_pathmod "PATH=path_list_prepend:${_win_nm_dir}")
            endif()
        endif()
        if(MCC_TARGETOS STREQUAL "WIN32" AND NOT _cli_sh)
            mcc_skip_test(cli-suite "needs a POSIX shell (no sh/bash found; set MCC_TEST_SH)")
        else()
            add_executable(cli_runner tests/cli/runner.c)
            target_include_directories(cli_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli")
            set(_cli_env "MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_ASM=$<IF:$<BOOL:${MCC_CONFIG_ASM}>,1,0>;MCC_TEST_DWARF=${MCC_CONFIG_DWARF}")
            if(_cli_sh)
                set(_cli_env "${_cli_env};MCC_TEST_SH=${_cli_sh}")
            endif()
            file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/cases.h" _cli_clines
                 REGEX "^[ \t]*{[ \t]*\"[A-Za-z0-9_]+\"[ \t]*,")
            foreach(_cl IN LISTS _cli_clines)
                string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_cl}")
                set(_cn "${CMAKE_MATCH_1}")
                add_test(NAME "cli/${_cn}"
                    COMMAND cli_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                            "${CMAKE_BINARY_DIR}/cli-gran-${_cn}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli" --only "${_cn}")
                set_tests_properties("cli/${_cn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_cli_env}")
                if(_win_sh_pathmod)
                    set_tests_properties("cli/${_cn}" PROPERTIES
                        ENVIRONMENT_MODIFICATION "${_win_sh_pathmod}")
                endif()
            endforeach()
        endif()
    endif()

    set(MCC_DIFF3_GCC "" CACHE FILEPATH
        "Explicit gcc/mingw reference compiler for the diff3/preprocess suites (overrides auto-detect)")
    set(MCC_DIFF3_CLANG "" CACHE FILEPATH
        "Explicit clang reference compiler for the diff3/preprocess suites (overrides auto-detect)")

    if(MCC_DIFF3_GCC)
        set(DIFF3_GCC "${MCC_DIFF3_GCC}")
    else()
        find_program(DIFF3_GCC NAMES gcc cc)

        mcc_find_gnu_gcc(_diff3_gnu_gcc)
        if(_diff3_gnu_gcc)
            set(DIFF3_GCC "${_diff3_gnu_gcc}")
        endif()

        if(NOT DIFF3_GCC AND WIN32)
            mcc_mingw_resolve()
            if(EXISTS "${_MCC_MINGW_X86_64_GCC}")
                set(DIFF3_GCC "${_MCC_MINGW_X86_64_GCC}")
            endif()
        endif()
    endif()

    if(MCC_DIFF3_CLANG)
        set(DIFF3_CLANG "${MCC_DIFF3_CLANG}")
    else()
        find_program(DIFF3_CLANG NAMES clang)

        if(NOT DIFF3_CLANG)
            mcc_clang_resolve()
            if(EXISTS "${_MCC_CLANG_EXE}")
                set(DIFF3_CLANG "${_MCC_CLANG_EXE}")
            endif()
        endif()
    endif()
    set(_diff3_sh_ok TRUE)
    if(MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TEST_SH)
        set(_diff3_sh_ok FALSE)
    endif()
    if(NOT MCC_EMULATOR AND DIFF3_GCC AND DIFF3_CLANG AND _diff3_sh_ok)
        add_executable(diff3_runner tests/diff3/runner.c)
        target_include_directories(diff3_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec")
        set(_diff3_env "MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_ASM=$<IF:$<BOOL:${MCC_CONFIG_ASM}>,1,0>")
        if(MCC_TEST_SH)
            set(_diff3_env "${_diff3_env};MCC_TEST_SH=${MCC_TEST_SH}")
        endif()
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/goldens.h" _d3_glines
             REGEX "^[ \t]*{[ \t]*\"[A-Za-z0-9_]+\"[ \t]*,[ \t]*\"run2?\"")
        foreach(_d3l IN LISTS _d3_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_d3l}")
            set(_d3n "${CMAKE_MATCH_1}")
            add_test(NAME "diff3/${_d3n}"
                COMMAND diff3_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/diff3-gran-${_d3n}"
                        "${DIFF3_GCC}" "${DIFF3_CLANG}" --only "${_d3n}")
            set_tests_properties("diff3/${_d3n}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_diff3_env}")
            if(_win_sh_pathmod)
                set_tests_properties("diff3/${_d3n}" PROPERTIES
                    ENVIRONMENT_MODIFICATION "${_win_sh_pathmod}")
            endif()
        endforeach()
    elseif(NOT _diff3_sh_ok)
        mcc_skip_test(diff3-suite "needs a POSIX shell on Windows (no sh/bash found; set MCC_TEST_SH)")
    else()
        mcc_skip_test(diff3-suite "needs native host + both gcc and clang (build the clang-toolchain target for a self-contained clang)")
    endif()

    if(MCC_TARGETOS STREQUAL "WIN32")
        mcc_skip_test(parts-suite "native gcc/clang/mcc 3-way needs a shared C99 libc; the PE/msvcrt target has no msvcrt equivalent for the parts' complex/tgmath/libm surface (covered in aggregate by mcctest)")
    elseif(NOT MCC_EMULATOR AND DIFF3_GCC AND DIFF3_CLANG AND MCC_CONFIG_ASM
       AND TARGET mccharness)
        file(GLOB _parts_wraps RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/parts"
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/parts/run_*.c")
        foreach(_pw IN LISTS _parts_wraps)
            string(REGEX REPLACE "\\.c$" "" _pn "${_pw}")
            add_test(NAME "parts/${_pn}" COMMAND "$<TARGET_FILE:mccharness>" parts
                --gcc ${DIFF3_GCC} --clang ${DIFF3_CLANG} --mcc ${_mcc_exe}
                --bdir ${_bdir} --idir ${_idir}
                --parts ${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/parts
                --work ${CMAKE_BINARY_DIR}/parts-gran-${_pn} --only ${_pn})
            set_tests_properties("parts/${_pn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()
    else()
        mcc_skip_test(parts-suite "needs native host + gcc + clang + integrated assembler")
    endif()

    if(NOT MCC_EMULATOR AND DIFF3_GCC AND DIFF3_CLANG)
        file(GLOB_RECURSE _pp_cases RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess"
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess/*.c"
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess/*.S")
        foreach(_ppc IN LISTS _pp_cases)
            string(REGEX REPLACE "\\.[cS]$" "" _ppn "${_ppc}")
            add_test(NAME "preprocess/${_ppn}"
                COMMAND "$<TARGET_FILE:mccharness>" preprocess
                        --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
                        --tdir ${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess
                        --gcc ${DIFF3_GCC} --clang ${DIFF3_CLANG} --only ${_ppc})
            set_tests_properties("preprocess/${_ppn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77)
        endforeach()
    else()
        mcc_skip_test(preprocess-suite
            "needs native host + both gcc and clang (build the clang-toolchain target for a self-contained clang)")
    endif()

    add_executable(libmcc_test tests/embed/api_basic.c)
    target_link_libraries(libmcc_test PRIVATE libmcc)
    target_include_directories(libmcc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME libtest
        COMMAND libmcc_test "-B${CMAKE_BINARY_DIR}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
    set_tests_properties(libtest PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

    add_executable(libmcc_test_extra tests/embed/api_extra.c)
    target_link_libraries(libmcc_test_extra PRIVATE libmcc)
    target_include_directories(libmcc_test_extra PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME libtest-extra
        COMMAND libmcc_test_extra "-B${CMAKE_BINARY_DIR}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
    set_tests_properties(libtest-extra PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

    if(MCC_CONFIG_BACKTRACE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/api_threaded.c")
        add_executable(libmcc_test_mt tests/embed/api_threaded.c)
        target_link_libraries(libmcc_test_mt PRIVATE libmcc)
        target_include_directories(libmcc_test_mt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(libmcc_test_mt PRIVATE Threads::Threads)
        endif()
        add_test(NAME libtest-mt
            COMMAND libmcc_test_mt
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/mt_workload.c"
                    "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(libtest-mt PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    else()
        mcc_skip_test(libtest-mt "requires MCC_CONFIG_BACKTRACE (multithread libmcc test)")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        add_test(NAME hello-run
            COMMAND ${_mcc_exe} "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
                    -run "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        set_tests_properties(hello-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME hello-exe COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c
            --out ${CMAKE_BINARY_DIR}/hello${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(hello-exe PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/static/smoke.c")
        if(NOT MCC_EMULATOR AND NOT MCC_BUILD_MUSL
           AND MCC_TARGETOS STREQUAL "Linux" AND MCC_CPU STREQUAL "x86_64")
            execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libc.a
                OUTPUT_VARIABLE _static_libc OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
            if(_static_libc AND EXISTS "${_static_libc}")
                add_test(NAME static-glibc-run COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}" "-DIDIR=${_idir}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/static/smoke.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/static-smoke${MCC_EXESUF}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/static/run_static.cmake")
                set_tests_properties(static-glibc-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test(static-glibc-run "no static glibc libc.a on host")
            endif()
        else()
            mcc_skip_test(static-glibc-run "static-glibc gate: native x86_64 Linux glibc only")
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/tls/models.c")
        if(NOT MCC_EMULATOR AND MCC_TARGETOS STREQUAL "Linux"
           AND MCC_CPU STREQUAL "x86_64")
            set(_tls_static 0)
            if(NOT MCC_BUILD_MUSL)
                execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libc.a
                    OUTPUT_VARIABLE _tls_libc OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
                if(_tls_libc AND EXISTS "${_tls_libc}")
                    set(_tls_static 1)
                endif()
            endif()
            add_test(NAME tls-models COMMAND "${CMAKE_COMMAND}"
                "-DMCC=${_mcc_exe}" "-DCC=${CMAKE_C_COMPILER}" "-DBDIR=${_bdir}"
                "-DIDIR=${_idir}" "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/tls/models.c"
                "-DOUTDIR=${CMAKE_BINARY_DIR}" "-DSTATIC=${_tls_static}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/tls/run_models.cmake")
            set_tests_properties(tls-models PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(tls-models "tls-models gate: native x86_64 Linux only")
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/vla.c")
        add_test(NAME vla_test-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/vla.c
            --out ${CMAKE_BINARY_DIR}/vla_test${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(vla_test-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/abi.c")
        add_executable(abitest tests/embed/abi.c)
        target_link_libraries(abitest PRIVATE libmcc)
        target_include_directories(abitest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME abitest-cc
            COMMAND abitest "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(abitest-cc PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    set(MCC_REF_CC "" CACHE FILEPATH
        "GCC-compatible reference compiler for the differential tests (mcctest/\
-bcheck). Defaults to the host compiler; on an MSVC host, auto-detected from PATH.")
    set(_ref_cc "${MCC_REF_CC}")
    set(_ref_cc_name "${MCC_CC_NAME}")
    set(_ref_cc_major "${MCC_GCC_MAJOR}")
    if(NOT _ref_cc)
        if(MCC_TARGETOS STREQUAL "WIN32" AND NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
            mcc_mingw_resolve()
            if(EXISTS "${_MCC_MINGW_X86_64_GCC}")
                set(_ref_cc "${_MCC_MINGW_X86_64_GCC}")
            else()
                find_program(_mcc_ref_gcc NAMES gcc)
                set(_ref_cc "${_mcc_ref_gcc}")
            endif()
        else()
            set(_ref_cc "${CMAKE_C_COMPILER}")
        endif()
    endif()
    if(_ref_cc AND NOT "${_ref_cc}" STREQUAL "${CMAKE_C_COMPILER}")

        execute_process(COMMAND "${_ref_cc}" -dumpversion
            OUTPUT_VARIABLE _ref_ver OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET RESULT_VARIABLE _ref_rc)
        if(_ref_rc EQUAL 0 AND "${_ref_ver}" MATCHES "^([0-9]+)")
            set(_ref_cc_major "${CMAKE_MATCH_1}")
        else()
            set(_ref_cc_major 0)
        endif()
        execute_process(COMMAND "${_ref_cc}" --version
            OUTPUT_VARIABLE _ref_banner ERROR_VARIABLE _ref_banner
            OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _ref_vrc)
        if(_ref_banner MATCHES "clang")
            set(_ref_cc_name clang)
        elseif(_ref_banner MATCHES "MSVC|Microsoft")
            set(_ref_cc_name msvc)
        elseif(_ref_banner MATCHES "ModernCC|TinyCC|[Tt]cc")
            set(_ref_cc_name mcc)
        else()
            set(_ref_cc_name gcc)
        endif()
    endif()

    set(_mcctest_refflags "")
    if(MCC_TARGETOS STREQUAL "WIN32")

        set(_msvcrt_start "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/msvcrt_start.c")
        set(_mcctest_refflags
            "-mno-ms-bitfields -D__USE_MINGW_ANSI_STDIO=0 -nostdlib ${_msvcrt_start} -lgcc -Wl,--start-group -lmingwex c:/windows/system32/msvcrt.dll -Wl,--end-group -lkernel32")
    elseif(MCC_CPU STREQUAL "arm")
        set(_mcctest_refflags "-marm")
    elseif(MCC_CPU STREQUAL "i386")
        set(_mcctest_refflags "-fno-PIC -fno-PIE -Wl,-z,notext")
    endif()
    if(NOT _ref_cc)
        mcc_skip_test(mcctest "needs a GCC-compatible reference cc (host cc is ${CMAKE_C_COMPILER_ID}; set MCC_REF_CC or put gcc on PATH)")
        mcc_skip_test(mcctest-bcheck "needs a GCC-compatible reference cc (host cc is ${CMAKE_C_COMPILER_ID}; set MCC_REF_CC or put gcc on PATH)")
    elseif(_ref_cc_name STREQUAL "gcc" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")

        mcc_skip_test(mcctest
            "GNU gcc reference on Darwin/arm64 diverges from clang on impl-defined/UB ABI corners (covered by the Apple-clang mcctest + diff3-suite)")
        mcc_skip_test(mcctest-bcheck
            "GNU gcc reference on Darwin/arm64 diverges from clang on impl-defined/UB ABI corners (covered by the Apple-clang mcctest + diff3-suite)")
    elseif(MCC_TARGETOS STREQUAL "WIN32" AND MCC_CPU STREQUAL "arm64")
        mcc_skip_test(mcctest "no arm64-Windows gcc reference matches the PE differential: the only reference is an emulated x86_64 mingw (UCRT printf + x86-only asm_test), which cannot be byte-identical to native arm64 mcc on legacy msvcrt (mcc arm64 codegen is covered by the exec/* goldens + pe-native-conformance; full_language differential runs on x86/x64 Windows)")
        mcc_skip_test(mcctest-bcheck "no arm64-Windows gcc reference matches the PE differential (emulated x86_64 mingw vs native arm64 mcc; see mcctest)")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c" AND MCC_CONFIG_ASM)
        add_test(NAME mcctest COMMAND "$<TARGET_FILE:mccharness>" mcctest
            --cc ${_ref_cc} --mcc ${_mcc_exe}
            --src ${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c
            --srcdir ${CMAKE_CURRENT_SOURCE_DIR} --bdir ${_bdir} --idir ${_idir}
            --work ${CMAKE_BINARY_DIR}
            --refflags "${_mcctest_refflags}"
            --testdefs "-DCC_NAME=CC_${_ref_cc_name} -DGCC_MAJOR=${_ref_cc_major}"
            --emu "${MCC_EMULATOR}")
        set_tests_properties(mcctest PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        if(MCC_CONFIG_BCHECK AND MCC_TARGETOS STREQUAL "WIN32")
            mcc_skip_test(mcctest-bcheck "mcc bounds-checking is unsupported on the PE/msvcrt target (faults in msvcrt callbacks/library calls)")
        elseif(MCC_CONFIG_BCHECK)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bcheck")
            add_test(NAME mcctest-bcheck COMMAND "$<TARGET_FILE:mccharness>" mcctest
                --cc ${_ref_cc} --mcc ${_mcc_exe}
                --src ${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c
                --srcdir ${CMAKE_CURRENT_SOURCE_DIR} --bdir ${_bdir} --idir ${_idir}
                --work ${CMAKE_BINARY_DIR}/bcheck
                --refflags "${_mcctest_refflags}"
                --testdefs "-DCC_NAME=CC_${_ref_cc_name} -DGCC_MAJOR=${_ref_cc_major}"
                --mccargs -b --emu "${MCC_EMULATOR}")
            set_tests_properties(mcctest-bcheck PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(mcctest-bcheck "requires MCC_CONFIG_BCHECK (bounds-checked differential test)")
        endif()
    else()
        mcc_skip_test(mcctest "requires integrated assembler (MCC_CONFIG_ASM)")
        mcc_skip_test(mcctest-bcheck "requires integrated assembler + MCC_CONFIG_BCHECK")
    endif()

    if(MCC_CPU MATCHES "^(i386|x86_64)$" AND MCC_CONFIG_ASM
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm/asm_c_connect/part1.c")
        add_test(NAME asm-c-connect-test COMMAND "$<TARGET_FILE:mccharness>" asmconnect
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcdir ${CMAKE_CURRENT_SOURCE_DIR}/tests --work ${CMAKE_BINARY_DIR}
            --emu "${MCC_EMULATOR}")
        set_tests_properties(asm-c-connect-test PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    else()
        mcc_skip_test(asm-c-connect-test
            "requires x86 target with integrated assembler (host target: ${MCC_CPU})")
    endif()

    if(MCC_CPU STREQUAL "x86_64" AND MCC_CONFIG_ASM
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm/dash_s_roundtrip/prog.c")
        add_test(NAME dash-s-roundtrip COMMAND "$<TARGET_FILE:mccharness>" dashs
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcdir ${CMAKE_CURRENT_SOURCE_DIR}/tests --work ${CMAKE_BINARY_DIR}
            --emu "${MCC_EMULATOR}")
        set_tests_properties(dash-s-roundtrip PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    else()
        mcc_skip_test(dash-s-roundtrip
            "requires x86_64 target with integrated assembler (host target: ${MCC_CPU})")
    endif()

    add_executable(seccmp tests/support/seccmp.c)
    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(dashsbytes tools/dashsbytes.c tools/toolsupport.c)
        target_link_libraries(dashsbytes ${CMAKE_DL_LIBS})
    endif()
    add_executable(objcheck tools/objcheck.c)

    add_executable(ckbuildmd tools/ckbuildmd.c)
    add_test(NAME build-md-nodes
        COMMAND "$<TARGET_FILE:ckbuildmd>"
                "${CMAKE_BINARY_DIR}/config-nodes.tsv"
                "${CMAKE_CURRENT_SOURCE_DIR}/BUILD.md")
    set_tests_properties(build-md-nodes PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(defcheck tools/defcheck.c tools/toolsupport.c)
        target_link_libraries(defcheck ${CMAKE_DL_LIBS})
        file(GLOB _win32_defs "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib/*.def")
        add_test(NAME def-verify COMMAND "$<TARGET_FILE:defcheck>" verify ${_win32_defs})
        set_tests_properties(def-verify PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME ci-matrix
            COMMAND "$<TARGET_FILE:ci>" matrix "${CMAKE_CURRENT_SOURCE_DIR}/CMakePresets.json")
        set_tests_properties(ci-matrix PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "linux-gcc")

        if(TARGET mccharness)
            add_test(NAME ci-pkg-smoke COMMAND "$<TARGET_FILE:mccharness>" pkgsmoke
                --ci "$<TARGET_FILE:ci>" --work "${CMAKE_BINARY_DIR}/pkgsmoke-work")
            set_tests_properties(ci-pkg-smoke PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77)

            add_test(NAME qemu-fetch-parse COMMAND "$<TARGET_FILE:mccharness>" qemufetch --selftest)
            set_tests_properties(qemu-fetch-parse PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endif()

        if(TARGET mccbuild AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
            add_test(NAME git-stamp COMMAND "$<TARGET_FILE:mccbuild>" --githash)
            set_tests_properties(git-stamp PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                PASS_REGULAR_EXPRESSION "@")
        endif()

        if(TARGET mccbuild AND NOT CMAKE_CROSSCOMPILING)
            add_test(NAME host-detect COMMAND "$<TARGET_FILE:mccbuild>" --detect
                --cc "${CMAKE_C_COMPILER}"
                --expect-cpu "${MCC_CPU}" --expect-os "${MCC_TARGETOS}"
                --expect-triplet "${MCC_TRIPLET}")
            set_tests_properties(host-detect PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()

        if(TARGET mccbuild)
            add_test(NAME arm-abi-detect COMMAND "$<TARGET_FILE:mccbuild>" --detect --arm-selftest)
            set_tests_properties(arm-abi-detect PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endif()

        if(TARGET mccbuild AND _ref_cc)
            add_test(NAME cross-factory COMMAND "$<TARGET_FILE:mccbuild>"
                --cross x86_64 --cc "${_ref_cc}"
                --out "${CMAKE_BINARY_DIR}/cross-factory-smoke")
            set_tests_properties(cross-factory PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        elseif(TARGET mccbuild)
            mcc_skip_test(cross-factory
                "needs a GCC-compatible cc (set MCC_REF_CC or put gcc on PATH)")
        endif()

        if(TARGET mccbuild)
            add_test(NAME config-defines COMMAND "$<TARGET_FILE:mccbuild>" --emit-defines
                --check "${CMAKE_BINARY_DIR}/config-defines.txt"
                --cpu "${MCC_CPU}" --targetos "${MCC_TARGETOS}" --libc "${MCC_CONFIG_LIBC}"
                --cpuver "${MCC_CPUVER}" --arm-eabi "${MCC_ARM_EABI}" --arm-vfp "${MCC_ARM_VFP}"
                --arm-hardfloat "${MCC_ARM_HARDFLOAT}" --arm-idiv "${MCC_ARM_IDIV}"
                --run-mmap-exec "${MCC_RUN_MMAP_EXEC}" --pie "${MCC_CONFIG_PIE}" --pic "${MCC_CONFIG_PIC}"
                --new-dtags "${MCC_CONFIG_NEW_DTAGS}" --codesign "${MCC_CONFIG_CODESIGN}"
                --bcheck "${MCC_CONFIG_BCHECK}" --asm "${MCC_CONFIG_ASM}" --backtrace "${MCC_CONFIG_BACKTRACE}"
                --cst "${MCC_CST}" --ast "${MCC_AST}"
                --new-macho "${MCC_CONFIG_NEW_MACHO}" --dwarf "${MCC_CONFIG_DWARF}" --semlock "${MCC_CONFIG_SEMLOCK}"
                --sysincludepaths "${MCC_SYSINCLUDEPATHS}" --libpaths "${MCC_LIBPATHS}"
                --crtprefix "${MCC_CRTPREFIX}" --elfinterp "${MCC_ELFINTERP}" --switches "${MCC_SWITCHES}"
                --triplet "${MCC_TRIPLET}" --os-release "${MCC_OS_RELEASE}" --sysroot "${MCC_SYSROOT}"
                --mccdir "${MCC_RESOLVED_MCCDIR}")
            set_tests_properties(config-defines PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endif()
    endif()
    foreach(_dsb_arch arm64 riscv64)
        if(TARGET mcc-${_dsb_arch} AND TARGET dashsbytes)
            add_test(NAME dash-s-bytes-${_dsb_arch}
                COMMAND "$<TARGET_FILE:dashsbytes>"
                "$<TARGET_FILE:mcc-${_dsb_arch}>"
                "$<TARGET_FILE:seccmp>"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm/dash_s_roundtrip/prog.c"
                "${CMAKE_BINARY_DIR}/dash-s-bytes-${_dsb_arch}")
            set_tests_properties(dash-s-bytes-${_dsb_arch} PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(dash-s-bytes-${_dsb_arch}
                "requires the mcc-${_dsb_arch} cross compiler (cmake --preset cross)")
        endif()
    endforeach()

    mcc_skip_test(asm-gas-directives
        "blocked: integrated assembler lacks sgdtq/sidtq/swapgs encodings (gas_directives.S:811)")

    set(MCC_GCCTESTSUITE_PATH "" CACHE PATH
        "gcc.c-torture directory for the `gcctestsuite` target")
    set(MCC_ARM_CROSS_COMPILE "" CACHE STRING
        "binutils prefix (e.g. arm-linux-gnueabi-) for `arm-asm-testsuite`")

    if(TARGET mcc-i386)

        mcc_mingw_resolve()
        set(_fc_harness_args "")
        if(EXISTS "${_MCC_MINGW_I686_GCC}")
            set(_fc_harness_args --gcc "${_MCC_MINGW_I686_GCC}" --m32 "")
        elseif(EXISTS "${_MCC_MINGW_MULTILIB_GCC}")
            set(_fc_harness_args --gcc "${_MCC_MINGW_MULTILIB_GCC}" --m32 "-m32")
        endif()
        add_test(NAME i386-fastcall-abi COMMAND "$<TARGET_FILE:mccharness>" i386fastcall
            --imcc $<TARGET_FILE:mcc-i386> ${_fc_harness_args}
            --work ${CMAKE_BINARY_DIR}/i386-fastcall-abi)
        set_tests_properties(i386-fastcall-abi PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_REGULAR_EXPRESSION "SKIP:")
    else()
        mcc_skip_test(i386-fastcall-abi
            "requires an i386 cross-compiler (target mcc-i386 is not configured)")
    endif()

    add_custom_target(gcctestsuite
        COMMAND "$<TARGET_FILE:mccharness>" gcctestsuite --mcc $<TARGET_FILE:mcc>
                --bdir "${CMAKE_BINARY_DIR}" --idir "${CMAKE_CURRENT_SOURCE_DIR}"
                --builddir "${CMAKE_BINARY_DIR}" --path "${MCC_GCCTESTSUITE_PATH}"
        DEPENDS mcc mccharness USES_TERMINAL
        COMMENT "gcctestsuite: gcc c-torture over mcc")

    if(MCC_AST)
        foreach(_col replay promote inline)
            add_custom_target(gcctestsuite-ast-${_col}
                COMMAND "$<TARGET_FILE:mccharness>" gcctestsuite --mcc $<TARGET_FILE:mcc>
                        --bdir "${CMAKE_BINARY_DIR}" --idir "${CMAKE_CURRENT_SOURCE_DIR}"
                        --builddir "${CMAKE_BINARY_DIR}/gcctestsuite-ast-${_col}"
                        --path "${MCC_GCCTESTSUITE_PATH}" --ast ${_col}
                DEPENDS mcc mccharness USES_TERMINAL
                COMMENT "gcctestsuite-ast-${_col}: gcc c-torture AST-vs-O0 differential (${_col} column)")
        endforeach()
        add_custom_target(gcctestsuite-ast
            DEPENDS gcctestsuite-ast-replay gcctestsuite-ast-promote gcctestsuite-ast-inline
            COMMENT "gcctestsuite-ast: all AST columns of the gcc c-torture differential gate")
    endif()

    if(TARGET mcc-arm)
        add_custom_target(arm-asm-testsuite
            COMMAND "$<TARGET_FILE:mccharness>" armasm --mcc $<TARGET_FILE:mcc-arm>
                    --cross-compile "${MCC_ARM_CROSS_COMPILE}"
                    --tokh "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm/arm-tok.h"
                    --builddir "${CMAKE_BINARY_DIR}"
            DEPENDS mcc-arm mccharness USES_TERMINAL
            COMMENT "arm-asm-testsuite: mcc vs GNU as over arm-tok.h")
    endif()

    file(GLOB _orphan_srcs CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/bounds_stress.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/floating_point.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/compile_errors.c")

    set(_x11_search "${MCC_SYSROOT}/usr/include" /usr/include /opt/X11/include
                    /usr/X11R6/include /usr/local/include /opt/homebrew/include)
    unset(MCC_X11_INCLUDE_DIR CACHE)
    find_path(MCC_X11_INCLUDE_DIR NAMES X11/Xlib.h PATHS ${_x11_search} NO_DEFAULT_PATH)
    foreach(_f IN LISTS _orphan_srcs)
        get_filename_component(_n "${_f}" NAME_WE)
        file(READ "${_f}" _orphan_txt)
        set(_orphan_x11_inc "")
        if(_orphan_txt MATCHES "X11/Xlib.h")
            if(MCC_TARGETOS STREQUAL "WIN32" OR NOT MCC_X11_INCLUDE_DIR)
                mcc_skip_test(compile.${_n} "X11 example; <X11/Xlib.h> not available on this host/target")
                continue()
            endif()
            set(_orphan_x11_inc "-I${MCC_X11_INCLUDE_DIR}")
        endif()
        add_test(NAME compile.${_n} COMMAND ${MCC_EMULATOR} "${_mcc_exe}"
            "-B${_bdir}" "-I${_idir}" ${_orphan_x11_inc} -c "${_f}"
            -o "${CMAKE_BINARY_DIR}/orphan_${_n}.o")
        set_tests_properties(compile.${_n} PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endforeach()
    if(MCC_TARGETOS STREQUAL "WIN32")
        file(GLOB _win_ex CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/examples/*.c")
        foreach(_f IN LISTS _win_ex)
            get_filename_component(_n "${_f}" NAME_WE)
            add_test(NAME compile.win32.${_n} COMMAND ${MCC_EMULATOR} "${_mcc_exe}"
                "-B${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/include" "-I${_idir}"
                -c "${_f}" -o "${CMAKE_BINARY_DIR}/orphan_win_${_n}.o")
            set_tests_properties(compile.win32.${_n} PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()
    else()
        mcc_skip_test(compile.win32 "requires WIN32 target (win32 runtime examples)")
    endif()

    if(MCC_ENABLE_CROSS)
        set(_mcc_cross_default "${CMAKE_BINARY_DIR}")
    else()
        set(_mcc_cross_default "${CMAKE_SOURCE_DIR}/cmake-cross")
    endif()
    set(MCC_CROSS_DIR "${_mcc_cross_default}"
        CACHE PATH "cross-compiler build dir for the wine PE runtime test")

    add_test(NAME pe-wine-conformance
        COMMAND "$<TARGET_FILE:mccharness>" pewine
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --work "${CMAKE_BINARY_DIR}/pe-wine-work")
    set_tests_properties(pe-wine-conformance PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT LABELS "wine" SKIP_RETURN_CODE 77)

    if(MCC_TARGETOS STREQUAL "WIN32" AND NOT CMAKE_CROSSCOMPILING)

        add_test(NAME pe-native-conformance
            COMMAND "$<TARGET_FILE:mccharness>" penative
                    --mcc "${_mcc_exe}" --b "${_bdir}"
                    --src "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/pe-native-work"
                    --cpu "${MCC_CPU}")
        set_tests_properties(pe-native-conformance PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(pe-native-conformance
            "native PE runtime conformance (only on a native WIN32 host)")
    endif()

    set(_macho_struct_native "")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(_macho_struct_native
            --nativemcc "$<TARGET_FILE:mcc>" --nativearch "${MCC_CPU}"
            --bdir "${CMAKE_BINARY_DIR}")
    endif()
    add_test(NAME macho-structural
        COMMAND "$<TARGET_FILE:mccharness>" machostructural
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --objcheck "$<TARGET_FILE:objcheck>"
                ${_macho_struct_native}
                --work "${CMAKE_BINARY_DIR}/macho-work")
    set_tests_properties(macho-structural PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-codegen-run
        COMMAND "$<TARGET_FILE:mccharness>" machocodegen
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --work "${CMAKE_BINARY_DIR}/macho-codegen-work")
    set_tests_properties(macho-codegen-run PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-image-run
        COMMAND "$<TARGET_FILE:mccharness>" machoimage
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --objcheck "$<TARGET_FILE:objcheck>"
                --work "${CMAKE_BINARY_DIR}/macho-image-work")
    set_tests_properties(macho-image-run PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-apple-libc
        COMMAND "$<TARGET_FILE:mccharness>" machoapplelibc
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --objcheck "$<TARGET_FILE:objcheck>"
                --work "${CMAKE_BINARY_DIR}/macho-apple-libc-work")
    set_tests_properties(macho-apple-libc PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-conformance-native
        COMMAND "$<TARGET_FILE:mccharness>" machonative
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --mcc "$<TARGET_FILE:mcc>"
                --objcheck "$<TARGET_FILE:objcheck>"
                --bdir "${CMAKE_BINARY_DIR}" --work "${CMAKE_BINARY_DIR}/macho-native-work")
    set_tests_properties(macho-conformance-native PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-stack-protector
        COMMAND "$<TARGET_FILE:mccharness>" stackguard
                --mcc "$<TARGET_FILE:mcc>" --objcheck "$<TARGET_FILE:objcheck>"
                --bdir "${CMAKE_BINARY_DIR}" --work "${CMAKE_BINARY_DIR}/stackguard-work")
    set_tests_properties(macho-stack-protector PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)

    if(TARGET machofat)
        set(_mf_xargs "")
        if(TARGET mcc-x86_64-osx)
            set(_mf_xargs --xmcc "$<TARGET_FILE:mcc-x86_64-osx>")
        endif()
        execute_process(COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _mf_sdk OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_mf_sdk)
            list(APPEND _mf_xargs --sdk "${_mf_sdk}")
        endif()
        add_test(NAME macho-universal
            COMMAND "$<TARGET_FILE:mccharness>" machofat
                    --mcc "$<TARGET_FILE:mcc>" --fat "$<TARGET_FILE:machofat>"
                    --bdir "${CMAKE_BINARY_DIR}" --work "${CMAKE_BINARY_DIR}/machofat-work"
                    ${_mf_xargs})
        set_tests_properties(macho-universal PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-universal
            "the machofat tool is POSIX-only (not built on a WIN32 host)")
    endif()

    option(MCC_DARWIN_HOST "Tests run on a macOS/darling host with real libSystem" OFF)
    if(NOT MCC_DARWIN_HOST)
        mcc_skip_test(macho-libsystem-kernel-fused
            "libmalloc/locale-stdio/dyld/pthread/GCD/ObjC need a macOS or darling host (-DMCC_DARWIN_HOST=ON); kernel-fused, no portable variant — see tests/qemu/apple-libc/PROVENANCE.md")
    endif()

    option(MCC_QEMU_TESTS
        "Download Gentoo stage3 rootfs and run cross conformance tests under qemu-user" OFF)
    if(MCC_QEMU_TESTS)
        set(MCC_QEMU_MIRROR "https://distfiles.gentoo.org/releases" CACHE STRING
            "Base URL of the Gentoo releases mirror (…/releases)")
        set(MCC_QEMU_ARCHS "x86_64;i386;arm;arm64;riscv64" CACHE STRING
            "Architectures to exercise under qemu-user")
        set(MCC_QEMU_LIBCS "glibc;musl" CACHE STRING
            "C libraries to exercise (glibc;musl)")
        set(MCC_QEMU_DLDIR "${MCC_VENDOR_DIR}" CACHE PATH
            "Parent dir for the downloaded Gentoo stage3 rootfs trees (vendor/gentoo-stage3-<arch>-<libc>)")

        set(_qsrc "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/conformance")
        foreach(_arch IN LISTS MCC_QEMU_ARCHS)

            if(_arch STREQUAL "x86_64")
                set(_gdir amd64)
                set(_qbin qemu-x86_64)
                set(_gptr latest-stage3-amd64-openrc.txt)
                set(_mptr latest-stage3-amd64-musl-openrc.txt)
            elseif(_arch STREQUAL "i386")
                set(_gdir x86)
                set(_qbin qemu-i386)
                set(_gptr latest-stage3-i686-openrc.txt)
                set(_mptr latest-stage3-i686-musl-openrc.txt)
            elseif(_arch STREQUAL "arm")
                set(_gdir arm)
                set(_qbin qemu-arm)
                set(_gptr latest-stage3-armv7a_hardfp-openrc.txt)
                set(_mptr latest-stage3-armv7a_hardfp_musl-openrc.txt)
            elseif(_arch STREQUAL "arm64")
                set(_gdir arm64)
                set(_qbin qemu-aarch64)
                set(_gptr latest-stage3-arm64-openrc.txt)
                set(_mptr latest-stage3-arm64-musl-openrc.txt)
            elseif(_arch STREQUAL "riscv64")
                set(_gdir riscv)
                set(_qbin qemu-riscv64)
                set(_gptr latest-stage3-rv64_lp64d-openrc.txt)
                set(_mptr latest-stage3-rv64_lp64d_musl-openrc.txt)
            else()
                message(WARNING "MCC_QEMU_TESTS: unknown arch '${_arch}', skipping")
                continue()
            endif()

            if(_arch STREQUAL MCC_CPU)
                set(_qcc "$<TARGET_FILE:mcc>")
            elseif(TARGET mcc-${_arch})
                set(_qcc "$<TARGET_FILE:mcc-${_arch}>")
            else()
                set(_qcc "")
            endif()
            find_program(_QEMU_${_arch} ${_qbin})

            foreach(_libc IN LISTS MCC_QEMU_LIBCS)
                set(_tn "qemu-${_arch}-${_libc}")
                if(_libc STREQUAL "glibc")
                    set(_ptr "${_gptr}")
                elseif(_libc STREQUAL "musl")
                    set(_ptr "${_mptr}")
                else()
                    mcc_skip_test(${_tn} "unknown libc '${_libc}'" LABELS qemu)
                    continue()
                endif()
                if(_qcc STREQUAL "")
                    mcc_skip_test(${_tn}
                        "no compiler for ${_arch} (native target is ${MCC_CPU}; build mcc-${_arch} via -DMCC_ENABLE_CROSS=ON)"
                        LABELS qemu)
                    continue()
                endif()
                if(NOT _QEMU_${_arch})
                    mcc_skip_test(${_tn} "${_qbin} not found (install the qemu-user package)" LABELS qemu)
                    continue()
                endif()

                set(_root "${MCC_QEMU_DLDIR}/gentoo-stage3-${_arch}-${_libc}")
                add_test(NAME ${_tn}-fetch COMMAND "$<TARGET_FILE:mccharness>" qemufetch
                    --ptrurl "${MCC_QEMU_MIRROR}/${_gdir}/autobuilds/${_ptr}"
                    --mirrorbase "${MCC_QEMU_MIRROR}/${_gdir}/autobuilds"
                    --dest "${_root}" --marker "${_root}/.fetched")
                set_tests_properties(${_tn}-fetch PROPERTIES
                    FIXTURES_SETUP FX_${_tn} FIXTURES_REQUIRED MCC_BUILT
                    LABELS qemu TIMEOUT 2000)

                add_test(NAME ${_tn} COMMAND "$<TARGET_FILE:mccharness>" qemurun
                    --mcc "${_qcc}" --mccbase "${CMAKE_BINARY_DIR}"
                    --sysroot "${_root}" --qemu "${_QEMU_${_arch}}"
                    --srcdir "${_qsrc}"
                    --workdir "${CMAKE_BINARY_DIR}/qemu-work/${_arch}-${_libc}")
                set_tests_properties(${_tn} PROPERTIES
                    FIXTURES_REQUIRED "MCC_BUILT;FX_${_tn}" LABELS qemu TIMEOUT 600)

                if(NOT _arch STREQUAL MCC_CPU)
                    add_test(NAME ${_tn}-exec COMMAND exec_runner
                        "${_qcc}" "${CMAKE_BINARY_DIR}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-cross-${_arch}-${_libc}"
                        integer_constants cast_operator struct_byval
                        struct_packed_indirect bitfields bitfields_ms
                        c11_freestanding_headers asm_outside_function riscv_asm
                        atomic_counter)
                    set_tests_properties(${_tn}-exec PROPERTIES
                        FIXTURES_REQUIRED "MCC_BUILT;FX_${_tn}" LABELS qemu
                        SKIP_RETURN_CODE 77 TIMEOUT 1200
                        ENVIRONMENT "MCC_TEST_EMU=;MCC_TEST_RUNEMU=${_QEMU_${_arch}} -L ${_root};MCC_TEST_SYSROOT=${_root};MCC_TEST_CPU=${_arch};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_ASM=$<IF:$<BOOL:${MCC_CONFIG_ASM}>,1,0>;MCC_TEST_BCHECK=0;MCC_TEST_BACKTRACE=$<IF:$<BOOL:${MCC_CONFIG_BACKTRACE}>,1,0>")
                endif()
            endforeach()
        endforeach()

        if((";${MCC_QEMU_ARCHS};" MATCHES ";arm64;")
           AND (";${MCC_QEMU_LIBCS};" MATCHES ";glibc;"))
            add_test(NAME qemu-arm64-osx COMMAND "$<TARGET_FILE:mccharness>" machocodegen
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --work "${CMAKE_BINARY_DIR}/macho-arm64-work" --arch arm64
                --sysroot "${MCC_QEMU_DLDIR}/gentoo-stage3-arm64-glibc")
            set_tests_properties(qemu-arm64-osx PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;FX_qemu-arm64-glibc"
                LABELS "qemu;macho" SKIP_RETURN_CODE 77 TIMEOUT 600)
        else()
            mcc_skip_test(qemu-arm64-osx
                "requires arm64+glibc in the qemu matrix and the arm64-osx cross compiler (cmake --preset cross)"
                LABELS qemu)
        endif()

        message(STATUS "mcc tests: qemu matrix enabled (ctest -L qemu) for "
                       "archs=[${MCC_QEMU_ARCHS}] libcs=[${MCC_QEMU_LIBCS}]")
    endif()

    message(STATUS "mcc tests: registered exec (golden) + preprocess + embed/abitest/diff "
                   "+ hello/vla + orphan compile checks (ctest)")
endif()

find_program(DOCKER_EXE NAMES docker)
if(DOCKER_EXE)
    set(MCC_QEMU_DOCKER_ARCHS "" CACHE STRING
        "ARCHS to pass to the qemu-docker matrix (empty = image default)")
    set(MCC_QEMU_DOCKER_LIBCS "" CACHE STRING
        "LIBCS to pass to the qemu-docker matrix (empty = image default)")
    set(_qd_env "")
    if(MCC_QEMU_DOCKER_ARCHS)
        list(APPEND _qd_env -e "ARCHS=${MCC_QEMU_DOCKER_ARCHS}")
    endif()
    if(MCC_QEMU_DOCKER_LIBCS)
        list(APPEND _qd_env -e "LIBCS=${MCC_QEMU_DOCKER_LIBCS}")
    endif()
    add_custom_target(qemu-docker
        COMMAND "${DOCKER_EXE}" build -t mcc-qemu
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/docker"
        COMMAND "${DOCKER_EXE}" run --rm
                -v "${CMAKE_CURRENT_SOURCE_DIR}:/work"
                -v "${MCC_VENDOR_DIR}:/vendor"
                -v "${MCC_DIST_DIR}:/dist"
                ${_qd_env} mcc-qemu
        USES_TERMINAL VERBATIM
        COMMENT "qemu-user cross matrix in Docker (image mcc-qemu)")
else()
    message(STATUS "mcc: docker not found — `qemu-docker` target unavailable "
                   "(see tests/qemu/docker/README.md)")
endif()

get_property(_mcc_all_tests DIRECTORY PROPERTY TESTS)
foreach(_t IN LISTS _mcc_all_tests)
    get_property(_l TEST "${_t}" PROPERTY LABELS)
    if(NOT "qemu" IN_LIST _l)
        set_property(TEST "${_t}" APPEND PROPERTY LABELS native)
    endif()
endforeach()
unset(_mcc_all_tests)

file(GLOB_RECURSE MCC_TAGFILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.S")
list(FILTER MCC_TAGFILES EXCLUDE REGEX "/(cmake-[^/]*|build-[^/]*|\\.git|\\.idea|\\.cache)/")
find_program(CTAGS_EXE ctags)
if(CTAGS_EXE)
    add_custom_target(tags
        COMMAND "${CTAGS_EXE}" ${MCC_TAGFILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "ctags ${MCC_TAGFILES}" VERBATIM)
endif()
find_program(ETAGS_EXE etags)
if(ETAGS_EXE)
    add_custom_target(ETAGS
        COMMAND "${ETAGS_EXE}" ${MCC_TAGFILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "etags ${MCC_TAGFILES}" VERBATIM)
endif()
if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _dist_hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_dist_hash)
        set(_dist "moderncc-mob-${_dist_hash}")
        add_custom_target(dist
            COMMAND "${GIT_EXECUTABLE}" archive --format=tar
                    "--prefix=${_dist}/" HEAD
                    -o "${CMAKE_BINARY_DIR}/${_dist}.tar"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "git archive -> ${_dist}.tar" VERBATIM)
    endif()
endif()

set(CPACK_PACKAGE_NAME "mcc")
set(CPACK_PACKAGE_VENDOR "ModernCC")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_CONTACT "moderncc-devel")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
endif()
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/" "/cmake-[^/]*/" "/build-[^/]*/" "/\\.idea/" "\\.o$" "\\.a$")
include(CPack)
