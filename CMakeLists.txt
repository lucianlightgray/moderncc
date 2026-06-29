cmake_minimum_required(VERSION 3.22)

set(MCC_VERSION "0.9.28rc")
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" MCC_VERSION_NUMERIC "${MCC_VERSION}")

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

    if((MCC_BUILD_SANITIZE OR MCC_BUILD_PROFILE)
       AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "mcc config: MCC_BUILD_SANITIZE/PROFILE need a GCC/Clang host compiler "
            "(have '${CMAKE_C_COMPILER_ID}'); disable them or set -DCMAKE_C_COMPILER "
            "to gcc/clang.")
    endif()
    if(MCC_BUILD_PROFILE AND MCC_TARGETOS STREQUAL "Darwin")
        message(FATAL_ERROR
            "mcc config: MCC_BUILD_PROFILE (mcc_p, -static) is unsupported on Darwin "
            "(no static crt0.o).")
    endif()
    if(CMAKE_CROSSCOMPILING AND "${MCC_EMULATOR}" STREQUAL "")
        if(MCC_CONFIG_AUTOCORRECT)
            if(NOT MCC_LIBMCC1_USEGCC)
                mcc_autocorrect(MCC_LIBMCC1_USEGCC ON
                    "no emulator: build libmcc1 with the host CC, not the foreign mcc")
            endif()
            if(MCC_BUILD_TESTS)
                mcc_autocorrect(MCC_BUILD_TESTS OFF
                    "no emulator: the test suite cannot run the foreign mcc / its exes")
            endif()
            if(MCC_BUILD_COVERAGE)
                mcc_autocorrect(MCC_BUILD_COVERAGE OFF
                    "no emulator: mcc_c is compiled by running the foreign mcc")
            endif()
            if(MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_BUILD_STATIC)
                message(FATAL_ERROR
                    "mcc config: even with MCC_CONFIG_AUTOCORRECT, a WIN32 *shared* "
                    "libmcc needs `mcc -impdef` to run on this host. Add "
                    "-DCMAKE_CROSSCOMPILING_EMULATOR=<run>, or build static with "
                    "-DMCC_BUILD_STATIC=ON.")
            endif()
        else()
            message(FATAL_ERROR
                "mcc config: CMAKE_CROSSCOMPILING is set, so the just-built `mcc` is a "
                "foreign binary, but it must run on this host to build libmcc1.a (and the "
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
    if(MCC_DISABLE_RPATH AND MCC_BUILD_STATIC)
        message(WARNING
            "mcc config: MCC_DISABLE_RPATH has no effect with a static libmcc "
            "(MCC_BUILD_STATIC=ON); rpath is only baked for a shared libmcc.")
    endif()
    if(NOT MCC_CONFIG_ASM AND NOT MCC_LIBMCC1_USEGCC)
        mcc_autocorrect(MCC_LIBMCC1_USEGCC ON
            "asm disabled: libmcc1 .S sources must be built with the host CC")
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


set(_elf "NOT MCC_TARGETOS STREQUAL \"WIN32\" AND NOT MCC_TARGETOS STREQUAL \"Darwin\"")
set(_darwin "MCC_TARGETOS STREQUAL \"Darwin\"")

mcc_snapshot_user_presets()

mcc_config_node(MCC_TOOLCHAIN_PROFILE TYPE STRING DEFAULT auto GROUP "Build targets"
    HELP "Toolchain(s) to build with, in order (list, e.g. gcc;clang;mcc); single = profile+defaults"
    CHOICES auto gcc clang mcc msvc)

set(_tc_allowed auto gcc clang mcc msvc)
foreach(_tc IN LISTS MCC_TOOLCHAIN_PROFILE)
    if(NOT _tc IN_LIST _tc_allowed)
        message(FATAL_ERROR
            "mcc config: MCC_TOOLCHAIN_PROFILE entry '${_tc}' is not one of: ${_tc_allowed}")
    endif()
endforeach()

option(MCC_SUPERBUILD_TEST "Run ctest after each matrix sub-build" ON)
option(MCC_SUPERBUILD_SEQUENTIAL "Build toolchain stages in order (targets within a stage stay parallel)" ON)
set(MCC_TARGETS "native" CACHE STRING
    "Cross-platform target(s) to build, as a list (native;cross;<custom>)")
set_property(CACHE MCC_TARGETS PROPERTY STRINGS native cross)

list(LENGTH MCC_TOOLCHAIN_PROFILE _tc_count)
list(LENGTH MCC_TARGETS _tg_count)
set(_is_matrix FALSE)
if(_tc_count GREATER 1 OR _tg_count GREATER 1 OR NOT MCC_TARGETS STREQUAL "native")
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
    set(_forward "")
    foreach(_v IN LISTS _MCC_PRESET_VARS)
        if(_v MATCHES "^MCC_(TOOLCHAIN_PROFILE|TARGETS|SUPERBUILD_TEST|SUPERBUILD_SEQUENTIAL)$")
            continue()
        endif()
        list(APPEND _forward "-D${_v}=${${_v}}")
    endforeach()
    set(_prev_stage "")
    foreach(_tc IN LISTS MCC_TOOLCHAIN_PROFILE)
        mcc_map_toolchain_cc("${_tc}" _cc)
        if(NOT _tc STREQUAL "auto" AND _cc STREQUAL "")
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
            if(NOT _cc STREQUAL "")
                list(APPEND _cargs "-DCMAKE_C_COMPILER=${_cc}")
            endif()
            set(_dep "")
            if(MCC_SUPERBUILD_SEQUENTIAL AND _prev_stage)
                set(_dep DEPENDS ${_prev_stage})
            endif()
            ExternalProject_Add(${_cell}
                SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}"
                BINARY_DIR  "${CMAKE_BINARY_DIR}/${_cell}"
                CMAKE_ARGS  ${_cargs}
                CONFIGURE_HANDLED_BY_BUILD ON
                BUILD_ALWAYS ON
                INSTALL_COMMAND ""
                ${_dep})
            if(MCC_SUPERBUILD_TEST)
                ExternalProject_Add_Step(${_cell} ctest
                    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${_cell}"
                    DEPENDEES build ALWAYS ON USES_TERMINAL ON)
            endif()
            list(APPEND _this_stage ${_cell})
        endforeach()
        set(_prev_stage "${_this_stage}")
    endforeach()
    return()
endif()

if(MCC_TOOLCHAIN_PROFILE STREQUAL "mcc")
    mcc_profile_seed(MCC_ONE_SOURCE ON)
elseif(MCC_TOOLCHAIN_PROFILE STREQUAL "msvc")
    mcc_profile_seed(MCC_ONE_SOURCE ON)
    mcc_profile_seed(MCC_BUILD_TESTS OFF)
endif()

mcc_config_node(MCC_ENABLE_CROSS  TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build all cross compilers (Makefile: make cross)")
mcc_config_node(MCC_BUILD_STATIC  TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build libmcc.a (ON) instead of shared libmcc (OFF)")
mcc_config_node(MCC_DISABLE_RPATH TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Do not bake -rpath into binaries linking libmcc.so"
    VISIBLE_WHEN "NOT MCC_BUILD_STATIC")
mcc_config_node(MCC_ONE_SOURCE    TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build libmcc from a single TU (Makefile: ONE_SOURCE)")
mcc_config_node(MCC_BUILD_TESTS   TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build/enable the test suite (tests/Makefile)")
mcc_config_node(MCC_DIAGNOSTICS TYPE BOOL DEFAULT OFF GROUP "Diagnostics" ADVANCED
    HELP "Everything-on diagnostics: verbose warnings + debug info + build mcc_s/mcc_p/mcc_c")
if(MCC_DIAGNOSTICS)
    mcc_profile_seed(MCC_BUILD_SANITIZE ON)
    mcc_profile_seed(MCC_BUILD_COVERAGE ON)
    mcc_profile_seed(MCC_BUILD_PROFILE  ON)
endif()

mcc_config_node(MCC_BUILD_PROFILE  TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_p (profiling: -pg -static)")
mcc_config_node(MCC_BUILD_COVERAGE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_c (coverage instrumentation)")
mcc_config_node(MCC_BUILD_SANITIZE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_s (-fsanitize=address,undefined)")
mcc_config_node(MCC_LIBMCC1_USEGCC TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build the native libmcc1 with the host CC instead of mcc (faster bcheck)")
mcc_config_node(MCC_CONFIG_AUTOCORRECT TYPE BOOL DEFAULT OFF GROUP "Build targets" ADVANCED
    HELP "Non-strict mode: auto-correct inert/non-runnable config combinations instead of only warning")

mcc_config_node(MCC_CONFIG_MINGW32  TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Build a WIN32/mingw32 target (input: forces MCC_TARGETOS=WIN32)")
mcc_config_node(MCC_CONFIG_BACKTRACE TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "Enable stack backtraces (-bt / -run)")
mcc_config_node(MCC_CONFIG_BCHECK   TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "Enable bounds checker (-b)"
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
mcc_config_node(MCC_WITH_SELINUX    TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Use mmap for executable memory (mcc -run)")
mcc_config_node(MCC_CONFIG_NEW_DTAGS TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Use DT_RUNPATH instead of DT_RPATH (mcc-emitted; always relevant on ELF)"
    VISIBLE_WHEN "${_elf}")

mcc_config_node(MCC_AUTO_MCCDIR     TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "Build-tree mcc auto-discovers libmcc1.a + headers locally, falling back to system CONFIG_MCCDIR")

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

if(WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Windows|MINGW|MSYS|CYGWIN" OR MCC_CONFIG_MINGW32)
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
    set(MCC_DISABLE_RPATH ON)
    if("${MCC_CONFIG_DWARF}" STREQUAL "")
        set(MCC_CONFIG_DWARF 4)
    endif()
    if(NOT MCC_CPU STREQUAL "i386")
        set(MCC_CONFIG_PIE ON)
    endif()
    set(MCC_BUILD_STATIC OFF)
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
elseif(CMAKE_C_COMPILER_ID STREQUAL "ModernCC")
    set(MCC_CC_NAME mcc)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(MCC_CC_NAME msvc)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
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
        if(_dumpmachine AND EXISTS "/usr/lib/${_dumpmachine}/crti.o")
            set(MCC_TRIPLET "${_dumpmachine}")
        endif()
    endif()
    if("${MCC_TRIPLET}" STREQUAL "" AND "${MCC_LIBPATHS}" STREQUAL ""
       AND MCC_CPU MATCHES "^(x86_64|arm64|riscv64)$"
       AND EXISTS "/usr/lib64/crti.o")
        set(MCC_LIBPATHS "{B}:/usr/lib64")
        if("${MCC_CRTPREFIX}" STREQUAL "")
            set(MCC_CRTPREFIX "/usr/lib64")
        endif()
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
if(MCC_DIAGNOSTICS)
    foreach(_f -Wall -Wextra -Wnull-dereference
               -Wno-sign-compare -Wno-unused-parameter -Wno-type-limits
               -Wno-unterminated-string-initialization
               -g3 -fno-omit-frame-pointer)
        string(MAKE_C_IDENTIFIER "MCC_HAS_DIAGFLAG_${_f}" _v)
        check_c_compiler_flag("${_f}" ${_v})
        if(${_v})
            list(APPEND MCC_DIAG_FLAGS "${_f}")
        endif()
    endforeach()
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
    if(NOT MCC_CC_NAME STREQUAL "clang"
       AND NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        add_link_options(-s)
    endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install"
        CACHE PATH "Install prefix (default: project-local <build>/install)" FORCE)
    message(STATUS "mcc install prefix: defaulted to local '${CMAKE_INSTALL_PREFIX}' "
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

if(MCC_CONFIG_LIBC STREQUAL "musl")
    list(APPEND _mccdefs "CONFIG_MCC_MUSL=1")
elseif(MCC_CONFIG_LIBC STREQUAL "uClibc")
    list(APPEND _mccdefs "CONFIG_MCC_UCLIBC=1")
endif()

if(MCC_WITH_SELINUX)
    list(APPEND _mccdefs "CONFIG_SELINUX=1")
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

set(_mccdefs_common
    "MCC_VERSION=\"${MCC_VERSION}\""
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

set(LIBMCC_ALL_SRC ${MCC_CORE_SRC} ${MCC_BACKEND_SRC})
list(REMOVE_ITEM LIBMCC_ALL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")

if(MCC_ONE_SOURCE)
    set(LIBMCC_SRC src/libmcc.c)
else()
    set(LIBMCC_SRC ${LIBMCC_ALL_SRC})
endif()

if(MCC_BUILD_STATIC)
    add_library(libmcc STATIC ${LIBMCC_SRC})
    if(MCC_CC_NAME STREQUAL "clang" AND MCC_CPU STREQUAL "x86_64")
        set_target_properties(libmcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
else()
    add_library(libmcc SHARED ${LIBMCC_SRC})
    target_compile_definitions(libmcc PRIVATE LIBMCC_AS_DLL)
    set_target_properties(libmcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(APPLE)
        set_target_properties(libmcc PROPERTIES
            MACOSX_RPATH ON VERSION "${MCC_VERSION_NUMERIC}")
    endif()
endif()
set_target_properties(libmcc PROPERTIES OUTPUT_NAME mcc PREFIX "lib")
if(NOT MCC_ONE_SOURCE)
    target_compile_definitions(libmcc PRIVATE ONE_SOURCE=0)
else()
    set(_one_src_deps "")
    foreach(_s IN LISTS LIBMCC_ALL_SRC)
        if(NOT _s STREQUAL "libmcc.c")
            list(APPEND _one_src_deps "${CMAKE_CURRENT_SOURCE_DIR}/${_s}")
        endif()
    endforeach()
    set_source_files_properties(src/libmcc.c PROPERTIES OBJECT_DEPENDS "${_one_src_deps}")
endif()
if(TARGET mcc_mccdefs)
    add_dependencies(libmcc mcc_mccdefs)
endif()
target_include_directories(libmcc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(NOT MCC_TARGETOS STREQUAL "WIN32")
    target_link_libraries(libmcc PUBLIC m)
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(libmcc PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    endif()
endif()
if(NOT "${MCC_EXTRA_LIBS}" STREQUAL "")
    separate_arguments(_mcc_extra_libs NATIVE_COMMAND "${MCC_EXTRA_LIBS}")
    target_link_libraries(libmcc PUBLIC ${_mcc_extra_libs})
endif()
if(MCC_DIAGNOSTICS)
    target_compile_options(libmcc PRIVATE ${MCC_DIAG_FLAGS})
endif()

add_executable(mcc src/mcc.c)
set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
target_compile_definitions(mcc PRIVATE ONE_SOURCE=0)
target_link_libraries(mcc PRIVATE libmcc)
if(MCC_GITHASH)
    target_compile_definitions(mcc PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
endif()
if(NOT MCC_BUILD_STATIC AND NOT MCC_DISABLE_RPATH)
    if(APPLE)
        set_target_properties(mcc PROPERTIES
            INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}")
    else()
        set_target_properties(mcc PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    endif()
endif()
if(MCC_DIAGNOSTICS)
    target_compile_options(mcc PRIVATE ${MCC_DIAG_FLAGS})
endif()


if(MCC_BUILD_PROFILE)
    add_executable(mcc_p src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    target_include_directories(mcc_p PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(mcc_p PRIVATE -pg)
    target_compile_definitions(mcc_p PRIVATE CONFIG_MCC_STATIC MCC_PROFILE)
    target_link_options(mcc_p PRIVATE -pg -static)
    if(MCC_GITHASH)
        target_compile_definitions(mcc_p PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(mcc_p mcc_mccdefs)
    endif()
endif()

if(MCC_BUILD_SANITIZE)
    add_executable(mcc_s src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    target_include_directories(mcc_s PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(mcc_s PRIVATE -fsanitize=address,undefined)
    target_link_options(mcc_s PRIVATE -fsanitize=address,undefined)
    if(MCC_GITHASH)
        target_compile_definitions(mcc_s PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(mcc_s mcc_mccdefs)
    endif()
endif()

foreach(_nt mcc libmcc mcc_p mcc_s)
    if(TARGET ${_nt})
        target_compile_definitions(${_nt} PRIVATE ${_mccdefs})
    endif()
endforeach()


set(MCC_LIBMCC1_EXTRA_O runmain bt-exe bt-dll bt-log bcheck)

function(_mcc_libmcc1_src obj outvar)
    set(_lib "${CMAKE_CURRENT_SOURCE_DIR}/runtime/lib")
    set(_win "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib")
    if(obj STREQUAL "crt1w")
        set(${outvar} "${_win}/crt1.c" PARENT_SCOPE)
    elseif(obj STREQUAL "wincrt1w")
        set(${outvar} "${_win}/wincrt1.c" PARENT_SCOPE)
    elseif(obj MATCHES "^(crt1|wincrt1|dllcrt1|dllmain|winex)$")
        set(${outvar} "${_win}/${obj}.c" PARENT_SCOPE)
    elseif(obj STREQUAL "chkstk")
        set(${outvar} "${_win}/chkstk.c" PARENT_SCOPE)
    else()
        set(${outvar} "${_lib}/${obj}.c" PARENT_SCOPE)
    endif()
endfunction()

function(mcc_build_libmcc1 tname mcctgt cpu os native archive objdir out_extra)
    set(_common stdatomic atomic builtin alloca alloca-bt complex)
    if(cpu STREQUAL "i386" OR cpu STREQUAL "x86_64")
        set(_objs libmcc1 ${_common})
    elseif(cpu STREQUAL "arm")
        set(_objs libmcc1 armeabi armflush ${_common})
    elseif(cpu STREQUAL "arm64" OR cpu STREQUAL "riscv64")
        set(_objs lib-arm64 ${_common})
    else()
        message(FATAL_ERROR "mcc_build_libmcc1: unknown cpu '${cpu}'")
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
        list(APPEND _objs crt1 crt1w wincrt1 wincrt1w dllcrt1 dllmain winex)
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
        elseif(cpu STREQUAL "arm64")
            list(APPEND _objs armflush)
        elseif(cpu STREQUAL "riscv64")
            list(APPEND _objs lib-riscv)
        endif()
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
        # Build the runtime position-independent (like libgcc) so it links into
        # PIEs without text relocations (musl rejects TEXTREL on i386).
        set(_xflags ${_mccdefs_flags} -fPIC
                    -B${CMAKE_CURRENT_SOURCE_DIR}/runtime
                    -I${CMAKE_BINARY_DIR} ${_arch_incs})
    endif()

    get_filename_component(_extra_dir "${archive}" DIRECTORY)
    file(MAKE_DIRECTORY "${objdir}")

    set(_use_gcc OFF)
    if(native AND MCC_LIBMCC1_USEGCC)
        set(_use_gcc ON)
        set(_gccflags ${_mccdefs_flags}
                      -fPIC -fno-omit-frame-pointer -Wno-unused-function
                      -Wno-unused-variable
                      -I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include -I${CMAKE_BINARY_DIR}
                      ${_arch_incs})
        if(MCC_CONFIG_DWARF STREQUAL "")
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
        _mcc_libmcc1_src("${obj}" _src)
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
        if(obj IN_LIST MCC_LIBMCC1_EXTRA_O)
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

mcc_build_libmcc1(libmcc1 mcc "${MCC_CPU}" "${MCC_TARGETOS}" ON
    "${CMAKE_BINARY_DIR}/libmcc1.a" "${CMAKE_BINARY_DIR}/lib"
    LIBMCC1_EXTRA_OBJS)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${CMAKE_BINARY_DIR}/include" FILES_MATCHING PATTERN "*.h")

if(MCC_BUILD_COVERAGE)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/mcc_c"
        COMMAND ${MCC_EMULATOR} "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
                -o "${CMAKE_BINARY_DIR}/mcc_c" -ftest-coverage
                "-I${CMAKE_BINARY_DIR}" "-I${CMAKE_CURRENT_SOURCE_DIR}"
                "-B${CMAKE_CURRENT_SOURCE_DIR}" "-B${CMAKE_BINARY_DIR}"
        DEPENDS mcc libmcc1 "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
        COMMENT "mcc -ftest-coverage mcc.c -> mcc_c" VERBATIM)
    add_custom_target(mcc_cov ALL DEPENDS "${CMAKE_BINARY_DIR}/mcc_c")
endif()

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

        add_executable(${_t}-mcc src/mcc.c)
        target_compile_definitions(${_t}-mcc PRIVATE ${_cdefs} ${_cpath_defs}
            "CONFIG_MCC_CROSSPREFIX=\"${_t}-\"")
        if(_cos STREQUAL "WIN32")
            target_compile_definitions(${_t}-mcc PRIVATE
                "CONFIG_MCCDIR=\"${_mccdir_install}/win32\"")
        endif()
        if(MCC_GITHASH)
            target_compile_definitions(${_t}-mcc PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
        endif()
        if(TARGET mcc_mccdefs)
            add_dependencies(${_t}-mcc mcc_mccdefs)
        endif()
        install(TARGETS ${_t}-mcc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

        mcc_build_libmcc1(${_t}-libmcc1 ${_t}-mcc "${_ccpu}" "${_cos}" OFF
            "${CMAKE_BINARY_DIR}/${_t}-libmcc1.a"
            "${CMAKE_BINARY_DIR}/lib-${_t}" _cross_extra)
        install(FILES "${CMAKE_BINARY_DIR}/${_t}-libmcc1.a"
            DESTINATION "${_mccdir_install}")
    endforeach()
endif()

if(MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_BUILD_STATIC)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/libmcc.def"
        COMMAND ${MCC_EMULATOR} "$<TARGET_FILE:mcc>" -impdef "$<TARGET_FILE:libmcc>"
                -o "${CMAKE_BINARY_DIR}/libmcc.def"
        DEPENDS mcc libmcc
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

install(TARGETS libmcc EXPORT mccTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
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

install(FILES "${CMAKE_BINARY_DIR}/libmcc1.a" ${LIBMCC1_EXTRA_OBJS}
    DESTINATION "${_mccdir}")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${_mccdir}/include" FILES_MATCHING PATTERN "*.h")

if(MCC_TARGETOS STREQUAL "WIN32")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/include/"
        DESTINATION "${_mccdir}/include")
    file(GLOB _win_defs "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib/*.def")
    install(FILES ${_win_defs} DESTINATION "${_mccdir}/lib")
    if(NOT MCC_BUILD_STATIC)
        install(FILES "${CMAKE_BINARY_DIR}/libmcc.def"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL)
    endif()
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/"
        DESTINATION examples FILES_MATCHING PATTERN "*.c")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/api_basic.c"
        DESTINATION examples)
endif()

if(UNIX AND NOT CMAKE_CROSSCOMPILING)
    add_executable(mccbuild tools/build.c)
endif()

if(MCC_BUILD_TESTS)
    enable_testing()
    set(_mcc_exe "$<TARGET_FILE:mcc>")
    set(_bdir "${CMAKE_BINARY_DIR}")
    set(_idir "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")

    # Register a test that always reports "Skipped" (with a reason) in ctest,
    # so platform-/config-inapplicable tests are visible instead of omitted.
    # Extra args (e.g. LABELS qemu) are forwarded to set_tests_properties.
    function(mcc_skip_test _name _reason)
        add_test(NAME ${_name} COMMAND "${CMAKE_COMMAND}" -E echo "SKIP: ${_reason}")
        set_tests_properties(${_name} PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:" ${ARGN})
    endfunction()


    set(_exe_driver "${CMAKE_BINARY_DIR}/run_mcc_exe.cmake")
    file(WRITE "${_exe_driver}" "set(EMU \"${MCC_EMULATOR}\")\n")
    file(APPEND "${_exe_driver}" [==[
separate_arguments(_srcs UNIX_COMMAND "${SRCS}")
separate_arguments(_rargs UNIX_COMMAND "${RUNARGS}")
execute_process(
    COMMAND ${EMU} "${MCC}" "-B${BDIR}" "-I${IDIR}" ${_srcs} -o "${OUTFILE}"
    RESULT_VARIABLE _crc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _crc EQUAL 0)
    message(FATAL_ERROR "mcc compile failed for ${SRCS}:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${OUTFILE}" ${_rargs} RESULT_VARIABLE _rrc)
if(NOT _rrc EQUAL 0)
    message(FATAL_ERROR "${OUTFILE} exited with ${_rrc}")
endif()
]==])

    set(_mcctest_driver "${CMAKE_BINARY_DIR}/run_mcctest.cmake")
    file(WRITE "${_mcctest_driver}" "set(EMU \"${MCC_EMULATOR}\")\n")
    file(APPEND "${_mcctest_driver}" [==[
set(_ref_exe "${OUTDIR}/mcctest.gcc")
set(_ref_out "${OUTDIR}/mcctest.ref")
separate_arguments(_refflags UNIX_COMMAND "${REFFLAGS}")
separate_arguments(_tdefs UNIX_COMMAND "${TESTDEFS}")
execute_process(
    COMMAND "${CC}" "${SRC}" -o "${_ref_exe}" "-I${IDIR}" "-I${SRCDIR}" "-I${BDIR}"
            ${_tdefs} -w -O0 -std=gnu99 -fno-omit-frame-pointer ${_refflags}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "cc failed to build mcctest.c:\n${_o}${_e}")
endif()
execute_process(COMMAND "${_ref_exe}" OUTPUT_FILE "${_ref_out}")
separate_arguments(_targs UNIX_COMMAND "${MCCARGS}")
set(_mcc_exe "${OUTDIR}/mcctest.mcc")
set(_mcc_out "${OUTDIR}/mcctest.out")
execute_process(
    COMMAND ${EMU} "${MCC}" "-B${BDIR}" "-I${IDIR}" "-I${SRCDIR}" "-I${BDIR}"
            ${_tdefs} ${_targs} "${SRC}" -o "${_mcc_exe}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc failed to build mcctest.c:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${_mcc_exe}" OUTPUT_FILE "${_mcc_out}")
find_program(_diff diff)
execute_process(COMMAND "${_diff}" -u "${_ref_out}" "${_mcc_out}"
    RESULT_VARIABLE _drc OUTPUT_VARIABLE _d)
if(NOT _drc EQUAL 0)
    message(FATAL_ERROR "mcctest mismatch (cc vs mcc):\n${_d}")
endif()
]==])

    set(_asmconnect_driver "${CMAKE_BINARY_DIR}/run_asm_c_connect.cmake")
    file(WRITE "${_asmconnect_driver}" "set(EMU \"${MCC_EMULATOR}\")\n")
    file(APPEND "${_asmconnect_driver}" [==[
set(_1 "${SRCDIR}/asm/asm_c_connect/part1.c")
set(_2 "${SRCDIR}/asm/asm_c_connect/part2.c")
set(_flags "-B${BDIR}" "-I${IDIR}")
execute_process(COMMAND ${EMU} "${MCC}" ${_flags} "${_1}" "${_2}"
        -o "${OUTDIR}/asm-c-connect" RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "asm-c-connect single build failed:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${OUTDIR}/asm-c-connect"
    OUTPUT_FILE "${OUTDIR}/asm-c-connect.out1")
execute_process(COMMAND ${EMU} "${MCC}" ${_flags} -c "${_1}" -o "${OUTDIR}/acc1.o")
execute_process(COMMAND ${EMU} "${MCC}" ${_flags} -c "${_2}" -o "${OUTDIR}/acc2.o")
execute_process(COMMAND ${EMU} "${MCC}" ${_flags}
        "${OUTDIR}/acc1.o" "${OUTDIR}/acc2.o" -o "${OUTDIR}/asm-c-connect-sep"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "asm-c-connect separate link failed:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${OUTDIR}/asm-c-connect-sep"
    OUTPUT_FILE "${OUTDIR}/asm-c-connect.out2")
find_program(_diff diff)
execute_process(COMMAND "${_diff}" -u
        "${OUTDIR}/asm-c-connect.out1" "${OUTDIR}/asm-c-connect.out2"
    RESULT_VARIABLE _drc OUTPUT_VARIABLE _d)
if(NOT _drc EQUAL 0)
    message(FATAL_ERROR "asm-c-connect single vs separate mismatch:\n${_d}")
endif()
]==])


    add_test(NAME mcc_build
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}")
    set_tests_properties(mcc_build PROPERTIES FIXTURES_SETUP MCC_BUILT)

    add_executable(exec_runner tests/exec/runner.c)
    target_include_directories(exec_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec")
    add_test(NAME exec-suite
        COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_BINARY_DIR}")
    # The runner reports its own per-test SKIP lines (host/config requirements);
    # it returns 77 only when every test skipped, which ctest treats as Skipped.
    set_tests_properties(exec-suite PROPERTIES FIXTURES_REQUIRED MCC_BUILT
        SKIP_RETURN_CODE 77
        ENVIRONMENT "MCC_TEST_EMU=${MCC_EMULATOR};MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_ASM=$<IF:$<BOOL:${MCC_CONFIG_ASM}>,1,0>;MCC_TEST_BCHECK=$<IF:$<BOOL:${MCC_CONFIG_BCHECK}>,1,0>;MCC_TEST_BACKTRACE=$<IF:$<BOOL:${MCC_CONFIG_BACKTRACE}>,1,0>")

    # Structural CLI suite: compiles with mcc and inspects the result with
    # readelf/nm (output structure, not just program stdout).  Native only --
    # the readelf/nm assertions assume a host-readable ELF toolchain.
    if(NOT MCC_EMULATOR)
        add_executable(cli_runner tests/cli/runner.c)
        target_include_directories(cli_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli")
        add_test(NAME cli-suite
            COMMAND cli_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_BINARY_DIR}/cli-work" "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli")
        set_tests_properties(cli-suite PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77
            ENVIRONMENT "MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS}")
    else()
        mcc_skip_test(cli-suite "structural readelf/nm suite runs on native host only")
    endif()

    # Three-way differential suite: gcc vs clang vs mcc on every portable "run"
    # golden, comparing program stdout (gcc/clang are the source of truth).
    # Native only, and only when BOTH reference compilers are available.
    find_program(DIFF3_GCC NAMES gcc cc)
    find_program(DIFF3_CLANG NAMES clang)
    if(NOT MCC_EMULATOR AND DIFF3_GCC AND DIFF3_CLANG)
        add_executable(diff3_runner tests/diff3/runner.c)
        target_include_directories(diff3_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec")
        add_test(NAME diff3-suite
            COMMAND diff3_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_BINARY_DIR}/diff3-work"
                    "${DIFF3_GCC}" "${DIFF3_CLANG}")
        set_tests_properties(diff3-suite PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77
            ENVIRONMENT "MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS}")
    else()
        mcc_skip_test(diff3-suite "needs native host + both gcc and clang")
    endif()

    add_executable(libmcc_test tests/embed/api_basic.c)
    target_link_libraries(libmcc_test PRIVATE libmcc)
    target_include_directories(libmcc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME libtest
        COMMAND libmcc_test "-B${CMAKE_BINARY_DIR}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
    set_tests_properties(libtest PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

    # Extra API coverage: list_symbols, undefine_symbol, add_library,
    # mcc_run argv/return-code, and mcc_output_file (MCC_OUTPUT_OBJ).
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
        add_test(NAME hello-exe COMMAND "${CMAKE_COMMAND}"
            -DMCC=${_mcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCS=${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c
            -DOUTFILE=${CMAKE_BINARY_DIR}/hello${MCC_EXESUF} -P "${_exe_driver}")
        set_tests_properties(hello-exe PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/vla.c")
        add_test(NAME vla_test-run COMMAND "${CMAKE_COMMAND}"
            -DMCC=${_mcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCS=${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/vla.c
            -DOUTFILE=${CMAKE_BINARY_DIR}/vla_test${MCC_EXESUF} -P "${_exe_driver}")
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

    set(_mcctest_refflags "")
    if(MCC_TARGETOS STREQUAL "WIN32")
        set(_msvcrt_start "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/msvcrt_start.c")
        if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
            set(_mcctest_refflags "-nostdlib ${_msvcrt_start} -lkernel32 msvcrt.lib")
        else()
            set(_mcctest_refflags
                "-nostdlib ${_msvcrt_start} -lgcc -lkernel32 c:/windows/system32/msvcrt.dll")
        endif()
    elseif(MCC_CPU STREQUAL "arm")
        set(_mcctest_refflags "-marm")
    elseif(MCC_CPU STREQUAL "i386")
        set(_mcctest_refflags "-fno-PIC -fno-PIE -Wl,-z,notext")
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c" AND MCC_CONFIG_ASM)
        add_test(NAME mcctest COMMAND "${CMAKE_COMMAND}"
            -DCC=${CMAKE_C_COMPILER} -DMCC=${_mcc_exe}
            -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c
            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DOUTDIR=${CMAKE_BINARY_DIR} "-DREFFLAGS=${_mcctest_refflags}"
            "-DTESTDEFS=-DCC_NAME=CC_${MCC_CC_NAME} -DGCC_MAJOR=${MCC_GCC_MAJOR}"
            -P "${_mcctest_driver}")
        set_tests_properties(mcctest PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        if(MCC_CONFIG_BCHECK)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bcheck")
            add_test(NAME mcctest-bcheck COMMAND "${CMAKE_COMMAND}"
                -DCC=${CMAKE_C_COMPILER} -DMCC=${_mcc_exe}
                -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -DBDIR=${_bdir} -DIDIR=${_idir}
                -DOUTDIR=${CMAKE_BINARY_DIR}/bcheck "-DREFFLAGS=${_mcctest_refflags}"
                "-DTESTDEFS=-DCC_NAME=CC_${MCC_CC_NAME} -DGCC_MAJOR=${MCC_GCC_MAJOR}"
                -DMCCARGS=-b -P "${_mcctest_driver}")
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
        add_test(NAME asm-c-connect-test COMMAND "${CMAKE_COMMAND}"
            -DMCC=${_mcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests -DOUTDIR=${CMAKE_BINARY_DIR}
            -P "${_asmconnect_driver}")
        set_tests_properties(asm-c-connect-test PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    else()
        mcc_skip_test(asm-c-connect-test
            "requires x86 target with integrated assembler (host target: ${MCC_CPU})")
    endif()

    set(MCC_GCCTESTSUITE_PATH "" CACHE PATH
        "gcc.c-torture directory for the `gcctestsuite` target")
    set(MCC_ARM_CROSS_COMPILE "" CACHE STRING
        "binutils prefix (e.g. arm-linux-gnueabi-) for `arm-asm-testsuite`")

    set(_fastcall_drv "${CMAKE_BINARY_DIR}/run_i386_fastcall.cmake")
    file(WRITE "${_fastcall_drv}" "set(BUILDDIR \"${CMAKE_BINARY_DIR}\")\n")
    file(APPEND "${_fastcall_drv}" [==[
if(NOT DEFINED GCC OR GCC STREQUAL "")
    set(GCC gcc)
endif()
find_program(_gcc "${GCC}")
if(NOT _gcc)
    message(STATUS "SKIP: no gcc")
    return()
endif()
set(_w "${BUILDDIR}/i386-fastcall-abi")
file(REMOVE_RECURSE "${_w}")
file(MAKE_DIRECTORY "${_w}")
file(WRITE "${_w}/m32probe.c" "int main(void){return 0;}\n")
execute_process(COMMAND "${_gcc}" -m32 "${_w}/m32probe.c" -o "${_w}/m32probe"
    RESULT_VARIABLE _m32 OUTPUT_QUIET ERROR_QUIET)
if(NOT _m32 EQUAL 0)
    message(STATUS "SKIP: gcc -m32 unavailable (no 32-bit multilib)")
    return()
endif()
if(NOT DEFINED IMCC OR NOT EXISTS "${IMCC}")
    message(STATUS "SKIP: i386 mcc not found at '${IMCC}'")
    return()
endif()
file(WRITE "${_w}/callee.c" "\
struct P2 { int x, y; };
int __attribute__((fastcall)) mix_ll(int a, long long b, int c){ return (int)(a+b+c); }
int __attribute__((fastcall)) small(char a, short b, int c){ return a+b+c; }
int __attribute__((fastcall)) ptr2(int *a, int *b){ return *a + *b; }
int __attribute__((fastcall)) ll_first(long long a, int b){ return (int)(a+b); }
int __attribute__((fastcall)) fs(int a, struct P2 p, int b){ return a*1000+p.x*100+p.y*10+b; }
")
file(WRITE "${_w}/caller.c" "\
struct P2 { int x, y; };
int __attribute__((fastcall)) mix_ll(int a, long long b, int c);
int __attribute__((fastcall)) small(char a, short b, int c);
int __attribute__((fastcall)) ptr2(int *a, int *b);
int __attribute__((fastcall)) ll_first(long long a, int b);
int __attribute__((fastcall)) fs(int a, struct P2 p, int b);
int main(void){
    int x=10, y=20, f=0; struct P2 p={2,3};
    if (mix_ll(1,100,3)!=104) f|=1;
    if (small(1,2,3)!=6) f|=2;
    if (ptr2(&x,&y)!=30) f|=4;
    if (ll_first(100,5)!=105) f|=8;
    if (fs(1,p,4)!=1234) f|=16;
    return f;
}
")
set(_fail 0)
macro(_cc cc out)
    execute_process(COMMAND ${cc} ${ARGN} -o "${out}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if(NOT _rc EQUAL 0)
        message(WARNING "compile failed: ${cc} ${ARGN}\n${_o}${_e}")
        set(_fail 1)
    endif()
endmacro()
_cc("${IMCC}" "${_w}/e_t.o" -c "${_w}/callee.c")
_cc("${IMCC}" "${_w}/l_t.o" -c "${_w}/caller.c")
_cc("${_gcc}" "${_w}/e_g.o" -m32 -O0 -c "${_w}/callee.c")
_cc("${_gcc}" "${_w}/l_g.o" -m32 -O0 -c "${_w}/caller.c")
macro(_check name)
    execute_process(COMMAND "${_gcc}" -m32 ${ARGN} -o "${_w}/run"
        RESULT_VARIABLE _lrc OUTPUT_QUIET ERROR_QUIET)
    if(NOT _lrc EQUAL 0)
        message("FAIL  ${name} (link)")
        set(_fail 1)
    else()
        execute_process(COMMAND "${_w}/run" RESULT_VARIABLE _rrc)
        if(_rrc EQUAL 0)
            message("PASS  ${name}")
        else()
            message("FAIL  ${name} (exit ${_rrc})")
            set(_fail 1)
        endif()
    endif()
endmacro()
_check("mcc caller -> gcc callee" "${_w}/e_g.o" "${_w}/l_t.o")
_check("gcc caller -> mcc callee" "${_w}/e_t.o" "${_w}/l_g.o")
_check("mcc caller -> mcc callee" "${_w}/e_t.o" "${_w}/l_t.o")
file(WRITE "${_w}/unsup.c"
    "int __attribute__((fastcall)) f(double a,int b); int g(){ return f(1.0,2); }\n")
execute_process(COMMAND "${IMCC}" -c "${_w}/unsup.c" -o "${_w}/unsup.o"
    RESULT_VARIABLE _urc OUTPUT_QUIET ERROR_QUIET)
if(_urc EQUAL 0)
    message("FAIL  unsupported float-before-reg case should error")
    set(_fail 1)
else()
    message("PASS  unsupported float-before-reg case errors cleanly")
endif()
if(_fail EQUAL 0)
    message("ALL OK")
else()
    message(FATAL_ERROR "i386-fastcall-abi: FAILURES")
endif()
]==])
    if(TARGET i386-mcc)
        add_test(NAME i386-fastcall-abi COMMAND "${CMAKE_COMMAND}"
            -DIMCC=$<TARGET_FILE:i386-mcc> -P "${_fastcall_drv}")
        set_tests_properties(i386-fastcall-abi PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    else()
        mcc_skip_test(i386-fastcall-abi
            "requires an i386 cross-compiler (target i386-mcc is not configured)")
    endif()

    set(_gccts_drv "${CMAKE_BINARY_DIR}/run_gcctestsuite.cmake")
    file(WRITE "${_gccts_drv}" "set(BUILDDIR \"${CMAKE_BINARY_DIR}\")\n")
    file(APPEND "${_gccts_drv}" [==[
if(NOT DEFINED TESTSUITE_PATH OR TESTSUITE_PATH STREQUAL "")
    set(TESTSUITE_PATH "$ENV{TESTSUITE_PATH}")
endif()
if(TESTSUITE_PATH STREQUAL "" OR NOT IS_DIRECTORY "${TESTSUITE_PATH}")
    message(STATUS "SKIP: gcc testsuite not found (set MCC_GCCTESTSUITE_PATH)")
    return()
endif()
if(NOT DEFINED MCC OR NOT EXISTS "${MCC}")
    message(FATAL_ERROR "gcctestsuite: -DMCC=<mcc binary> required")
endif()
if(NOT DEFINED BDIR OR BDIR STREQUAL "")
    get_filename_component(BDIR "${MCC}" DIRECTORY)
endif()
set(_rt "${BUILDDIR}/gcctestsuite")
file(MAKE_DIRECTORY "${_rt}")
set(_flags "-B${BDIR}" -DNO_TRAMPOLINES)
if(DEFINED IDIR AND NOT IDIR STREQUAL "")
    list(APPEND _flags "-I${IDIR}" "-I${IDIR}/include")
endif()
file(GLOB _all "${TESTSUITE_PATH}/compile/*.c" "${TESTSUITE_PATH}/execute/*.c"
               "${TESTSUITE_PATH}/execute/ieee/*.c")
set(_skip "")
foreach(_f IN LISTS _all)
    file(READ "${_f}" _t)
    if(_t MATCHES "_builtin_" OR _t MATCHES "[_a-zA-Z]?[Cc]omplex"
       OR _t MATCHES "__u?int128_t" OR _t MATCHES "vector|vector_size|__vector_size__")
        get_filename_component(_b "${_f}" NAME)
        list(APPEND _skip "${_b}")
    endif()
endforeach()
list(APPEND _skip 20000120-2.c mipscop-1.c mipscop-2.c mipscop-3.c mipscop-4.c
     fp-cmp-4f.c fp-cmp-4l.c fp-cmp-8f.c fp-cmp-8l.c pr38016.c)
set(_ok 0)
set(_sk 0)
set(_fa 0)
set(_xf 0)
set(_sum "")
file(GLOB _csrc "${TESTSUITE_PATH}/compile/*.c")
foreach(_s IN LISTS _csrc)
    get_filename_component(_base "${_s}" NAME)
    execute_process(COMMAND "${MCC}" ${_flags} -o "${_rt}/tst.o" -c "${_s}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if("${_o}${_e}" MATCHES "cannot use local functions")
        set(_r SKIP)
        math(EXPR _sk "${_sk}+1")
    elseif(_rc EQUAL 0)
        set(_r PASS)
        math(EXPR _ok "${_ok}+1")
    elseif(_base IN_LIST _skip)
        set(_r SKIP)
        math(EXPR _sk "${_sk}+1")
    else()
        set(_r FAIL)
        math(EXPR _fa "${_fa}+1")
    endif()
    string(APPEND _sum "${_r}: ${_s}\n")
endforeach()
file(GLOB _xsrc "${TESTSUITE_PATH}/execute/*.c" "${TESTSUITE_PATH}/execute/ieee/*.c")
foreach(_s IN LISTS _xsrc)
    get_filename_component(_base "${_s}" NAME)
    execute_process(COMMAND "${MCC}" ${_flags} "${_s}" -o "${_rt}/tst" -lm
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if("${_o}${_e}" MATCHES "cannot use local functions")
        set(_r SKIP)
        math(EXPR _sk "${_sk}+1")
    elseif(_rc EQUAL 0)
        execute_process(COMMAND "${_rt}/tst" RESULT_VARIABLE _xrc OUTPUT_QUIET ERROR_QUIET)
        if(_xrc EQUAL 0)
            set(_r PASS)
            math(EXPR _ok "${_ok}+1")
        else()
            set(_r FAILEXE)
            math(EXPR _xf "${_xf}+1")
        endif()
    elseif(_base IN_LIST _skip)
        set(_r SKIP)
        math(EXPR _sk "${_sk}+1")
    else()
        set(_r FAIL)
        math(EXPR _fa "${_fa}+1")
    endif()
    string(APPEND _sum "${_r}: ${_s}\n")
endforeach()
file(WRITE "${BUILDDIR}/mcc.sum" "${_sum}${_ok} ok\n${_sk} skipped\n${_fa} failed\n${_xf} exe failed\n")
message("${_ok} test(s) ok.")
message("${_sk} test(s) skipped.")
message("${_fa} test(s) failed.")
message("${_xf} test(s) exe failed.")
]==])
    add_custom_target(gcctestsuite
        COMMAND "${CMAKE_COMMAND}" -DMCC=$<TARGET_FILE:mcc>
                "-DBDIR=${CMAKE_BINARY_DIR}" "-DIDIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DTESTSUITE_PATH=${MCC_GCCTESTSUITE_PATH}" -P "${_gccts_drv}"
        DEPENDS mcc USES_TERMINAL
        COMMENT "gcctestsuite: gcc c-torture over mcc")

    set(_armasm_drv "${CMAKE_BINARY_DIR}/run_arm_asm.cmake")
    file(WRITE "${_armasm_drv}" "set(BUILDDIR \"${CMAKE_BINARY_DIR}\")\n")
    file(APPEND "${_armasm_drv}" [==[
if(NOT DEFINED CROSS_COMPILE)
    set(CROSS_COMPILE "")
endif()
if(NOT DEFINED ARM_VFP)
    set(ARM_VFP ON)
endif()
find_program(AS "${CROSS_COMPILE}as")
find_program(OBJDUMP "${CROSS_COMPILE}objdump")
if(NOT AS OR NOT OBJDUMP)
    message(STATUS "SKIP: ARM binutils (${CROSS_COMPILE}as/objdump) not found")
    return()
endif()
if(NOT DEFINED MCC OR NOT EXISTS "${MCC}")
    message(STATUS "SKIP: ARM mcc not found at '${MCC}'")
    return()
endif()
if(NOT EXISTS "${TOKH}")
    message(FATAL_ERROR "arm-tok.h not found at '${TOKH}'")
endif()
set(_w "${BUILDDIR}/arm-asm-testsuite")
file(REMOVE_RECURSE "${_w}")
file(MAKE_DIRECTORY "${_w}")
file(STRINGS "${TOKH}" _lines)
set(_mnem "")
foreach(_l IN LISTS _lines)
    if(NOT _l MATCHES "DEF_ASM")
        continue()
    endif()
    if(_l MATCHES "not useful" OR _l MATCHES "#define" OR _l MATCHES "/\\*"
       OR _l MATCHES "DEF_ASM_CONDED_WITH_SUFFIX\\(x")
        continue()
    endif()
    if(_l MATCHES "DEF_ASM_CONDED_VFP_F32_F64[^(]*\\((.*)\\)")
        list(APPEND _mnem "${CMAKE_MATCH_1}.f32" "${CMAKE_MATCH_1}.f64")
    elseif(_l MATCHES "DEF_ASM[^(]*\\((.*)\\)")
        string(REPLACE ", " "." _name "${CMAKE_MATCH_1}")
        list(APPEND _mnem "${_name}")
    endif()
endforeach()
set(_f2 "")
foreach(_s IN LISTS _mnem)
    if(NOT _s MATCHES "^([rcpsd][0-9]+|fp|ip|sp|lr|pc|asl|apsr_nzcv|fpsid|fpscr|fpexc)$")
        list(APPEND _f2 "${_s}")
    endif()
endforeach()
set(_mnem "${_f2}")
set(_combos
    "r3, r4, r5, r6" "r3, r4, r5" "r3, r4, r5, asl #7" "r3, r4, r5, lsl #7"
    "r3, r4, r5, asr #7" "r3, r4, r5, lsr #7" "r3, r4, r5, ror #7"
    "r3, r4, r5, rrx" "r3, r4, r5, asl r6" "r3, r4, r5, lsl r6"
    "r3, r4, r5, asr r6" "r3, r4, r5, lsr r6" "r3, r4, r5, ror r6"
    "r3, r4, #5, asl #7" "r3, r4, #5, lsl #7" "r3, r4, #5, asr #7"
    "r3, r4, #5, lsr #7" "r3, r4, #5, ror #7" "r3, r4, #5, rrx" "r3, #5, r4"
    "r3, #4, #8" "r3, r4, asl #5" "r3, r4, lsl #5" "r3, r4, asr #5"
    "r3, r4, lsr #5" "r3, r4, ror #5" "r3, r4, ror #8" "r3, r4, asl r5"
    "r3, r4, lsl r5" "r3, r4, asr r5" "r3, r4, lsr r5" "r3, r4, ror r5"
    "r3, r4, ror #8" "r3, r4, ror #16" "r3, r4, ror #24" "r3, r4, rrx"
    "r3, #4, asl #5" "r3, #4, lsl #5" "r3, #4, asr #5" "r3, #4, lsr #5"
    "r3, #4, ror #5" "r3, r4, rrx" "r3, r4" "r3" "{r3,r4,r5}" "{r3,r5,r4}"
    "r2!, {r3,r4,r5}" "r2!, {r3,r5,r4}" "r2, {r3,r4,r5}" "r2, {r3,r5,r4}"
    "r2, [r3, r4]" "r2, [r3, r4]!" "r2, [r3, -r4]" "r2, [r3, -r4]!"
    "r2, [r3], r4" "r2, [r3], -r4" "r2, [r3]" "r2, r3, [r4, lsl# 2]"
    "r2, [r3, r4, lsr# 1]" "r2, [r3, r4, lsr# 2]!" "r2, [r3, -r4, ror# 3]"
    "r2, [r3, -r4, lsl# 1]!" "r2, [r3], r4, lsl# 3" "r2, [r3], -r4, asr# 31"
    "r2, [r3], -r4, asl# 1" "r2, [r3], -r4, rrx" "r2, r3, [r4]"
    "r2, [r3, #4]" "r2, [r3, #-4]" "r2, [r3, #0x45]" "r2, [r3, #-0x45]"
    "r2, r3, #4" "r2, r3, #-4" "p10, #7, c2, c0, c1, #4"
    "p10, #7, r2, c0, c1, #4" "p10, #0, c2, c0, c1, #4" "p10, #0, r2, c0, c1, #4"
    "r2, #4" "r2, #-4" "r2, #0xEFFF" "r3, #0x0000" "r4, #0x0201"
    "r4, #0xFFFFFF00" "#4" "#-4" "p5, c2, [r3]" "p5, c3, [r4]"
    "p5, c2, [r3, #4]" "p5, c2, [r3, #-4]" "p5, c2, [r3, #0x45]"
    "p5, c2, [r3, #-0x45]" "s2, [r3]" "s3, [r4]" "s2, [r3, #4]"
    "s2, [r3, #-4]" "s2, [r3, #0x45]" "s2, [r3, #-0x45]" "r1, {d3-d4}"
    "r1!, {d3-d4}" "r2, {d4-d15}" "r3!, {d4-d15}" "r3!, {d4}" "r2, {s4-s31}"
    "r3!, {s4}" "{d3-d4}" "{d4-d15}" "{d4}" "{s4-s31}" "{s4}" "s2, s3, s4"
    "s2, s3" "d2, d3, d4" "d2, d3" "s2, #0" "d2, #0" "s3, #0.0" "d3, #0.0"
    "s4, #-0.1796875" "d4, #0.1796875" "r2, r3, d1" "d1, r2, r3" "s1, r2"
    "r2, s1" "r2, fpexc" "r2, fpscr" "r2, fpsid" "apsr_nzcv, fpscr"
    "fpexc, r2" "fpscr, r2" "fpsid, r2" "s3, d4" "d4, s3" "")
function(_dump obj outvar)
    execute_process(COMMAND "${OBJDUMP}" -S "${obj}"
        OUTPUT_VARIABLE _od ERROR_QUIET)
    set(_keep "")
    string(REPLACE "\n" ";" _odl "${_od}")
    foreach(_ln IN LISTS _odl)
        if(_ln MATCHES "^[ \t]*0:")
            string(APPEND _keep "${_ln}\n")
        endif()
    endforeach()
    set(${outvar} "${_keep}" PARENT_SCOPE)
endfunction()
set(_total 0)
set(_okc 0)
set(_failed "")
foreach(_s IN LISTS _mnem)
    set(_as_opts "")
    if(_s MATCHES "^v")
        if(ARM_VFP)
            set(_as_opts -mfpu=vfp)
        else()
            continue()
        endif()
    endif()
    foreach(_args IN LISTS _combos)
        set(_text "${_s} ${_args}")
        file(WRITE "${_w}/in.s" "${_text}\n")
        execute_process(COMMAND "${AS}" -mlittle-endian ${_as_opts}
            -o "${_w}/as.o" "${_w}/in.s" RESULT_VARIABLE _arc OUTPUT_QUIET ERROR_QUIET)
        if(NOT _arc EQUAL 0)
            continue()
        endif()
        math(EXPR _total "${_total}+1")
        _dump("${_w}/as.o" _expected)
        file(WRITE "${_w}/in.c" "__asm__(\"${_text}\");\n")
        execute_process(COMMAND "${MCC}" -o "${_w}/mcc.o" -c "${_w}/in.c"
            RESULT_VARIABLE _trc OUTPUT_QUIET ERROR_QUIET)
        if(NOT _trc EQUAL 0)
            message("warning: '${_text}' did not work in mcc")
            continue()
        endif()
        _dump("${_w}/mcc.o" _got)
        if(_got STREQUAL _expected)
            math(EXPR _okc "${_okc}+1")
        else()
            list(APPEND _failed "${_text}")
            message("warning: '${_text}' did not match GNU as")
        endif()
    endforeach()
endforeach()
message("${_okc} of ${_total} tests succeeded.")
set(_known "bl r3" "b r3" "mov r2, #0xEFFF" "mov r4, #0x0201"
    "vmov.f32 r2, r3, d1" "vmov.f32 d1, r2, r3")
set(_st 0)
foreach(_t IN LISTS _failed)
    if(_t IN_LIST _known)
        message("Failed test: ${_t} (known failure)")
    else()
        message("Failed test: ${_t}")
        set(_st 1)
    endif()
endforeach()
if(NOT _st EQUAL 0)
    message(FATAL_ERROR "arm-asm-testsuite: FAILURES")
endif()
]==])
    if(TARGET arm-mcc)
        add_custom_target(arm-asm-testsuite
            COMMAND "${CMAKE_COMMAND}" -DMCC=$<TARGET_FILE:arm-mcc>
                    "-DCROSS_COMPILE=${MCC_ARM_CROSS_COMPILE}"
                    "-DTOKH=${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm/arm-tok.h" -P "${_armasm_drv}"
            DEPENDS arm-mcc USES_TERMINAL
            COMMENT "arm-asm-testsuite: mcc vs GNU as over arm-tok.h")
    endif()

    file(GLOB _orphan_srcs CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/bounds_stress.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/floating_point.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/compile_errors.c")
    foreach(_f IN LISTS _orphan_srcs)
        get_filename_component(_n "${_f}" NAME_WE)
        add_test(NAME compile.${_n} COMMAND ${MCC_EMULATOR} "${_mcc_exe}"
            "-B${_bdir}" "-I${_idir}" -c "${_f}"
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

    # ------------------------------------------------------------------ #
    # PE (Windows) RUNTIME conformance under wine. The win32 cross compilers
    # are built by the 'cross' preset; this links tests/qemu/conformance/*.c
    # into PE executables and runs them under wine. The driver self-skips
    # (exit 0) when wine or the win32 cross compilers are unavailable, so it is
    # safe to register unconditionally. MCC_CROSS_DIR points at the cross build
    # tree (default: <source>/cmake-build-cross).
    find_program(MCC_WINE NAMES wine wine64 wine64-proton-10.0.4)
    set(MCC_CROSS_DIR "${CMAKE_SOURCE_DIR}/cmake-build-cross"
        CACHE PATH "cross-compiler build dir for the wine PE runtime test")
    if(MCC_WINE AND EXISTS "${MCC_CROSS_DIR}/x86_64-win32-mcc")
        add_test(NAME pe-wine-conformance
            COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/run_pe_wine.sh"
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/pe-wine-work")
        set_tests_properties(pe-wine-conformance PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "wine")
    else()
        mcc_skip_test(pe-wine-conformance
            "requires wine + the win32 cross compilers (cmake --preset cross)")
    endif()

    # ------------------------------------------------------------------ #
    # Mach-O (Darwin) structural validation. Executing Mach-O needs macOS or
    # darling (absent off-Darwin), so this links the codegen-self-contained
    # conformance programs into Mach-O executables and confirms a Mach-O parser
    # (llvm-otool/otool) reads the header and all load commands. Self-skips
    # when a parser or the osx cross compilers are unavailable.
    find_program(MCC_OTOOL NAMES llvm-otool otool)
    if(MCC_OTOOL AND EXISTS "${MCC_CROSS_DIR}/x86_64-osx-mcc")
        add_test(NAME macho-structural
            COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/validate_macho.sh"
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-work")
        set_tests_properties(macho-structural PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "macho")
    else()
        mcc_skip_test(macho-structural
            "requires llvm-otool/otool + the osx cross compilers (cmake --preset cross)")
    endif()

    # Mach-O (Darwin) codegen RUNTIME execution. A Mach-O binary can't be loaded
    # on Linux, but mcc's osx `-c` emits the Darwin-target machine code in an ELF
    # object; on an x86_64 host (identical SysV ABI) we link the self-checking
    # conformance programs into a Linux ELF (via a libc trampoline shim + mcc's
    # osx runtime objects) and run them, executing the exact code mcc generates
    # for Darwin. Self-skips off-x86_64 or when the osx compiler is absent.
    if(EXISTS "${MCC_CROSS_DIR}/x86_64-osx-mcc")
        add_test(NAME macho-codegen-run
            COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/run_macho_codegen.sh"
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-codegen-work")
        set_tests_properties(macho-codegen-run PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "macho")
    else()
        mcc_skip_test(macho-codegen-run
            "requires the x86_64-osx cross compiler (cmake --preset cross)")
    endif()

    # Mach-O (Darwin) IMAGE execution. A minimal in-repo Mach-O loader
    # (tests/qemu/macho/loader.c) maps an mcc-produced Mach-O executable's
    # segments, seccomp-traps the macOS syscalls, and jumps to LC_MAIN — so the
    # actual Mach-O binary is loaded and run on Linux/x86_64 (no macOS/darling).
    # Self-skips off-x86_64 or when the osx compiler / seccomp are unavailable.
    if(EXISTS "${MCC_CROSS_DIR}/x86_64-osx-mcc")
        add_test(NAME macho-image-run
            COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/run_macho_image.sh"
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-image-work")
        set_tests_properties(macho-image-run PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "macho")
    else()
        mcc_skip_test(macho-image-run
            "requires the x86_64-osx cross compiler (cmake --preset cross)")
    endif()

    # Mach-O against Apple's REAL libc. tests/qemu/apple-libc/src/*.c are verbatim
    # copies of the C library that ships in macOS's libSystem (apple-oss-
    # distributions/Libc); this compiles them with mcc for x86_64 Darwin, links
    # them into a Mach-O image, and executes that image through the in-repo loader
    # on Linux/x86_64. It is the closest achievable approximation off-macOS to
    # exercising Mach-O against its actual target libc (the kernel-fused remainder
    # of libSystem — commpage asm, Mach-VM malloc, Darwin stdio — needs a
    # macOS/darling host; see tests/qemu/apple-libc/PROVENANCE.md).
    if(EXISTS "${MCC_CROSS_DIR}/x86_64-osx-mcc")
        add_test(NAME macho-apple-libc
            COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/run_macho_apple_libc.sh"
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-apple-libc-work")
        set_tests_properties(macho-apple-libc PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "macho")
    else()
        mcc_skip_test(macho-apple-libc
            "requires the x86_64-osx cross compiler (cmake --preset cross)")
    endif()

    # The kernel-fused remainder of libSystem (libmalloc, locale/FILE stdio,
    # dyld, pthread, GCD, ObjC) cannot be exercised off-macOS: verified
    # empirically — `mcc -c` of Apple's libmalloc stops at internal.h:32 with a
    # hard #error and pulls ~34 Apple-private headers (mach/*, os/lock_private,
    # os/tsd, mach-o/dyld_priv, corecrypto, machine/cpu_capabilities=commpage,
    # ptrauth); Libc's locale stdio is bound to xlocale/__v2printf/__sFILE/gdtoa.
    # Running these requires a macOS or darling host (see PROVENANCE.md). This
    # permanent, reasoned skip keeps that boundary visible in ctest output rather
    # than silent. Set MCC_DARWIN_HOST=ON on an actual macOS/darling host to wire
    # the full-libSystem suite here.
    option(MCC_DARWIN_HOST "Tests run on a macOS/darling host with real libSystem" OFF)
    if(NOT MCC_DARWIN_HOST)
        mcc_skip_test(macho-libsystem-kernel-fused
            "libmalloc/locale-stdio/dyld/pthread/GCD/ObjC need a macOS or darling host (-DMCC_DARWIN_HOST=ON); kernel-fused, no portable variant — see tests/qemu/apple-libc/PROVENANCE.md")
    endif()

    # ------------------------------------------------------------------ #
    # qemu-user cross-target conformance matrix (opt-in: -DMCC_QEMU_TESTS=ON)
    #
    # For each (arch x libc) it downloads a minimal Gentoo stage3 rootfs (glibc
    # and musl variants), cross-compiles the self-checking tests/qemu/conformance
    # programs against that sysroot, and runs them under qemu-<arch> -L <rootfs>.
    # Everything is gated behind the cache flag and labelled "qemu" so the normal
    # ctest run stays fast and offline. All sources/URLs are cache variables.
    # ------------------------------------------------------------------ #
    option(MCC_QEMU_TESTS
        "Download Gentoo stage3 rootfs and run cross conformance tests under qemu-user" OFF)
    if(MCC_QEMU_TESTS)
        set(MCC_QEMU_MIRROR "https://distfiles.gentoo.org/releases" CACHE STRING
            "Base URL of the Gentoo releases mirror (…/releases)")
        set(MCC_QEMU_ARCHS "x86_64;i386;arm;arm64;riscv64" CACHE STRING
            "Architectures to exercise under qemu-user")
        set(MCC_QEMU_LIBCS "glibc;musl" CACHE STRING
            "C libraries to exercise (glibc;musl)")
        set(MCC_QEMU_DLDIR "${CMAKE_BINARY_DIR}/qemu-roots" CACHE PATH
            "Where downloaded rootfs trees are extracted")

        set(_qfetch "${CMAKE_BINARY_DIR}/run_qemu_fetch.cmake")
        file(WRITE "${_qfetch}" [==[
# Resolve a Gentoo latest-stage3-*.txt pointer, download the stage3 tarball and
# extract it. Idempotent: a MARKER file short-circuits repeat runs.
if(EXISTS "${MARKER}")
    return()
endif()
file(DOWNLOAD "${PTRURL}" "${DEST}.ptr" STATUS _st TIMEOUT 120)
list(GET _st 0 _code)
if(NOT _code EQUAL 0)
    message(FATAL_ERROR "fetch pointer failed (${_st}): ${PTRURL}")
endif()
# The latest-stage3-*.txt pointer is a PGP-clearsigned document; the path we
# want is the first whitespace-delimited token on the line naming the tarball.
file(STRINGS "${DEST}.ptr" _lines)
set(_rel "")
foreach(_l IN LISTS _lines)
    if(_l MATCHES "\\.tar\\.(xz|bz2|gz)")
        string(REGEX MATCH "^[^ \t]+" _rel "${_l}")
        break()
    endif()
endforeach()
if(_rel STREQUAL "")
    message(FATAL_ERROR "could not parse stage3 path from ${PTRURL}")
endif()
file(MAKE_DIRECTORY "${DEST}")
message(STATUS "qemu-roots: downloading ${MIRRORBASE}/${_rel}")
file(DOWNLOAD "${MIRRORBASE}/${_rel}" "${DEST}.tar" STATUS _st2 SHOW_PROGRESS TIMEOUT 1800)
list(GET _st2 0 _code2)
if(NOT _code2 EQUAL 0)
    message(FATAL_ERROR "fetch stage3 failed (${_st2}): ${MIRRORBASE}/${_rel}")
endif()
# A Gentoo stage3 contains /dev device nodes; `cmake -E tar` aborts on those as
# a non-root user, so prefer system tar (skip dev/, don't chown). We only need
# the headers/libs anyway.
find_program(_TAR NAMES tar)
if(_TAR)
    execute_process(COMMAND "${_TAR}" --no-same-owner --exclude=./dev/*
        -xpf "${DEST}.tar" -C "${DEST}" RESULT_VARIABLE _xrc)
else()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${DEST}.tar"
        WORKING_DIRECTORY "${DEST}" RESULT_VARIABLE _xrc)
endif()
if(NOT _xrc EQUAL 0)
    message(FATAL_ERROR "extract failed for ${DEST}.tar")
endif()
file(REMOVE "${DEST}.tar")
file(WRITE "${MARKER}" "${_rel}\n")
]==])

        set(_qrun "${CMAKE_BINARY_DIR}/run_qemu_run.cmake")
        file(WRITE "${_qrun}" [==[
# Cross-compile every tests/qemu/conformance/*.c against SYSROOT with MCC and
# run it under QEMU -L SYSROOT; each program self-checks and must exit 0. Each
# program is built twice: default codegen and -fPIC -pie (runtime PIC).
file(MAKE_DIRECTORY "${WORKDIR}")
file(GLOB _srcs "${SRCDIR}/*.c")
set(_fails "")
# lib64 first: on a multilib (amd64) stage3, /lib holds the 32-bit libc.so
# linker script and would mis-link otherwise.
set(_common "-B${MCCBASE}" "--sysroot=${SYSROOT}" "-isystem" "${SYSROOT}/usr/include"
    "-L${SYSROOT}/usr/lib64" "-L${SYSROOT}/lib64" "-L${SYSROOT}/usr/lib" "-L${SYSROOT}/lib")
foreach(_src IN LISTS _srcs)
    get_filename_component(_n "${_src}" NAME_WE)
    foreach(_mode "default" "pic")
        if(_mode STREQUAL "pic")
            set(_modeflags -fPIC -pie)
        else()
            set(_modeflags "")
        endif()
        set(_out "${WORKDIR}/${_n}.${_mode}")
        execute_process(
            COMMAND "${MCC}" ${_common} ${_modeflags} "${_src}" -o "${_out}"
            RESULT_VARIABLE _crc OUTPUT_VARIABLE _co ERROR_VARIABLE _ce)
        if(NOT _crc EQUAL 0)
            string(APPEND _fails "  ${_n} [${_mode}]: compile failed\n${_co}${_ce}\n")
            continue()
        endif()
        execute_process(COMMAND "${QEMU}" -L "${SYSROOT}" "${_out}"
            RESULT_VARIABLE _rrc OUTPUT_VARIABLE _ro ERROR_VARIABLE _re)
        if(NOT _rrc EQUAL 0)
            string(APPEND _fails "  ${_n} [${_mode}]: run exited ${_rrc}\n${_ro}${_re}\n")
        endif()
    endforeach()
endforeach()
if(NOT _fails STREQUAL "")
    message(FATAL_ERROR "qemu conformance failures:\n${_fails}")
endif()
message(STATUS "qemu conformance: all programs passed")
]==])

        set(_qsrc "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/conformance")
        foreach(_arch IN LISTS MCC_QEMU_ARCHS)
            # arch -> (gentoo autobuilds subdir, qemu binary, glibc ptr, musl ptr)
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

            # Pick the compiler: native mcc for the host arch, else <arch>-mcc.
            if(_arch STREQUAL MCC_CPU)
                set(_qcc "$<TARGET_FILE:mcc>")
            elseif(TARGET ${_arch}-mcc)
                set(_qcc "$<TARGET_FILE:${_arch}-mcc>")
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
                        "no compiler for ${_arch} (native target is ${MCC_CPU}; build ${_arch}-mcc via -DMCC_ENABLE_CROSS=ON)"
                        LABELS qemu)
                    continue()
                endif()
                if(NOT _QEMU_${_arch})
                    mcc_skip_test(${_tn} "${_qbin} not found (install the qemu-user package)" LABELS qemu)
                    continue()
                endif()

                set(_root "${MCC_QEMU_DLDIR}/${_arch}-${_libc}")
                add_test(NAME ${_tn}-fetch COMMAND "${CMAKE_COMMAND}"
                    "-DPTRURL=${MCC_QEMU_MIRROR}/${_gdir}/autobuilds/${_ptr}"
                    "-DMIRRORBASE=${MCC_QEMU_MIRROR}/${_gdir}/autobuilds"
                    "-DDEST=${_root}" "-DMARKER=${_root}/.fetched"
                    -P "${_qfetch}")
                set_tests_properties(${_tn}-fetch PROPERTIES
                    FIXTURES_SETUP FX_${_tn} LABELS qemu TIMEOUT 2000)

                add_test(NAME ${_tn} COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=${_qcc}" "-DMCCBASE=${CMAKE_BINARY_DIR}"
                    "-DSYSROOT=${_root}" "-DQEMU=${_QEMU_${_arch}}"
                    "-DSRCDIR=${_qsrc}"
                    "-DWORKDIR=${CMAKE_BINARY_DIR}/qemu-work/${_arch}-${_libc}"
                    -P "${_qrun}")
                set_tests_properties(${_tn} PROPERTIES
                    FIXTURES_REQUIRED "MCC_BUILT;FX_${_tn}" LABELS qemu TIMEOUT 600)
            endforeach()
        endforeach()
        message(STATUS "mcc tests: qemu matrix enabled (ctest -L qemu) for "
                       "archs=[${MCC_QEMU_ARCHS}] libcs=[${MCC_QEMU_LIBCS}]")
    endif()

    message(STATUS "mcc tests: registered exec (golden) + preprocess + embed/abitest/diff "
                   "+ hello/vla + orphan compile checks (ctest)")
endif()

# ------------------------------------------------------------------ #
# qemu-docker: run the MCC_QEMU_TESTS cross matrix inside a Linux
# container. The matrix needs user-mode qemu (qemu-x86_64, qemu-aarch64,
# …), which only builds on a Linux host — so on macOS (and any non-Linux
# host) it can't run natively. This target builds tests/qemu/docker and
# runs the whole grid there:
#     cmake --build <dir> --target qemu-docker
# Sysroot downloads are cached in the `mcc-qemu-roots` Docker volume.
# Narrow the grid with -DMCC_QEMU_DOCKER_ARCHS=... / _LIBCS=... (the same
# values MCC_QEMU_ARCHS/_LIBCS accept); empty uses the image defaults.
# ------------------------------------------------------------------ #
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
                -v mcc-qemu-roots:/qemu-roots
                ${_qd_env} mcc-qemu
        USES_TERMINAL VERBATIM
        COMMENT "qemu-user cross matrix in Docker (image mcc-qemu)")
else()
    message(STATUS "mcc: docker not found — `qemu-docker` target unavailable "
                   "(see tests/qemu/docker/README.md)")
endif()

file(GLOB_RECURSE MCC_TAGFILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.S")
list(FILTER MCC_TAGFILES EXCLUDE REGEX "/(cmake-build-[^/]*|build-c|\\.git|\\.idea)/")
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
    "/\\.git/" "/cmake-build-[^/]*/" "/\\.idea/" "\\.o$" "\\.a$")
include(CPack)
