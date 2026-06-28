cmake_minimum_required(VERSION 3.22)

set(TCC_VERSION "0.9.28rc")
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" TCC_VERSION_NUMERIC "${TCC_VERSION}")

project(tcc
    VERSION "${TCC_VERSION_NUMERIC}"
    LANGUAGES C ASM
    DESCRIPTION "Tiny C Compiler"
    HOMEPAGE_URL "https://repo.or.cz/tinycc.git")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()

set(TCC_CONFIG_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/config-extra.cmake"
    CACHE FILEPATH "Optional extra CMake config, included before options")
if(EXISTS "${TCC_CONFIG_EXTRA}")
    include("${TCC_CONFIG_EXTRA}")
    message(STATUS "tcc: included ${TCC_CONFIG_EXTRA}")
endif()

function(tcc_config_node name)
    cmake_parse_arguments(PARSE_ARGV 1 N "EMPTY_OK;ADVANCED"
        "TYPE;DEFAULT;HELP;GROUP;VISIBLE_WHEN" "CHOICES")

    set(_def "${N_DEFAULT}")
    if(DEFINED TCC_SEED_${name})
        set(_def "${TCC_SEED_${name}}")
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

    set_property(GLOBAL APPEND PROPERTY TCC_NODES "${name}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_DEFAULT "${N_DEFAULT}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_TYPE    "${N_TYPE}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_GROUP   "${N_GROUP}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_HELP    "${N_HELP}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_CHOICES "${N_CHOICES}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_EMPTYOK "${N_EMPTY_OK}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_WHEN    "${N_VISIBLE_WHEN}")
endfunction()

function(tcc_apply_node_visibility)
    get_property(_nodes GLOBAL PROPERTY TCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_when GLOBAL PROPERTY TCC_NODE_${_n}_WHEN)
        get_property(_type GLOBAL PROPERTY TCC_NODE_${_n}_TYPE)
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

function(tcc_status_node name value)
    set_property(GLOBAL APPEND PROPERTY TCC_STATUS_NODES "${name}")
    set_property(GLOBAL PROPERTY TCC_STATUS_${name} "${value}")
endfunction()

function(tcc_snapshot_user_presets)
    if(DEFINED CACHE{_TCC_PRESET_VARS})
        return()
    endif()
    get_cmake_property(_cv CACHE_VARIABLES)
    set(_preset "")
    foreach(_v IN LISTS _cv)
        if(_v MATCHES "^TCC_" AND NOT _v MATCHES "^_TCC_")
            list(APPEND _preset "${_v}")
        endif()
    endforeach()
    set(_TCC_PRESET_VARS "${_preset}" CACHE INTERNAL
        "TCC_* cache vars set by the user (-D / config-extra), for superbuild forwarding")
endfunction()

macro(tcc_profile_seed var val)
    set(TCC_SEED_${var} "${val}")
endmacro()

function(tcc_map_toolchain_cc tc outvar)
    if(DEFINED TCC_CC_${tc})
        set(${outvar} "${TCC_CC_${tc}}" PARENT_SCOPE)
        return()
    endif()
    if(tc STREQUAL "auto")
        set(${outvar} "" PARENT_SCOPE)
        return()
    endif()
    set(_names_auto "")
    set(_names_gcc gcc cc)
    set(_names_clang clang)
    set(_names_tcc tcc)
    set(_names_msvc cl)
    find_program(TCC_TOOLCHAIN_CC_${tc} NAMES ${_names_${tc}})
    if(TCC_TOOLCHAIN_CC_${tc})
        set(${outvar} "${TCC_TOOLCHAIN_CC_${tc}}" PARENT_SCOPE)
    else()
        set(${outvar} "" PARENT_SCOPE)
    endif()
endfunction()

function(tcc_map_target_args name outvar)
    if(DEFINED TCC_TARGET_ARGS_${name})
        set(${outvar} "${TCC_TARGET_ARGS_${name}}" PARENT_SCOPE)
    elseif(name STREQUAL "native")
        set(${outvar} "" PARENT_SCOPE)
    elseif(name STREQUAL "cross")
        set(${outvar} "-DTCC_ENABLE_CROSS=ON" PARENT_SCOPE)
    else()
        set(${outvar} "__UNKNOWN__" PARENT_SCOPE)
    endif()
endfunction()


function(tcc_autocorrect name value reason)
    get_property(_t GLOBAL PROPERTY TCC_NODE_${name}_TYPE)
    get_property(_h GLOBAL PROPERTY TCC_NODE_${name}_HELP)
    if(_t STREQUAL "BOOL")
        set(${name} "${value}" CACHE BOOL "${_h}" FORCE)
    else()
        set(${name} "${value}" CACHE "${_t}" "${_h}" FORCE)
    endif()
    message(STATUS "tcc config [autocorrect]: ${name} -> ${value} (${reason})")
endfunction()

function(tcc_validate_config)
    get_property(_nodes GLOBAL PROPERTY TCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_choices GLOBAL PROPERTY TCC_NODE_${_n}_CHOICES)
        get_property(_emptyok GLOBAL PROPERTY TCC_NODE_${_n}_EMPTYOK)
        if(_choices)
            set(_allowed ${_choices})
            if(_emptyok)
                list(APPEND _allowed "")
            endif()
            if(NOT "${${_n}}" IN_LIST _allowed)
                message(WARNING
                    "tcc config: ${_n}='${${_n}}' is not one of the known values "
                    "(${_allowed}); proceeding, but check for a typo.")
            endif()
        endif()
    endforeach()

    if(NOT "${TCC_CONFIG_SEMLOCK}" STREQUAL "" AND
       NOT "${TCC_CONFIG_SEMLOCK}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "tcc config: TCC_CONFIG_SEMLOCK='${TCC_CONFIG_SEMLOCK}' must be a number")
    endif()

    if((TCC_BUILD_SANITIZE OR TCC_BUILD_PROFILE)
       AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "tcc config: TCC_BUILD_SANITIZE/PROFILE need a GCC/Clang host compiler "
            "(have '${CMAKE_C_COMPILER_ID}'); disable them or set -DCMAKE_C_COMPILER "
            "to gcc/clang.")
    endif()
    if(TCC_BUILD_PROFILE AND TCC_TARGETOS STREQUAL "Darwin")
        message(FATAL_ERROR
            "tcc config: TCC_BUILD_PROFILE (tcc_p, -static) is unsupported on Darwin "
            "(no static crt0.o).")
    endif()
    if(CMAKE_CROSSCOMPILING AND "${TCC_EMULATOR}" STREQUAL "")
        if(TCC_CONFIG_AUTOCORRECT)
            if(NOT TCC_LIBTCC1_USEGCC)
                tcc_autocorrect(TCC_LIBTCC1_USEGCC ON
                    "no emulator: build libtcc1 with the host CC, not the foreign tcc")
            endif()
            if(TCC_BUILD_TESTS)
                tcc_autocorrect(TCC_BUILD_TESTS OFF
                    "no emulator: the test suite cannot run the foreign tcc / its exes")
            endif()
            if(TCC_BUILD_COVERAGE)
                tcc_autocorrect(TCC_BUILD_COVERAGE OFF
                    "no emulator: tcc_c is compiled by running the foreign tcc")
            endif()
            if(TCC_TARGETOS STREQUAL "WIN32" AND NOT TCC_BUILD_STATIC)
                message(FATAL_ERROR
                    "tcc config: even with TCC_CONFIG_AUTOCORRECT, a WIN32 *shared* "
                    "libtcc needs `tcc -impdef` to run on this host. Add "
                    "-DCMAKE_CROSSCOMPILING_EMULATOR=<run>, or build static with "
                    "-DTCC_BUILD_STATIC=ON.")
            endif()
        else()
            message(FATAL_ERROR
                "tcc config: CMAKE_CROSSCOMPILING is set, so the just-built `tcc` is a "
                "foreign binary, but it must run on this host to build libtcc1.a (and the "
                "tests/impdef/coverage steps). Set -DCMAKE_CROSSCOMPILING_EMULATOR=<run> "
                "(e.g. wine for a WIN32 target, qemu-arm for arm) so it can be executed, "
                "build host-runnable cross compilers instead with -DTCC_ENABLE_CROSS=ON "
                "(no toolchain file), or enable non-strict -DTCC_CONFIG_AUTOCORRECT=ON to "
                "build the runtime with the host CC and skip the run-tcc steps.")
        endif()
    endif()

    if(TCC_CONFIG_BCHECK AND NOT TCC_CONFIG_BACKTRACE)
        if(TCC_CONFIG_AUTOCORRECT)
            tcc_autocorrect(TCC_CONFIG_BACKTRACE ON
                "bcheck (-b) requires the backtrace runtime to link")
        else()
            message(WARNING
                "tcc config: TCC_CONFIG_BCHECK=ON with TCC_CONFIG_BACKTRACE=OFF -- the "
                "bcheck runtime is gated behind backtrace, so `tcc -b` will fail to link. "
                "Enable backtrace or disable bcheck (or set -DTCC_CONFIG_AUTOCORRECT=ON).")
        endif()
    endif()
    if(TCC_DISABLE_RPATH AND TCC_BUILD_STATIC)
        message(WARNING
            "tcc config: TCC_DISABLE_RPATH has no effect with a static libtcc "
            "(TCC_BUILD_STATIC=ON); rpath is only baked for a shared libtcc.")
    endif()
    if(NOT TCC_CONFIG_ASM AND NOT TCC_LIBTCC1_USEGCC)
        tcc_autocorrect(TCC_LIBTCC1_USEGCC ON
            "asm disabled: libtcc1 .S sources must be built with the host CC")
    endif()
    if(NOT TCC_TOOLCHAIN_PROFILE STREQUAL "auto"
       AND NOT TCC_TOOLCHAIN_PROFILE STREQUAL "${TCC_CC_NAME}")
        message(WARNING
            "tcc config: TCC_TOOLCHAIN_PROFILE='${TCC_TOOLCHAIN_PROFILE}' but the "
            "detected host compiler is '${TCC_CC_NAME}'. The profile seeds defaults; "
            "it does NOT switch compilers (use -DCMAKE_C_COMPILER for that).")
    endif()
    if(NOT TCC_CONFIG_PREDEFS)
        message(WARNING
            "tcc config: TCC_CONFIG_PREDEFS=OFF makes tcc load <tccdefs.h> from the "
            "tccdir at runtime instead of compiling it in -- a runtime dependency.")
    endif()
    if(TCC_CONFIG_LIBC STREQUAL "uClibc")
        message(WARNING
            "tcc config: TCC_CONFIG_LIBC=uClibc is a legacy selector (emits "
            "CONFIG_TCC_UCLIBC, no path effect on modern hosts).")
    endif()
    if((TCC_TARGETOS STREQUAL "WIN32" OR TCC_TARGETOS STREQUAL "Darwin") AND
       (TCC_CONFIG_NEW_DTAGS OR TCC_CONFIG_PIE OR TCC_CONFIG_PIC
        OR NOT TCC_CONFIG_LIBC STREQUAL ""))
        message(WARNING
            "tcc config: NEW_DTAGS/PIE/PIC/LIBC are ELF-only and inert on the "
            "${TCC_TARGETOS} target.")
    endif()
    if(NOT TCC_TARGETOS STREQUAL "Darwin")
        if(NOT TCC_CONFIG_CODESIGN STREQUAL "" OR NOT TCC_CONFIG_NEW_MACHO STREQUAL "")
            message(WARNING
                "tcc config: CODESIGN/NEW_MACHO only affect Mach-O output; inert on the "
                "${TCC_TARGETOS} target.")
        endif()
    endif()
endfunction()

function(tcc_report_config)
    message(STATUS "================ tcc configuration ================")
    get_property(_st GLOBAL PROPERTY TCC_STATUS_NODES)
    foreach(_n IN LISTS _st)
        get_property(_v GLOBAL PROPERTY TCC_STATUS_${_n})
        message(STATUS "  ${_n} = ${_v}   [derived]")
    endforeach()
    get_property(_nodes GLOBAL PROPERTY TCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_type GLOBAL PROPERTY TCC_NODE_${_n}_TYPE)
        get_property(_realtype CACHE ${_n} PROPERTY TYPE)
        if(_realtype STREQUAL "INTERNAL")
            continue()
        endif()
        message(STATUS "  ${_n} = ${${_n}}")
    endforeach()
    message(STATUS "===================================================")
endfunction()

function(tcc_generate_node_doc outfile)
    set(_md "<!-- Rendered from CMakeLists.txt section 1z by tcc_generate_node_doc(). -->\n")
    string(APPEND _md "# tcc configurable nodes\n\n")
    string(APPEND _md "The user-settable build knobs, grouped, with their relevance gate.\n")
    string(APPEND _md "Source of truth: the tcc_config_node() declarations in CMakeLists.txt\n")
    string(APPEND _md "section 1z; preprocessor-flag catalog in section 3a.\n\n")
    string(APPEND _md "| Node | Group | Type | Default | Choices | Shown when | Description |\n")
    string(APPEND _md "|---|---|---|---|---|---|---|\n")
    get_property(_nodes GLOBAL PROPERTY TCC_NODES)
    foreach(_n IN LISTS _nodes)
        get_property(_g GLOBAL PROPERTY TCC_NODE_${_n}_GROUP)
        get_property(_t GLOBAL PROPERTY TCC_NODE_${_n}_TYPE)
        get_property(_d GLOBAL PROPERTY TCC_NODE_${_n}_DEFAULT)
        get_property(_c GLOBAL PROPERTY TCC_NODE_${_n}_CHOICES)
        get_property(_e GLOBAL PROPERTY TCC_NODE_${_n}_EMPTYOK)
        get_property(_w GLOBAL PROPERTY TCC_NODE_${_n}_WHEN)
        get_property(_h GLOBAL PROPERTY TCC_NODE_${_n}_HELP)
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
    message(STATUS "tcc: generated ${outfile}")
endfunction()


set(_elf "NOT TCC_TARGETOS STREQUAL \"WIN32\" AND NOT TCC_TARGETOS STREQUAL \"Darwin\"")
set(_darwin "TCC_TARGETOS STREQUAL \"Darwin\"")

tcc_snapshot_user_presets()

tcc_config_node(TCC_TOOLCHAIN_PROFILE TYPE STRING DEFAULT auto GROUP "Build targets"
    HELP "Toolchain(s) to build with, in order (list, e.g. gcc;clang;tcc); single = profile+defaults"
    CHOICES auto gcc clang tcc msvc)

set(_tc_allowed auto gcc clang tcc msvc)
foreach(_tc IN LISTS TCC_TOOLCHAIN_PROFILE)
    if(NOT _tc IN_LIST _tc_allowed)
        message(FATAL_ERROR
            "tcc config: TCC_TOOLCHAIN_PROFILE entry '${_tc}' is not one of: ${_tc_allowed}")
    endif()
endforeach()

option(TCC_SUPERBUILD_TEST "Run ctest after each matrix sub-build" ON)
option(TCC_SUPERBUILD_SEQUENTIAL "Build toolchain stages in order (targets within a stage stay parallel)" ON)
set(TCC_TARGETS "native" CACHE STRING
    "Cross-platform target(s) to build, as a list (native;cross;<custom>)")
set_property(CACHE TCC_TARGETS PROPERTY STRINGS native cross)

list(LENGTH TCC_TOOLCHAIN_PROFILE _tc_count)
list(LENGTH TCC_TARGETS _tg_count)
set(_is_matrix FALSE)
if(_tc_count GREATER 1 OR _tg_count GREATER 1 OR NOT TCC_TARGETS STREQUAL "native")
    set(_is_matrix TRUE)
endif()
if(_is_matrix AND NOT TCC_SUPERBUILD_CHILD)
    include(ExternalProject)
    set(_order "parallel")
    if(TCC_SUPERBUILD_SEQUENTIAL)
        set(_order "toolchains in order, targets parallel")
    endif()
    message(STATUS "tcc: superbuild matrix toolchains=[${TCC_TOOLCHAIN_PROFILE}] x "
                   "targets=[${TCC_TARGETS}] (${_order})")
    set(_forward "")
    foreach(_v IN LISTS _TCC_PRESET_VARS)
        if(_v MATCHES "^TCC_(TOOLCHAIN_PROFILE|TARGETS|SUPERBUILD_TEST|SUPERBUILD_SEQUENTIAL)$")
            continue()
        endif()
        list(APPEND _forward "-D${_v}=${${_v}}")
    endforeach()
    set(_prev_stage "")
    foreach(_tc IN LISTS TCC_TOOLCHAIN_PROFILE)
        tcc_map_toolchain_cc("${_tc}" _cc)
        if(NOT _tc STREQUAL "auto" AND _cc STREQUAL "")
            message(FATAL_ERROR
                "tcc config: toolchain '${_tc}' requested but its compiler was not "
                "found; install it or pass -DTCC_CC_${_tc}=<path>.")
        endif()
        set(_this_stage "")
        foreach(_tg IN LISTS TCC_TARGETS)
            tcc_map_target_args("${_tg}" _targs)
            if(_targs STREQUAL "__UNKNOWN__")
                message(FATAL_ERROR
                    "tcc config: target '${_tg}' is unknown; use native/cross or "
                    "define -DTCC_TARGET_ARGS_${_tg}=\"-Dk=v;...\".")
            endif()
            set(_cell "${_tc}-${_tg}")
            set(_cargs -DTCC_TOOLCHAIN_PROFILE=${_tc} -DTCC_SUPERBUILD_CHILD=ON
                       ${_forward} ${_targs})
            if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
                list(APPEND _cargs "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
            endif()
            if(NOT _cc STREQUAL "")
                list(APPEND _cargs "-DCMAKE_C_COMPILER=${_cc}")
            endif()
            set(_dep "")
            if(TCC_SUPERBUILD_SEQUENTIAL AND _prev_stage)
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
            if(TCC_SUPERBUILD_TEST)
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

if(TCC_TOOLCHAIN_PROFILE STREQUAL "tcc")
    tcc_profile_seed(TCC_ONE_SOURCE ON)
elseif(TCC_TOOLCHAIN_PROFILE STREQUAL "msvc")
    tcc_profile_seed(TCC_ONE_SOURCE ON)
    tcc_profile_seed(TCC_BUILD_TESTS OFF)
endif()

tcc_config_node(TCC_ENABLE_CROSS  TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build all cross compilers (Makefile: make cross)")
tcc_config_node(TCC_BUILD_STATIC  TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build libtcc.a (ON) instead of shared libtcc (OFF)")
tcc_config_node(TCC_DISABLE_RPATH TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Do not bake -rpath into binaries linking libtcc.so"
    VISIBLE_WHEN "NOT TCC_BUILD_STATIC")
tcc_config_node(TCC_ONE_SOURCE    TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build libtcc from a single TU (Makefile: ONE_SOURCE)")
tcc_config_node(TCC_BUILD_TESTS   TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build/enable the test suite (tests/Makefile)")
tcc_config_node(TCC_DIAGNOSTICS TYPE BOOL DEFAULT OFF GROUP "Diagnostics" ADVANCED
    HELP "Everything-on diagnostics: verbose warnings + debug info + build tcc_s/tcc_p/tcc_c")
if(TCC_DIAGNOSTICS)
    tcc_profile_seed(TCC_BUILD_SANITIZE ON)
    tcc_profile_seed(TCC_BUILD_COVERAGE ON)
    tcc_profile_seed(TCC_BUILD_PROFILE  ON)
endif()

tcc_config_node(TCC_BUILD_PROFILE  TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build tcc_p (profiling: -pg -static)")
tcc_config_node(TCC_BUILD_COVERAGE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build tcc_c (coverage instrumentation)")
tcc_config_node(TCC_BUILD_SANITIZE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build tcc_s (-fsanitize=address,undefined)")
tcc_config_node(TCC_LIBTCC1_USEGCC TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build the native libtcc1 with the host CC instead of tcc (faster bcheck)")
tcc_config_node(TCC_CONFIG_AUTOCORRECT TYPE BOOL DEFAULT OFF GROUP "Build targets" ADVANCED
    HELP "Non-strict mode: auto-correct inert/non-runnable config combinations instead of only warning")

tcc_config_node(TCC_CONFIG_MINGW32  TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Build a WIN32/mingw32 target (input: forces TCC_TARGETOS=WIN32)")
tcc_config_node(TCC_CONFIG_BACKTRACE TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Enable stack backtraces (-bt / -run)")
tcc_config_node(TCC_CONFIG_BCHECK   TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Enable bounds checker (-b)"
    VISIBLE_WHEN "TCC_CONFIG_BACKTRACE")
tcc_config_node(TCC_CONFIG_ASM      TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Enable the integrated assembler (inline asm, global asm, .s files, asm labels)")
tcc_config_node(TCC_CONFIG_PREDEFS  TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Compile tccdefs.h into the binary (c2str)")
tcc_config_node(TCC_CONFIG_PIE      TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Let tcc generate position independent executables"
    VISIBLE_WHEN "${_elf}")
tcc_config_node(TCC_CONFIG_PIC      TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Position independent code"
    VISIBLE_WHEN "${_elf}")
tcc_config_node(TCC_WITH_SELINUX    TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Use mmap for executable memory (tcc -run)")
tcc_config_node(TCC_CONFIG_NEW_DTAGS TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Use DT_RUNPATH instead of DT_RPATH (tcc-emitted; always relevant on ELF)"
    VISIBLE_WHEN "${_elf}")

tcc_config_node(TCC_AUTO_TCCDIR     TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Build-tree tcc auto-discovers libtcc1.a + headers locally, falling back to system CONFIG_TCCDIR")

tcc_config_node(TCC_CONFIG_LIBC    TYPE STRING DEFAULT "" GROUP "tcc features"
    HELP "Target libc: '', uClibc, or musl" CHOICES uClibc musl EMPTY_OK
    VISIBLE_WHEN "${_elf}")
tcc_config_node(TCC_CONFIG_DWARF   TYPE STRING DEFAULT "" GROUP "tcc features"
    HELP "DWARF debug version (2..5); empty = stabs" CHOICES 0 2 3 4 5 EMPTY_OK)
tcc_config_node(TCC_CONFIG_SEMLOCK TYPE STRING DEFAULT "" GROUP "tcc features"
    HELP "CONFIG_TCC_SEMLOCK value (free-form numeric; empty = tcc.h default 1)")
tcc_config_node(TCC_CONFIG_NEW_MACHO TYPE STRING DEFAULT "" GROUP "Darwin"
    HELP "Force apple object format: yes|no|auto" CHOICES yes no auto EMPTY_OK
    VISIBLE_WHEN "${_darwin}")
tcc_config_node(TCC_CONFIG_CODESIGN TYPE STRING DEFAULT "" GROUP "Darwin"
    HELP "Use codesign on apple to sign executables: yes|no|auto" CHOICES yes no auto EMPTY_OK
    VISIBLE_WHEN "${_darwin}")


tcc_config_node(TCC_SYSROOT         TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --sysroot")
tcc_config_node(TCC_TRIPLET         TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --triplet")
tcc_config_node(TCC_SYSINCLUDEPATHS TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --sysincludepaths (colon sep)")
tcc_config_node(TCC_LIBPATHS        TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --libpaths (colon sep)")
tcc_config_node(TCC_CRTPREFIX       TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --crtprefix (colon sep)")
tcc_config_node(TCC_ELFINTERP       TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --elfinterp")
tcc_config_node(TCC_SWITCHES        TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --tcc-switches")
tcc_config_node(TCC_OS_RELEASE      TYPE STRING DEFAULT "" GROUP "Runtime paths" ADVANCED
    HELP "configure --os-release")
tcc_config_node(TCC_EXTRA_CFLAGS  TYPE STRING DEFAULT "" GROUP "Build flags" ADVANCED
    HELP "configure --extra-cflags (space separated)")
tcc_config_node(TCC_EXTRA_LDFLAGS TYPE STRING DEFAULT "" GROUP "Build flags" ADVANCED
    HELP "configure --extra-ldflags (space separated)")
tcc_config_node(TCC_EXTRA_LIBS    TYPE STRING DEFAULT "" GROUP "Build flags" ADVANCED
    HELP "configure --extra-libs (space separated)")
if(NOT "${TCC_EXTRA_CFLAGS}" STREQUAL "")
    separate_arguments(_tcc_extra_cflags NATIVE_COMMAND "${TCC_EXTRA_CFLAGS}")
    add_compile_options(${_tcc_extra_cflags})
endif()
if(NOT "${TCC_EXTRA_LDFLAGS}" STREQUAL "")
    separate_arguments(_tcc_extra_ldflags NATIVE_COMMAND "${TCC_EXTRA_LDFLAGS}")
    add_link_options(${_tcc_extra_ldflags})
endif()


set(_proc "${CMAKE_SYSTEM_PROCESSOR}")
set(_arm_cpuver "")
if(_proc MATCHES "^(x86|i[3-6]86|i86pc|BePC)$")
    set(TCC_CPU "i386")
elseif(_proc MATCHES "^(x86_64|amd64|x86-64|AMD64)$")
    set(TCC_CPU "x86_64")
elseif(_proc MATCHES "^(aarch64|arm64|ARM64)$")
    set(TCC_CPU "arm64")
elseif(_proc MATCHES "^arm")
    set(TCC_CPU "arm")
elseif(_proc MATCHES "^riscv64$")
    set(TCC_CPU "riscv64")
else()
    message(FATAL_ERROR "Unsupported CPU '${_proc}' (configure: Unsupported CPU)")
endif()

if(WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Windows|MINGW|MSYS|CYGWIN" OR TCC_CONFIG_MINGW32)
    set(TCC_TARGETOS "WIN32")
    set(TCC_LIBSUF ".lib")
    set(TCC_EXESUF ".exe")
    set(TCC_DLLSUF ".dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TCC_TARGETOS "Darwin")
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".dylib")
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    set(TCC_TARGETOS "${CMAKE_SYSTEM_NAME}")
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(TCC_TARGETOS "Android")
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
else()
    set(TCC_TARGETOS "${CMAKE_SYSTEM_NAME}")
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
endif()

if(TCC_TARGETOS STREQUAL "Darwin")
    if("${TCC_CONFIG_DWARF}" STREQUAL "")
        set(TCC_CONFIG_DWARF 4)
    endif()
    if("${TCC_CONFIG_CODESIGN}" STREQUAL "")
        set(TCC_CONFIG_CODESIGN yes)
    endif()
    if("${TCC_CONFIG_NEW_MACHO}" STREQUAL "" AND NOT CMAKE_CROSSCOMPILING)
        execute_process(COMMAND sw_vers -productVersion
            OUTPUT_VARIABLE _osxver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_osxver MATCHES "^([0-9]+)" AND CMAKE_MATCH_1 LESS 11)
            set(TCC_CONFIG_NEW_MACHO no)
        endif()
    endif()
elseif(TCC_TARGETOS MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    if("${TCC_ELFINTERP}" STREQUAL "")
        if(TCC_TARGETOS STREQUAL "FreeBSD")
            set(TCC_ELFINTERP "/libexec/ld-elf.so.1")
        elseif(TCC_TARGETOS STREQUAL "DragonFly")
            set(TCC_ELFINTERP "/usr/libexec/ld-elf.so.2")
        elseif(TCC_TARGETOS STREQUAL "NetBSD")
            set(TCC_ELFINTERP "/usr/libexec/ld.elf_so")
        elseif(TCC_TARGETOS STREQUAL "OpenBSD")
            set(TCC_ELFINTERP "/usr/libexec/ld.so")
        endif()
    endif()
elseif(TCC_TARGETOS STREQUAL "Android")
    if(DEFINED ENV{TERMUX_VERSION})
        set(_android_sysroot "/data/data/com.termux/files/usr")
    else()
        set(_android_sysroot "/usr")
    endif()
    if("${TCC_SYSROOT}" STREQUAL "")
        set(TCC_SYSROOT "${_android_sysroot}")
    endif()
    set(TCC_CONFIG_NEW_DTAGS ON)
    set(TCC_DISABLE_RPATH ON)
    if("${TCC_CONFIG_DWARF}" STREQUAL "")
        set(TCC_CONFIG_DWARF 4)
    endif()
    if(NOT TCC_CPU STREQUAL "i386")
        set(TCC_CONFIG_PIE ON)
    endif()
    set(TCC_BUILD_STATIC OFF)
    if("${TCC_TRIPLET}" STREQUAL "")
        if(TCC_CPU STREQUAL "arm")
            set(TCC_TRIPLET "arm-linux-androideabi")
            set(_arm_cpuver 7)
        elseif(TCC_CPU STREQUAL "arm64")
            set(TCC_TRIPLET "aarch64-linux-android")
        elseif(TCC_CPU STREQUAL "x86_64")
            set(TCC_TRIPLET "x86_64-linux-android")
        elseif(TCC_CPU STREQUAL "i386")
            set(TCC_TRIPLET "i686-linux-android")
        endif()
    endif()
    if(TCC_CPU MATCHES "64$")
        set(_android_s "64")
    else()
        set(_android_s "")
    endif()
    if("${TCC_SYSINCLUDEPATHS}" STREQUAL "")
        set(TCC_SYSINCLUDEPATHS "{B}/include:{R}/include:{R}/include/${TCC_TRIPLET}")
    endif()
    if("${TCC_LIBPATHS}" STREQUAL "")
        set(TCC_LIBPATHS "{B}:{R}/lib:/system/lib${_android_s}")
    endif()
    if("${TCC_CRTPREFIX}" STREQUAL "")
        set(TCC_CRTPREFIX "{R}/lib")
    endif()
    if("${TCC_ELFINTERP}" STREQUAL "")
        set(TCC_ELFINTERP "/system/bin/linker${_android_s}")
    endif()
    if("${TCC_SWITCHES}" STREQUAL "")
        set(TCC_SWITCHES "-Wl,-rpath=${TCC_SYSROOT}/lib")
    endif()
endif()

if("${TCC_OS_RELEASE}" STREQUAL "" AND NOT CMAKE_CROSSCOMPILING)
    set(TCC_OS_RELEASE "${CMAKE_HOST_SYSTEM_VERSION}")
endif()

if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    set(TCC_BIGENDIAN ON)
else()
    set(TCC_BIGENDIAN OFF)
endif()


message(STATUS "tcc target: CPU=${TCC_CPU} OS=${TCC_TARGETOS} bigendian=${TCC_BIGENDIAN}")


set(_tcc_cpu_define_i386    TCC_TARGET_I386)
set(_tcc_cpu_define_x86_64  TCC_TARGET_X86_64)
set(_tcc_cpu_define_arm     TCC_TARGET_ARM)
set(_tcc_cpu_define_arm64   TCC_TARGET_ARM64)
set(_tcc_cpu_define_riscv64 TCC_TARGET_RISCV64)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(TCC_CC_NAME clang)
elseif(CMAKE_C_COMPILER_ID STREQUAL "TinyCC")
    set(TCC_CC_NAME tcc)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(TCC_CC_NAME msvc)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(TCC_CC_NAME gcc)
else()
    set(TCC_CC_NAME unknown)
endif()
if("${CMAKE_C_COMPILER_VERSION}" MATCHES "^([0-9]+)\\.([0-9]+)")
    set(TCC_GCC_MAJOR "${CMAKE_MATCH_1}")
    set(TCC_GCC_MINOR "${CMAKE_MATCH_2}")
else()
    set(TCC_GCC_MAJOR 0)
    set(TCC_GCC_MINOR 0)
endif()

set(_arm_eabi OFF)
set(_arm_vfp OFF)
set(_arm_hardfloat OFF)
set(_arm_idiv OFF)
if(TCC_CPU STREQUAL "arm" AND NOT _arm_cpuver)
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

tcc_profile_seed(TCC_ARM_EABI      "${_arm_eabi}")
tcc_profile_seed(TCC_ARM_VFP       "${_arm_vfp}")
tcc_profile_seed(TCC_ARM_HARDFLOAT "${_arm_hardfloat}")
tcc_profile_seed(TCC_ARM_IDIV      "${_arm_idiv}")
tcc_profile_seed(TCC_CPUVER        "${_arm_cpuver}")
tcc_config_node(TCC_ARM_EABI TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "TCC_CPU STREQUAL arm"
    HELP "ARM EABI calling convention (TCC_ARM_EABI); autodetected from the host cc")
tcc_config_node(TCC_ARM_VFP TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "TCC_CPU STREQUAL arm"
    HELP "ARM VFP floating-point unit (TCC_ARM_VFP); autodetected from the host cc")
tcc_config_node(TCC_ARM_HARDFLOAT TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "TCC_CPU STREQUAL arm"
    HELP "ARM hard-float PCS (TCC_ARM_HARDFLOAT); autodetected from the host cc")
tcc_config_node(TCC_ARM_IDIV TYPE BOOL DEFAULT OFF GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "TCC_CPU STREQUAL arm"
    HELP "ARM hardware integer divide (__ARM_FEATURE_IDIV); autodetected from the host cc")
tcc_config_node(TCC_CPUVER TYPE STRING DEFAULT "" EMPTY_OK GROUP "ARM ABI" ADVANCED
    VISIBLE_WHEN "TCC_CPU STREQUAL arm"
    HELP "ARM architecture version 4/5/6/7 (CONFIG_TCC_CPUVER); autodetected (__ARM_ARCH)")

if(NOT TCC_TARGETOS STREQUAL "WIN32" AND NOT CMAKE_CROSSCOMPILING)
    if("${TCC_TRIPLET}" STREQUAL "")
        execute_process(COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
            OUTPUT_VARIABLE _dumpmachine OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_dumpmachine AND EXISTS "/usr/lib/${_dumpmachine}/crti.o")
            set(TCC_TRIPLET "${_dumpmachine}")
        endif()
    endif()
    if("${TCC_TRIPLET}" STREQUAL "" AND "${TCC_LIBPATHS}" STREQUAL ""
       AND TCC_CPU MATCHES "^(x86_64|arm64|riscv64)$"
       AND EXISTS "/usr/lib64/crti.o")
        set(TCC_LIBPATHS "{B}:/usr/lib64")
        if("${TCC_CRTPREFIX}" STREQUAL "")
            set(TCC_CRTPREFIX "/usr/lib64")
        endif()
    endif()
    message(STATUS "tcc paths: triplet='${TCC_TRIPLET}' libpaths='${TCC_LIBPATHS}' crtprefix='${TCC_CRTPREFIX}'")
    if("${TCC_CONFIG_LIBC}" STREQUAL "")
        if(EXISTS "/lib/ld-uClibc.so.0")
            message(STATUS "Perhaps you want -DTCC_CONFIG_LIBC=uClibc")
        endif()
        if(EXISTS "/lib/ld-musl-${TCC_CPU}.so.1")
            message(STATUS "Perhaps you want -DTCC_CONFIG_LIBC=musl")
        endif()
    endif()
endif()

include(CheckCCompilerFlag)
if(NOT TCC_CC_NAME STREQUAL "tcc" AND NOT MSVC)
    if(NOT TCC_CC_NAME STREQUAL "clang")
        check_c_compiler_flag("-Wno-unused-result" TCC_HAS_WNO_UNUSED_RESULT)
        if(TCC_HAS_WNO_UNUSED_RESULT)
            add_compile_options(-Wno-unused-result)
        endif()
    endif()
endif()
if(TCC_CC_NAME STREQUAL "gcc" AND TCC_GCC_MAJOR EQUAL 4
   AND NOT TCC_GCC_MINOR MATCHES "^[5-9]$")
    add_compile_options(-D_FORTIFY_SOURCE=0)
endif()

set(TCC_DIAG_FLAGS "")
if(TCC_DIAGNOSTICS)
    foreach(_f -Wall -Wextra -Wnull-dereference
               -Wno-sign-compare -Wno-unused-parameter -Wno-type-limits
               -Wno-unterminated-string-initialization
               -g3 -fno-omit-frame-pointer)
        string(MAKE_C_IDENTIFIER "TCC_HAS_DIAGFLAG_${_f}" _v)
        check_c_compiler_flag("${_f}" ${_v})
        if(${_v})
            list(APPEND TCC_DIAG_FLAGS "${_f}")
        endif()
    endforeach()
    message(STATUS "tcc diagnostics: core warning/debug flags =${TCC_DIAG_FLAGS}")
endif()

if(TCC_TARGETOS STREQUAL "Darwin" AND NOT TCC_CC_NAME STREQUAL "tcc")
    add_link_options(-flat_namespace)
    if(TCC_CC_NAME STREQUAL "clang" AND TCC_GCC_MAJOR LESS 15)
        add_link_options(-undefined warning)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6" CACHE STRING
            "Makefile: MACOSX_DEPLOYMENT_TARGET" FORCE)
    endif()
    if(TCC_CPU STREQUAL "x86_64" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        add_compile_options(-arch x86_64)
        add_link_options(-arch x86_64)
    endif()
endif()

if(TCC_TARGETOS STREQUAL "WIN32")
    if(TCC_CC_NAME STREQUAL "gcc")
        add_link_options(-static)
    endif()
    if(NOT TCC_CC_NAME STREQUAL "clang"
       AND NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        add_link_options(-s)
    endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install"
        CACHE PATH "Install prefix (default: project-local <build>/install)" FORCE)
    message(STATUS "tcc install prefix: defaulted to local '${CMAKE_INSTALL_PREFIX}' "
                   "(pass -DCMAKE_INSTALL_PREFIX=/usr/local for a system install)")
endif()

include(GNUInstallDirs)
tcc_config_node(TCC_INSTALL_TCCDIR TYPE STRING DEFAULT "" GROUP "Install" ADVANCED
    HELP "Runtime tccdir (CONFIG_TCCDIR); empty = <prefix>/<libdir>/tcc")

if(TCC_CONFIG_PREDEFS)
    set(_tcc_predefs_value 1)
else()
    set(_tcc_predefs_value 0)
endif()

tcc_status_node(TCC_CPU         "${TCC_CPU}")
tcc_status_node(TCC_TARGETOS    "${TCC_TARGETOS}")
tcc_status_node(host_compiler   "${TCC_CC_NAME} (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION})")
tcc_status_node(cross_compiling "${CMAKE_CROSSCOMPILING}")

set(TCC_EMULATOR "${CMAKE_CROSSCOMPILING_EMULATOR}")
if(CMAKE_CROSSCOMPILING)
    tcc_status_node(emulator "${TCC_EMULATOR}")
endif()

if(TCC_CPU STREQUAL "arm")
    tcc_status_node(arm_abi "eabi=${TCC_ARM_EABI} vfp=${TCC_ARM_VFP} hardfloat=${TCC_ARM_HARDFLOAT} idiv=${TCC_ARM_IDIV} cpuver=${TCC_CPUVER}")
endif()

tcc_apply_node_visibility()
tcc_validate_config()
tcc_report_config()
if(TCC_REGEN_DOCS)
    tcc_generate_node_doc("${CMAKE_BINARY_DIR}/config-nodes.md")
endif()


macro(tcc_def_str _var _name _val)
    if(NOT "${_val}" STREQUAL "")
        string(APPEND ${_var} "#define ${_name} \"${_val}\"\n")
    endif()
endmacro()

set(_nat "#define ${_tcc_cpu_define_${TCC_CPU}} 1\n")

if(TCC_CPU STREQUAL "arm")
    if(TCC_CPUVER)
        string(APPEND _nat "#define CONFIG_TCC_CPUVER ${TCC_CPUVER}\n")
    endif()
    if(TCC_ARM_EABI)
        string(APPEND _nat "#define TCC_ARM_EABI 1\n")
    endif()
    if(TCC_ARM_VFP)
        string(APPEND _nat "#define TCC_ARM_VFP 1\n")
    endif()
    if(TCC_ARM_HARDFLOAT)
        string(APPEND _nat "#define TCC_ARM_HARDFLOAT 1\n")
    endif()
    if(TCC_ARM_IDIV)
        string(APPEND _nat "#define __ARM_FEATURE_IDIV 1\n")
    endif()
endif()

if(TCC_TARGETOS STREQUAL "WIN32")
    string(APPEND _nat "#define TCC_TARGET_PE 1\n")
elseif(TCC_TARGETOS STREQUAL "Darwin")
    string(APPEND _nat "#define TCC_TARGET_MACHO 1\n")
elseif(TCC_TARGETOS STREQUAL "Android")
    string(APPEND _nat "#define TARGETOS_ANDROID 1\n")
elseif(TCC_TARGETOS MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    string(APPEND _nat "#define TARGETOS_${TCC_TARGETOS} 1\n")
endif()

if(TCC_CONFIG_LIBC STREQUAL "musl")
    string(APPEND _nat "#define CONFIG_TCC_MUSL 1\n")
elseif(TCC_CONFIG_LIBC STREQUAL "uClibc")
    string(APPEND _nat "#define CONFIG_TCC_UCLIBC 1\n")
endif()

if(TCC_WITH_SELINUX)
    string(APPEND _nat "#define CONFIG_SELINUX 1\n")
endif()
if(TCC_CONFIG_PIE)
    string(APPEND _nat "#define CONFIG_TCC_PIE 1\n")
endif()
if(TCC_CONFIG_PIC)
    string(APPEND _nat "#define CONFIG_TCC_PIC 1\n")
endif()
if(TCC_CONFIG_NEW_DTAGS)
    string(APPEND _nat "#define CONFIG_NEW_DTAGS 1\n")
endif()
if(TCC_CONFIG_CODESIGN)
    string(APPEND _nat "#define CONFIG_CODESIGN 1\n")
endif()

if(NOT TCC_CONFIG_BCHECK)
    string(APPEND _nat "#define CONFIG_TCC_BCHECK 0\n")
endif()
if(NOT TCC_CONFIG_ASM)
    string(APPEND _nat "#define CONFIG_TCC_ASM 0\n")
endif()
if(NOT TCC_CONFIG_BACKTRACE)
    string(APPEND _nat "#define CONFIG_TCC_BACKTRACE 0\n")
endif()
if(TCC_CONFIG_NEW_MACHO STREQUAL "no")
    string(APPEND _nat "#define CONFIG_NEW_MACHO 0\n")
endif()

if(NOT "${TCC_CONFIG_DWARF}" STREQUAL "")
    string(APPEND _nat "#define CONFIG_DWARF_VERSION ${TCC_CONFIG_DWARF}\n")
endif()
if(NOT "${TCC_CONFIG_SEMLOCK}" STREQUAL "")
    string(APPEND _nat "#define CONFIG_TCC_SEMLOCK ${TCC_CONFIG_SEMLOCK}\n")
endif()

tcc_def_str(_nat CONFIG_TCC_SYSINCLUDEPATHS "${TCC_SYSINCLUDEPATHS}")
tcc_def_str(_nat CONFIG_TCC_LIBPATHS        "${TCC_LIBPATHS}")
tcc_def_str(_nat CONFIG_TCC_CRTPREFIX       "${TCC_CRTPREFIX}")
tcc_def_str(_nat CONFIG_TCC_ELFINTERP       "${TCC_ELFINTERP}")
tcc_def_str(_nat CONFIG_TCC_SWITCHES        "${TCC_SWITCHES}")
tcc_def_str(_nat CONFIG_TRIPLET             "${TCC_TRIPLET}")
tcc_def_str(_nat CONFIG_OS_RELEASE          "${TCC_OS_RELEASE}")

set(_always "")
if(NOT "${TCC_SYSROOT}" STREQUAL "")
    string(APPEND _always
        "#ifndef CONFIG_SYSROOT\n# define CONFIG_SYSROOT \"${TCC_SYSROOT}\"\n#endif\n")
endif()
if(NOT TCC_TARGETOS STREQUAL "WIN32")
    if("${TCC_INSTALL_TCCDIR}" STREQUAL "")
        set(_tccdir "${CMAKE_INSTALL_FULL_LIBDIR}/tcc")
    else()
        set(_tccdir "${TCC_INSTALL_TCCDIR}")
    endif()
    string(APPEND _always
        "#ifndef CONFIG_TCCDIR\n# define CONFIG_TCCDIR \"${_tccdir}\"\n#endif\n")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h"
"/* config.h - automatically generated by CMake. Do NOT edit.
 *
 * Faithful port of the config.h produced by ./configure. Every macro below is
 * a *dynamic* preprocessor flag whose default lives in tcc.h; we only emit one
 * when it differs from that default. See the dynamic preprocessor flag catalog
 * in CMakeLists.txt (section 3a) for the full catalog and emission rules.
 */

#define TCC_VERSION \"${TCC_VERSION}\"

/* compiler identity (configure: conftest.c probes; consumed by tests) */
#define CC_NAME CC_${TCC_CC_NAME}
#define GCC_MAJOR ${TCC_GCC_MAJOR}
#define GCC_MINOR ${TCC_GCC_MINOR}

/* ---- native-target block ------------------------------------------------
 * configure suppresses this whole block for cross builds (which pass
 * -DTCC_TARGET_* on the command line, making the guard false). */
#if !(TCC_TARGET_I386 || TCC_TARGET_X86_64 || TCC_TARGET_ARM \\
   || TCC_TARGET_ARM64 || TCC_TARGET_RISCV64)
${_nat}#endif

/* ---- always-emitted defines (outside the native guard) ------------------ */
${_always}/* CONFIG_TCC_PREDEFS 1 -> tccdefs_.h compiled in (section 5);
 * 0 -> tcc loads <tccdefs.h> at runtime. */
#define CONFIG_TCC_PREDEFS ${_tcc_predefs_value}
")
message(STATUS "tcc config.h: CC_NAME=CC_${TCC_CC_NAME} predefs=${_tcc_predefs_value} "
               "bcheck=${TCC_CONFIG_BCHECK} backtrace=${TCC_CONFIG_BACKTRACE}")

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

if(TCC_AUTO_TCCDIR)
    add_compile_definitions(CONFIG_TCC_AUTO_TCCDIR=1)
endif()


find_package(Git QUIET)
set(TCC_GITHASH "")
if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_branch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_git_branch)
        execute_process(COMMAND "${GIT_EXECUTABLE}" log -1 --date=short
            "--pretty=format:%cd ${_git_branch}@%h"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE TCC_GITHASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND "${GIT_EXECUTABLE}" diff --quiet
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESULT_VARIABLE _git_dirty)
        if(_git_dirty)
            set(TCC_GITHASH "${TCC_GITHASH}*")
        endif()
    endif()
endif()
if(TCC_GITHASH)
    message(STATUS "tcc githash: ${TCC_GITHASH}")
endif()

if(TCC_CONFIG_PREDEFS)
    if(CMAKE_CROSSCOMPILING)
        find_program(TCC_HOSTCC
            NAMES "$ENV{CC_FOR_BUILD}" "$ENV{HOSTCC}" cc gcc clang
            DOC "Host C compiler used to build the c2str codegen tool")
        if(NOT TCC_HOSTCC)
            message(FATAL_ERROR
                "TCC_CONFIG_PREDEFS with CMAKE_CROSSCOMPILING needs a host C compiler "
                "to build c2str. Set -DTCC_HOSTCC=<cc> (or CC_FOR_BUILD), or pass "
                "-DTCC_CONFIG_PREDEFS=OFF to load <tccdefs.h> at runtime instead.")
        endif()
        set(_c2str_exe "${CMAKE_CURRENT_BINARY_DIR}/c2str_host")
        execute_process(
            COMMAND "${TCC_HOSTCC}"
                    -o "${_c2str_exe}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/c2str.c"
            RESULT_VARIABLE _c2str_rc)
        if(NOT _c2str_rc EQUAL 0)
            message(FATAL_ERROR "Host compiler '${TCC_HOSTCC}' failed to build c2str")
        endif()
        set(_c2str_dep "${_c2str_exe}")
    else()
        add_executable(c2str tools/c2str.c)
        set(_c2str_exe "$<TARGET_FILE:c2str>")
        set(_c2str_dep c2str)
    endif()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tccdefs_.h"
        COMMAND "${_c2str_exe}"
                "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/tccdefs.h"
                "${CMAKE_CURRENT_BINARY_DIR}/tccdefs_.h"
        DEPENDS ${_c2str_dep} "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/tccdefs.h"
        COMMENT "Generating tccdefs_.h (c2str include/tccdefs.h)"
        VERBATIM)
    add_custom_target(tcc_tccdefs
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/tccdefs_.h")
endif()


file(GLOB TCC_CORE_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/tccelf.c")
list(REMOVE_ITEM TCC_CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tcc.c")

file(GLOB TCC_BACKEND_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/${TCC_CPU}/*.c")
if(NOT TCC_BACKEND_SRC)
    message(FATAL_ERROR "No backend sources for CPU '${TCC_CPU}' (no src/arch/${TCC_CPU}/ directory)")
endif()
if(TCC_CPU STREQUAL "x86_64")
    list(APPEND TCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386/i386-asm.c")
endif()

if(TCC_TARGETOS STREQUAL "WIN32")
    list(APPEND TCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/tccpe.c")
elseif(TCC_TARGETOS STREQUAL "Darwin")
    list(APPEND TCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/tccmacho.c")
endif()

set(LIBTCC_ALL_SRC ${TCC_CORE_SRC} ${TCC_BACKEND_SRC})
list(REMOVE_ITEM LIBTCC_ALL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tcctools.c")

if(TCC_ONE_SOURCE)
    set(LIBTCC_SRC src/libtcc.c)
else()
    set(LIBTCC_SRC ${LIBTCC_ALL_SRC})
endif()

if(TCC_BUILD_STATIC)
    add_library(libtcc STATIC ${LIBTCC_SRC})
    if(TCC_CC_NAME STREQUAL "clang" AND TCC_CPU STREQUAL "x86_64")
        set_target_properties(libtcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
else()
    add_library(libtcc SHARED ${LIBTCC_SRC})
    target_compile_definitions(libtcc PRIVATE LIBTCC_AS_DLL)
    set_target_properties(libtcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(APPLE)
        set_target_properties(libtcc PROPERTIES
            MACOSX_RPATH ON VERSION "${TCC_VERSION_NUMERIC}")
    endif()
endif()
set_target_properties(libtcc PROPERTIES OUTPUT_NAME tcc PREFIX "lib")
if(NOT TCC_ONE_SOURCE)
    target_compile_definitions(libtcc PRIVATE ONE_SOURCE=0)
else()
    set(_one_src_deps "")
    foreach(_s IN LISTS LIBTCC_ALL_SRC)
        if(NOT _s STREQUAL "libtcc.c")
            list(APPEND _one_src_deps "${CMAKE_CURRENT_SOURCE_DIR}/${_s}")
        endif()
    endforeach()
    set_source_files_properties(src/libtcc.c PROPERTIES OBJECT_DEPENDS "${_one_src_deps}")
endif()
if(TARGET tcc_tccdefs)
    add_dependencies(libtcc tcc_tccdefs)
endif()
target_include_directories(libtcc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(NOT TCC_TARGETOS STREQUAL "WIN32")
    target_link_libraries(libtcc PUBLIC m)
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(libtcc PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    endif()
endif()
if(NOT "${TCC_EXTRA_LIBS}" STREQUAL "")
    separate_arguments(_tcc_extra_libs NATIVE_COMMAND "${TCC_EXTRA_LIBS}")
    target_link_libraries(libtcc PUBLIC ${_tcc_extra_libs})
endif()
if(TCC_DIAGNOSTICS)
    target_compile_options(libtcc PRIVATE ${TCC_DIAG_FLAGS})
endif()

add_executable(tcc src/tcc.c)
set_source_files_properties(src/tcc.c PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tcctools.c")
target_compile_definitions(tcc PRIVATE ONE_SOURCE=0)
target_link_libraries(tcc PRIVATE libtcc)
if(TCC_GITHASH)
    target_compile_definitions(tcc PRIVATE "TCC_GITHASH=\"${TCC_GITHASH}\"")
endif()
if(NOT TCC_BUILD_STATIC AND NOT TCC_DISABLE_RPATH)
    if(APPLE)
        set_target_properties(tcc PROPERTIES
            INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}")
    else()
        set_target_properties(tcc PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    endif()
endif()
if(TCC_DIAGNOSTICS)
    target_compile_options(tcc PRIVATE ${TCC_DIAG_FLAGS})
endif()


if(TCC_BUILD_PROFILE)
    add_executable(tcc_p src/tcc.c)
    set_source_files_properties(src/tcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/tcctools.c")
    target_include_directories(tcc_p PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(tcc_p PRIVATE -pg)
    target_compile_definitions(tcc_p PRIVATE CONFIG_TCC_STATIC TCC_PROFILE)
    target_link_options(tcc_p PRIVATE -pg -static)
    if(TCC_GITHASH)
        target_compile_definitions(tcc_p PRIVATE "TCC_GITHASH=\"${TCC_GITHASH}\"")
    endif()
    if(TARGET tcc_tccdefs)
        add_dependencies(tcc_p tcc_tccdefs)
    endif()
endif()

if(TCC_BUILD_SANITIZE)
    add_executable(tcc_s src/tcc.c)
    set_source_files_properties(src/tcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/tcctools.c")
    target_include_directories(tcc_s PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(tcc_s PRIVATE -fsanitize=address,undefined)
    target_link_options(tcc_s PRIVATE -fsanitize=address,undefined)
    if(TCC_GITHASH)
        target_compile_definitions(tcc_s PRIVATE "TCC_GITHASH=\"${TCC_GITHASH}\"")
    endif()
    if(TARGET tcc_tccdefs)
        add_dependencies(tcc_s tcc_tccdefs)
    endif()
endif()



set(TCC_LIBTCC1_EXTRA_O runmain bt-exe bt-dll bt-log bcheck)

function(_tcc_libtcc1_src obj outvar)
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

function(tcc_build_libtcc1 tname tcctgt cpu os native archive objdir out_extra)
    set(_common stdatomic atomic builtin alloca alloca-bt)
    if(cpu STREQUAL "i386" OR cpu STREQUAL "x86_64")
        set(_objs libtcc1 ${_common})
    elseif(cpu STREQUAL "arm")
        set(_objs libtcc1 armeabi armflush ${_common})
    elseif(cpu STREQUAL "arm64" OR cpu STREQUAL "riscv64")
        set(_objs lib-arm64 ${_common})
    else()
        message(FATAL_ERROR "tcc_build_libtcc1: unknown cpu '${cpu}'")
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
        if(TCC_CONFIG_BACKTRACE)
            list(APPEND _objs bt-exe bt-log)
            if(_win)
                list(APPEND _objs bt-dll)
            endif()
            if(TCC_CONFIG_BCHECK)
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
        set(_xflags -B${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32
                    -I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
                    -I${CMAKE_BINARY_DIR} ${_arch_incs})
    else()
        set(_xflags -B${CMAKE_CURRENT_SOURCE_DIR}/runtime
                    -I${CMAKE_BINARY_DIR} ${_arch_incs})
    endif()

    get_filename_component(_extra_dir "${archive}" DIRECTORY)
    file(MAKE_DIRECTORY "${objdir}")

    set(_use_gcc OFF)
    if(native AND TCC_LIBTCC1_USEGCC)
        set(_use_gcc ON)
        set(_gccflags -fPIC -fno-omit-frame-pointer -Wno-unused-function
                      -Wno-unused-variable
                      -I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include -I${CMAKE_BINARY_DIR}
                      ${_arch_incs})
        if(TCC_CONFIG_DWARF STREQUAL "")
            set(_bflags_gcc -gstabs)
        else()
            set(_bflags_gcc -gdwarf)
        endif()
    endif()

    set(_launch "")
    if(native)
        set(_launch ${TCC_EMULATOR})
    endif()

    set(_archived "")
    set(_extra "")
    foreach(obj IN LISTS _objs)
        _tcc_libtcc1_src("${obj}" _src)
        if(_use_gcc)
            set(_cc "${CMAKE_C_COMPILER}")
            set(_flags ${_gccflags})
            set(_ccdep "")
            set(_run "")
            if(obj STREQUAL "bcheck")
                list(APPEND _flags ${_bflags_gcc})
            endif()
        else()
            set(_cc "$<TARGET_FILE:${tcctgt}>")
            set(_flags ${_xflags})
            set(_ccdep ${tcctgt})
            set(_run ${_launch})
            if(obj STREQUAL "bcheck")
                list(APPEND _flags -bt)
            endif()
        endif()
        if(obj IN_LIST TCC_LIBTCC1_EXTRA_O)
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
            COMMAND ${_launch} "$<TARGET_FILE:${tcctgt}>" -ar rcs "${archive}" ${_archived}
            DEPENDS ${_archived} ${tcctgt}
            COMMENT "tcc -ar ${tname}"
            VERBATIM)
    endif()
    add_custom_target(${tname} ALL DEPENDS "${archive}" ${_extra})
    set(${out_extra} "${_extra}" PARENT_SCOPE)
endfunction()

tcc_build_libtcc1(libtcc1 tcc "${TCC_CPU}" "${TCC_TARGETOS}" ON
    "${CMAKE_BINARY_DIR}/libtcc1.a" "${CMAKE_BINARY_DIR}/lib"
    LIBTCC1_EXTRA_OBJS)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${CMAKE_BINARY_DIR}/include" FILES_MATCHING PATTERN "*.h")

if(TCC_BUILD_COVERAGE)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/tcc_c"
        COMMAND ${TCC_EMULATOR} "$<TARGET_FILE:tcc>" "${CMAKE_CURRENT_SOURCE_DIR}/src/tcc.c"
                -o "${CMAKE_BINARY_DIR}/tcc_c" -ftest-coverage
                "-I${CMAKE_BINARY_DIR}" "-I${CMAKE_CURRENT_SOURCE_DIR}"
                "-B${CMAKE_CURRENT_SOURCE_DIR}" "-B${CMAKE_BINARY_DIR}"
        DEPENDS tcc libtcc1 "${CMAKE_CURRENT_SOURCE_DIR}/src/tcc.c"
        COMMENT "tcc -ftest-coverage tcc.c -> tcc_c" VERBATIM)
    add_custom_target(tcc_cov ALL DEPENDS "${CMAKE_BINARY_DIR}/tcc_c")
endif()

if(TCC_ENABLE_CROSS)
    if("${TCC_INSTALL_TCCDIR}" STREQUAL "")
        set(_tccdir_install "${CMAKE_INSTALL_FULL_LIBDIR}/tcc")
    else()
        set(_tccdir_install "${TCC_INSTALL_TCCDIR}")
    endif()

    set(TCC_X i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64
              arm64-win32 arm-wince riscv64 arm64-osx)
    foreach(_t IN LISTS TCC_X)
        set(_cdefs "")
        set(_cos "Linux")
        if(_t STREQUAL "i386")
            set(_ccpu i386)
            set(_cdefs TCC_TARGET_I386)
        elseif(_t STREQUAL "x86_64")
            set(_ccpu x86_64)
            set(_cdefs TCC_TARGET_X86_64)
        elseif(_t STREQUAL "i386-win32")
            set(_ccpu i386)
            set(_cos WIN32)
            set(_cdefs TCC_TARGET_I386 TCC_TARGET_PE)
        elseif(_t STREQUAL "x86_64-win32")
            set(_ccpu x86_64)
            set(_cos WIN32)
            set(_cdefs TCC_TARGET_X86_64 TCC_TARGET_PE)
        elseif(_t STREQUAL "x86_64-osx")
            set(_ccpu x86_64)
            set(_cos Darwin)
            set(_cdefs TCC_TARGET_X86_64 TCC_TARGET_MACHO)
        elseif(_t STREQUAL "arm")
            set(_ccpu arm)
            set(_cdefs TCC_TARGET_ARM TCC_ARM_VFP TCC_ARM_EABI TCC_ARM_HARDFLOAT)
        elseif(_t STREQUAL "arm64")
            set(_ccpu arm64)
            set(_cdefs TCC_TARGET_ARM64)
        elseif(_t STREQUAL "arm64-win32")
            set(_ccpu arm64)
            set(_cos WIN32)
            set(_cdefs TCC_TARGET_ARM64 TCC_TARGET_PE)
        elseif(_t STREQUAL "arm-wince")
            set(_ccpu arm)
            set(_cos WIN32)
            set(_cdefs TCC_TARGET_ARM TCC_ARM_VFP TCC_ARM_EABI TCC_ARM_HARDFLOAT TCC_TARGET_PE)
        elseif(_t STREQUAL "riscv64")
            set(_ccpu riscv64)
            set(_cdefs TCC_TARGET_RISCV64)
        elseif(_t STREQUAL "arm64-osx")
            set(_ccpu arm64)
            set(_cos Darwin)
            set(_cdefs TCC_TARGET_ARM64 TCC_TARGET_MACHO)
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
                "CONFIG_TCC_CRTPREFIX=\"{R}/lib\""
                "CONFIG_TCC_LIBPATHS=\"{R}/lib:{B}\""
                "CONFIG_TCC_SYSINCLUDEPATHS=\"{B}/include:{R}/include\"")
        endif()

        add_executable(${_t}-tcc src/tcc.c)
        target_compile_definitions(${_t}-tcc PRIVATE ${_cdefs} ${_cpath_defs}
            "CONFIG_TCC_CROSSPREFIX=\"${_t}-\"")
        if(_cos STREQUAL "WIN32")
            target_compile_definitions(${_t}-tcc PRIVATE
                "CONFIG_TCCDIR=\"${_tccdir_install}/win32\"")
        endif()
        if(TCC_GITHASH)
            target_compile_definitions(${_t}-tcc PRIVATE "TCC_GITHASH=\"${TCC_GITHASH}\"")
        endif()
        if(TARGET tcc_tccdefs)
            add_dependencies(${_t}-tcc tcc_tccdefs)
        endif()
        install(TARGETS ${_t}-tcc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

        tcc_build_libtcc1(${_t}-libtcc1 ${_t}-tcc "${_ccpu}" "${_cos}" OFF
            "${CMAKE_BINARY_DIR}/${_t}-libtcc1.a"
            "${CMAKE_BINARY_DIR}/lib-${_t}" _cross_extra)
        install(FILES "${CMAKE_BINARY_DIR}/${_t}-libtcc1.a"
            DESTINATION "${_tccdir_install}")
    endforeach()
endif()

if(TCC_TARGETOS STREQUAL "WIN32" AND NOT TCC_BUILD_STATIC)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/libtcc.def"
        COMMAND ${TCC_EMULATOR} "$<TARGET_FILE:tcc>" -impdef "$<TARGET_FILE:libtcc>"
                -o "${CMAKE_BINARY_DIR}/libtcc.def"
        DEPENDS tcc libtcc
        COMMENT "tcc -impdef libtcc.dll -> libtcc.def" VERBATIM)
    add_custom_target(libtcc_def ALL DEPENDS "${CMAKE_BINARY_DIR}/libtcc.def")
endif()

include(GNUInstallDirs)
if("${TCC_INSTALL_TCCDIR}" STREQUAL "")
    set(_tccdir "${CMAKE_INSTALL_FULL_LIBDIR}/tcc")
else()
    set(_tccdir "${TCC_INSTALL_TCCDIR}")
endif()

install(TARGETS tcc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(TARGETS libtcc EXPORT tccTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/libtcc.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
set(_tcc_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/tcc")
install(EXPORT tccTargets
    NAMESPACE tcc::
    FILE tccTargets.cmake
    DESTINATION "${_tcc_cmakedir}")
file(WRITE "${CMAKE_BINARY_DIR}/tccConfig.cmake.in"
"@PACKAGE_INIT@\ninclude(CMakeFindDependencyMacro)\nfind_dependency(Threads)\ninclude(\"\${CMAKE_CURRENT_LIST_DIR}/tccTargets.cmake\")\ncheck_required_components(tcc)\n")
configure_package_config_file(
    "${CMAKE_BINARY_DIR}/tccConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/tccConfig.cmake"
    INSTALL_DESTINATION "${_tcc_cmakedir}")
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/tccConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_BINARY_DIR}/tccConfig.cmake"
    "${CMAKE_BINARY_DIR}/tccConfigVersion.cmake"
    DESTINATION "${_tcc_cmakedir}")

install(FILES "${CMAKE_BINARY_DIR}/libtcc1.a" ${LIBTCC1_EXTRA_OBJS}
    DESTINATION "${_tccdir}")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${_tccdir}/include" FILES_MATCHING PATTERN "*.h")

if(TCC_TARGETOS STREQUAL "WIN32")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/include/"
        DESTINATION "${_tccdir}/include")
    file(GLOB _win_defs "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib/*.def")
    install(FILES ${_win_defs} DESTINATION "${_tccdir}/lib")
    if(NOT TCC_BUILD_STATIC)
        install(FILES "${CMAKE_BINARY_DIR}/libtcc.def"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL)
    endif()
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/"
        DESTINATION examples FILES_MATCHING PATTERN "*.c")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/libtcc_test.c"
        DESTINATION examples)
    file(WRITE "${CMAKE_BINARY_DIR}/tcc-win32.txt" [==[

    TinyCC
    ======

    This file contains specific information for usage of TinyCC
    under MS-Windows.


    Installation from the binary ZIP package:
    -----------------------------------------
    Unzip the package to a directory of your choice.


    Set the system PATH:
    --------------------
    To be able to invoke the compiler from everywhere on your computer by
    just typing "tcc", please add the directory containing tcc.exe to your
    system PATH.


    Include and library search paths
    --------------------------------
    On windows,  the standard "include" and "lib" directories are searched
    relatively from the location of the executables (tcc.exe, libtcc.dll).


    Examples:
    ---------
    Open a console window (DOS box) and 'cd' to the examples directory.

    For the 'Fibonacci' example type:

	tcc fib.c

    For the 'Hello Windows' GUI example type:

	tcc hello_win.c

    For the 'Hello DLL' example type

	tcc -shared dll.c
	tcc -impdef dll.dll (optional)
        tcc hello_dll.c dll.def


    Using libtcc as JIT compiler in your program
    --------------------------------------------
    Check out the 'libtcc_test' example:

    - Running it from source:
        tcc -I libtcc libtcc/libtcc.def -run examples/libtcc_test.c

    - Compiling with TCC:
        tcc examples/libtcc_test.c -I libtcc libtcc/libtcc.def

    - Compiling with MinGW:
        gcc examples/libtcc_test.c -I libtcc libtcc.dll -o libtcc_test.exe

    - Compiling with MSVC:
        lib /def:libtcc\libtcc.def /out:libtcc.lib
        cl /MD examples/libtcc_test.c -I libtcc libtcc.lib


    Import Definition Files:
    ------------------------
    To link with Windows system DLLs, TCC uses import definition
    files (.def) instead of libraries.

    The now built-in 'tiny_impdef' program may be used to make
    additional .def files for any DLL. For example

        tcc -impdef [-v] opengl32.dll [-o opengl32.def]

    Put opengl32.def into the tcc/lib directory.  Specify -lopengl32 at
    the TCC commandline to link a program that uses opengl32.dll.


    Header Files:
    -------------
    The system header files (except _mingw.h) are from the MinGW
    distribution:

	http://www.mingw.org/

    From the windows headers, only a minimal set is included.  If you need
    more,  get MinGW's "w32api" package.  Extract the files from "include"
    into your "tcc/include/winapi" directory.

    If the latest mingw headers don't play nice with the TinyCC headers,
    there's also TinyCC-specific win32 headers package which contains
    an "include" dir. Files in that dir are additional to the built-in
    include files. At the time of writing there are few files in that
    package which also exist as built-in headers:

        unistd.h
        winapi/qos.h
        winapi/winnls.h
        winapi/ws2ipdef.h
        winapi/ws2tcpip.h
        winapi/windows.h
        winapi/winsock2.h

    With the exception of winapi/winsock2.h, these should overwrite the
    built-in headers (unistd.h and winapi/windows.h are enhanced in that
    package, winapi/winsock2.h is older than the built-in file - and with
    less definitions, and the rest are identical).

    The latest package at the time of writing is winapi-full-for-0.9.27.zip
    which works well also with TinyCC 0.9.28rc, and is available at:

        http://download.savannah.nongnu.org/releases/tinycc/


    Resource Files:
    ---------------
    TCC can link windows resources in coff format as generated by MinGW's
    windres.exe.  For example:

        windres -O coff app.rc -o appres.o
        tcc app.c appres.o -o app.exe


    Tiny Libmaker:
    --------------
    The now built-in tiny_libmaker tool by Timovj Lahde can be used as
    'ar' replacement to make a library from several object files:

	tcc -ar [rcsv] library objectfiles ...


    Compilation from source:
    ------------------------
    The build is CMake (this CMakeLists.txt) -- it replaces the old
    win32/build-tcc.bat and ./configure scripts and produces tcc.exe,
    libtcc.dll, libtcc1.a and the install layout (include, lib, examples,
    libtcc, doc).

    * MinGW / MSYS2 (GCC), from an MSYS2/MinGW shell:
        cmake -S . -B build
        cmake --build build
        cmake --install build --prefix <installpath>

    * MSVC, from a "VS Developer Command Prompt":
        cmake -S . -B build -G "NMake Makefiles"   (or "Ninja")
        cmake --build build
        cmake --install build --prefix <installpath>

    * Cross-build a Windows tcc from another host by pointing CMake at a
      mingw-w64 toolchain file (-DCMAKE_TOOLCHAIN_FILE=...), or force a PE
      target in place with -DTCC_CONFIG_MINGW32=ON.

    Limitations:
    ------------
    - On the object file level, currently TCC supports only the ELF format,
      not COFF as used by MinGW and MSVC.  It is not possible to exchange
      object files or libraries between TCC and these compilers.

      However libraries for TCC from objects by TCC can be made using
        tcc -ar lib.a files.o ,,,

    - No leading underscore is generated in the ELF symbols.

    Documentation and License:
    --------------------------
    TCC is distributed under the GNU Lesser General Public License. (See
    COPYING file or http://www.gnu.org/licenses/lgpl-2.1.html)

    TinyCC homepage is at:

	http://fabrice.bellard.free.fr/tcc/


    WinAPI Help and 3rd-party tools:
    --------------------------------
    The Windows API documentation (Win95) in a single .hlp file is
    available on the lcc-win32 site as "win32hlp.exe" or from other
    locations as "win32hlp_big.zip".

    A nice RAD tool to create windows resources (dialog boxes etc.) is
    "ResEd", available at the RadASM website.


    --- grischka
]==])
    install(FILES "${CMAKE_BINARY_DIR}/tcc-win32.txt" DESTINATION doc)
endif()

if(UNIX AND NOT CMAKE_CROSSCOMPILING)
    add_executable(tccbuild tools/build.c)
endif()

if(TCC_BUILD_TESTS)
    enable_testing()
    set(_tcc_exe "$<TARGET_FILE:tcc>")
    set(_bdir "${CMAKE_BINARY_DIR}")
    set(_idir "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")


    set(_exe_driver "${CMAKE_BINARY_DIR}/run_tcc_exe.cmake")
    file(WRITE "${_exe_driver}" "set(EMU \"${TCC_EMULATOR}\")\n")
    file(APPEND "${_exe_driver}" [==[
separate_arguments(_srcs UNIX_COMMAND "${SRCS}")
separate_arguments(_rargs UNIX_COMMAND "${RUNARGS}")
execute_process(
    COMMAND ${EMU} "${TCC}" "-B${BDIR}" "-I${IDIR}" ${_srcs} -o "${OUTFILE}"
    RESULT_VARIABLE _crc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _crc EQUAL 0)
    message(FATAL_ERROR "tcc compile failed for ${SRCS}:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${OUTFILE}" ${_rargs} RESULT_VARIABLE _rrc)
if(NOT _rrc EQUAL 0)
    message(FATAL_ERROR "${OUTFILE} exited with ${_rrc}")
endif()
]==])

    set(_tcctest_driver "${CMAKE_BINARY_DIR}/run_tcctest.cmake")
    file(WRITE "${_tcctest_driver}" "set(EMU \"${TCC_EMULATOR}\")\n")
    file(APPEND "${_tcctest_driver}" [==[
set(_ref_exe "${OUTDIR}/tcctest.gcc")
set(_ref_out "${OUTDIR}/tcctest.ref")
separate_arguments(_refflags UNIX_COMMAND "${REFFLAGS}")
execute_process(
    COMMAND "${CC}" "${SRC}" -o "${_ref_exe}" "-I${IDIR}" "-I${SRCDIR}" "-I${BDIR}"
            -w -O0 -std=gnu99 -fno-omit-frame-pointer ${_refflags}
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "cc failed to build tcctest.c:\n${_o}${_e}")
endif()
execute_process(COMMAND "${_ref_exe}" OUTPUT_FILE "${_ref_out}")
separate_arguments(_targs UNIX_COMMAND "${TCCARGS}")
set(_tcc_exe "${OUTDIR}/tcctest.tcc")
set(_tcc_out "${OUTDIR}/tcctest.out")
execute_process(
    COMMAND ${EMU} "${TCC}" "-B${BDIR}" "-I${IDIR}" "-I${SRCDIR}" "-I${BDIR}"
            ${_targs} "${SRC}" -o "${_tcc_exe}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "tcc failed to build tcctest.c:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${_tcc_exe}" OUTPUT_FILE "${_tcc_out}")
find_program(_diff diff)
execute_process(COMMAND "${_diff}" -u "${_ref_out}" "${_tcc_out}"
    RESULT_VARIABLE _drc OUTPUT_VARIABLE _d)
if(NOT _drc EQUAL 0)
    message(FATAL_ERROR "tcctest mismatch (cc vs tcc):\n${_d}")
endif()
]==])

    set(_asmconnect_driver "${CMAKE_BINARY_DIR}/run_asm_c_connect.cmake")
    file(WRITE "${_asmconnect_driver}" "set(EMU \"${TCC_EMULATOR}\")\n")
    file(APPEND "${_asmconnect_driver}" [==[
set(_1 "${SRCDIR}/asm-c-connect-1.c")
set(_2 "${SRCDIR}/asm-c-connect-2.c")
set(_flags "-B${BDIR}" "-I${IDIR}")
execute_process(COMMAND ${EMU} "${TCC}" ${_flags} "${_1}" "${_2}"
        -o "${OUTDIR}/asm-c-connect" RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "asm-c-connect single build failed:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${OUTDIR}/asm-c-connect"
    OUTPUT_FILE "${OUTDIR}/asm-c-connect.out1")
execute_process(COMMAND ${EMU} "${TCC}" ${_flags} -c "${_1}" -o "${OUTDIR}/acc1.o")
execute_process(COMMAND ${EMU} "${TCC}" ${_flags} -c "${_2}" -o "${OUTDIR}/acc2.o")
execute_process(COMMAND ${EMU} "${TCC}" ${_flags}
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


    add_test(NAME tcc_build
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}")
    set_tests_properties(tcc_build PROPERTIES FIXTURES_SETUP TCC_BUILT)

    add_executable(tests2_runner tests/tests2_runner.c)
    target_include_directories(tests2_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set(_t2r_skip "")
    if(NOT TCC_CONFIG_ASM)
        set(_t2r_skip 127_asm_goto 85_asm-outside-function 98_al_ax_extend)
    endif()
    add_test(NAME tests2-runner
        COMMAND tests2_runner "${_tcc_exe}" "${_bdir}" "${_idir}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_BINARY_DIR}" ${_t2r_skip})
    set_tests_properties(tests2-runner PROPERTIES FIXTURES_REQUIRED TCC_BUILT
        ENVIRONMENT "TCC_TEST_EMU=${TCC_EMULATOR}")

    add_executable(libtcc_test tests/libtcc_test.c)
    target_link_libraries(libtcc_test PRIVATE libtcc)
    target_include_directories(libtcc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME libtest
        COMMAND libtcc_test "-B${CMAKE_BINARY_DIR}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
    set_tests_properties(libtest PROPERTIES FIXTURES_REQUIRED TCC_BUILT)

    if(TCC_CONFIG_BACKTRACE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/libtcc_test_mt.c")
        add_executable(libtcc_test_mt tests/libtcc_test_mt.c)
        target_link_libraries(libtcc_test_mt PRIVATE libtcc)
        target_include_directories(libtcc_test_mt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(libtcc_test_mt PRIVATE Threads::Threads)
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        add_test(NAME hello-run
            COMMAND ${_tcc_exe} "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
                    -run "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        set_tests_properties(hello-run PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        add_test(NAME hello-exe COMMAND "${CMAKE_COMMAND}"
            -DTCC=${_tcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCS=${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c
            -DOUTFILE=${CMAKE_BINARY_DIR}/hello${TCC_EXESUF} -P "${_exe_driver}")
        set_tests_properties(hello-exe PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/vla_test.c")
        add_test(NAME vla_test-run COMMAND "${CMAKE_COMMAND}"
            -DTCC=${_tcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCS=${CMAKE_CURRENT_SOURCE_DIR}/tests/vla_test.c
            -DOUTFILE=${CMAKE_BINARY_DIR}/vla_test${TCC_EXESUF} -P "${_exe_driver}")
        set_tests_properties(vla_test-run PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/abitest.c")
        add_executable(abitest tests/abitest.c)
        target_link_libraries(abitest PRIVATE libtcc)
        target_include_directories(abitest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME abitest-cc
            COMMAND abitest "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(abitest-cc PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    set(_tcctest_refflags "")
    if(TCC_TARGETOS STREQUAL "WIN32")
        set(_msvcrt_start "${CMAKE_CURRENT_SOURCE_DIR}/tests/msvcrt_start.c")
        if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
            set(_tcctest_refflags "-nostdlib ${_msvcrt_start} -lkernel32 msvcrt.lib")
        else()
            set(_tcctest_refflags
                "-nostdlib ${_msvcrt_start} -lgcc -lkernel32 c:/windows/system32/msvcrt.dll")
        endif()
    elseif(TCC_CPU STREQUAL "arm")
        set(_tcctest_refflags "-marm")
    elseif(TCC_CPU STREQUAL "i386")
        set(_tcctest_refflags "-fno-PIC -fno-PIE -Wl,-z,notext")
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/tcctest.c" AND TCC_CONFIG_ASM)
        add_test(NAME tcctest COMMAND "${CMAKE_COMMAND}"
            -DCC=${CMAKE_C_COMPILER} -DTCC=${_tcc_exe}
            -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/tcctest.c
            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DOUTDIR=${CMAKE_BINARY_DIR} "-DREFFLAGS=${_tcctest_refflags}"
            -P "${_tcctest_driver}")
        set_tests_properties(tcctest PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        if(TCC_CONFIG_BCHECK)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bcheck")
            add_test(NAME tcctest-bcheck COMMAND "${CMAKE_COMMAND}"
                -DCC=${CMAKE_C_COMPILER} -DTCC=${_tcc_exe}
                -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/tcctest.c
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -DBDIR=${_bdir} -DIDIR=${_idir}
                -DOUTDIR=${CMAKE_BINARY_DIR}/bcheck "-DREFFLAGS=${_tcctest_refflags}"
                -DTCCARGS=-b -P "${_tcctest_driver}")
            set_tests_properties(tcctest-bcheck PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        endif()
    endif()

    if(TCC_CPU MATCHES "^(i386|x86_64)$" AND TCC_CONFIG_ASM
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm-c-connect-1.c")
        add_test(NAME asm-c-connect-test COMMAND "${CMAKE_COMMAND}"
            -DTCC=${_tcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests -DOUTDIR=${CMAKE_BINARY_DIR}
            -P "${_asmconnect_driver}")
        set_tests_properties(asm-c-connect-test PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    set(TCC_GCCTESTSUITE_PATH "" CACHE PATH
        "gcc.c-torture directory for the `gcctestsuite` target")
    set(TCC_ARM_CROSS_COMPILE "" CACHE STRING
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
if(NOT DEFINED ITCC OR NOT EXISTS "${ITCC}")
    message(STATUS "SKIP: i386 tcc not found at '${ITCC}'")
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
_cc("${ITCC}" "${_w}/e_t.o" -c "${_w}/callee.c")
_cc("${ITCC}" "${_w}/l_t.o" -c "${_w}/caller.c")
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
_check("tcc caller -> gcc callee" "${_w}/e_g.o" "${_w}/l_t.o")
_check("gcc caller -> tcc callee" "${_w}/e_t.o" "${_w}/l_g.o")
_check("tcc caller -> tcc callee" "${_w}/e_t.o" "${_w}/l_t.o")
file(WRITE "${_w}/unsup.c"
    "int __attribute__((fastcall)) f(double a,int b); int g(){ return f(1.0,2); }\n")
execute_process(COMMAND "${ITCC}" -c "${_w}/unsup.c" -o "${_w}/unsup.o"
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
    if(TARGET i386-tcc)
        add_test(NAME i386-fastcall-abi COMMAND "${CMAKE_COMMAND}"
            -DITCC=$<TARGET_FILE:i386-tcc> -P "${_fastcall_drv}")
        set_tests_properties(i386-fastcall-abi PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    set(_gccts_drv "${CMAKE_BINARY_DIR}/run_gcctestsuite.cmake")
    file(WRITE "${_gccts_drv}" "set(BUILDDIR \"${CMAKE_BINARY_DIR}\")\n")
    file(APPEND "${_gccts_drv}" [==[
if(NOT DEFINED TESTSUITE_PATH OR TESTSUITE_PATH STREQUAL "")
    set(TESTSUITE_PATH "$ENV{TESTSUITE_PATH}")
endif()
if(TESTSUITE_PATH STREQUAL "" OR NOT IS_DIRECTORY "${TESTSUITE_PATH}")
    message(STATUS "SKIP: gcc testsuite not found (set TCC_GCCTESTSUITE_PATH)")
    return()
endif()
if(NOT DEFINED TCC OR NOT EXISTS "${TCC}")
    message(FATAL_ERROR "gcctestsuite: -DTCC=<tcc binary> required")
endif()
if(NOT DEFINED BDIR OR BDIR STREQUAL "")
    get_filename_component(BDIR "${TCC}" DIRECTORY)
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
    execute_process(COMMAND "${TCC}" ${_flags} -o "${_rt}/tst.o" -c "${_s}"
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
    execute_process(COMMAND "${TCC}" ${_flags} "${_s}" -o "${_rt}/tst" -lm
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
file(WRITE "${BUILDDIR}/tcc.sum" "${_sum}${_ok} ok\n${_sk} skipped\n${_fa} failed\n${_xf} exe failed\n")
message("${_ok} test(s) ok.")
message("${_sk} test(s) skipped.")
message("${_fa} test(s) failed.")
message("${_xf} test(s) exe failed.")
]==])
    add_custom_target(gcctestsuite
        COMMAND "${CMAKE_COMMAND}" -DTCC=$<TARGET_FILE:tcc>
                "-DBDIR=${CMAKE_BINARY_DIR}" "-DIDIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DTESTSUITE_PATH=${TCC_GCCTESTSUITE_PATH}" -P "${_gccts_drv}"
        DEPENDS tcc USES_TERMINAL
        COMMENT "gcctestsuite: gcc c-torture over tcc")

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
if(NOT DEFINED TCC OR NOT EXISTS "${TCC}")
    message(STATUS "SKIP: ARM tcc not found at '${TCC}'")
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
        execute_process(COMMAND "${TCC}" -o "${_w}/tcc.o" -c "${_w}/in.c"
            RESULT_VARIABLE _trc OUTPUT_QUIET ERROR_QUIET)
        if(NOT _trc EQUAL 0)
            message("warning: '${_text}' did not work in tcc")
            continue()
        endif()
        _dump("${_w}/tcc.o" _got)
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
    if(TARGET arm-tcc)
        add_custom_target(arm-asm-testsuite
            COMMAND "${CMAKE_COMMAND}" -DTCC=$<TARGET_FILE:arm-tcc>
                    "-DCROSS_COMPILE=${TCC_ARM_CROSS_COMPILE}"
                    "-DTOKH=${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm/arm-tok.h" -P "${_armasm_drv}"
            DEPENDS arm-tcc USES_TERMINAL
            COMMENT "arm-asm-testsuite: tcc vs GNU as over arm-tok.h")
    endif()

    file(GLOB _orphan_srcs CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/boundtest.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/testfp.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/bug.c")
    foreach(_f IN LISTS _orphan_srcs)
        get_filename_component(_n "${_f}" NAME_WE)
        add_test(NAME compile.${_n} COMMAND ${TCC_EMULATOR} "${_tcc_exe}"
            "-B${_bdir}" "-I${_idir}" -c "${_f}"
            -o "${CMAKE_BINARY_DIR}/orphan_${_n}.o")
        set_tests_properties(compile.${_n} PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endforeach()
    if(TCC_TARGETOS STREQUAL "WIN32")
        file(GLOB _win_ex CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/examples/*.c")
        foreach(_f IN LISTS _win_ex)
            get_filename_component(_n "${_f}" NAME_WE)
            add_test(NAME compile.win32.${_n} COMMAND ${TCC_EMULATOR} "${_tcc_exe}"
                "-B${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/include" "-I${_idir}"
                -c "${_f}" -o "${CMAKE_BINARY_DIR}/orphan_win_${_n}.o")
            set_tests_properties(compile.win32.${_n} PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        endforeach()
    endif()

    message(STATUS "tcc tests: registered pp + tests2 + libtcc/abitest/tcctest "
                   "+ hello/vla + orphan compile checks (ctest)")
endif()

file(GLOB_RECURSE TCC_TAGFILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.S")
list(FILTER TCC_TAGFILES EXCLUDE REGEX "/(cmake-build-[^/]*|build-c|\\.git|\\.idea)/")
find_program(CTAGS_EXE ctags)
if(CTAGS_EXE)
    add_custom_target(tags
        COMMAND "${CTAGS_EXE}" ${TCC_TAGFILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "ctags ${TCC_TAGFILES}" VERBATIM)
endif()
find_program(ETAGS_EXE etags)
if(ETAGS_EXE)
    add_custom_target(ETAGS
        COMMAND "${ETAGS_EXE}" ${TCC_TAGFILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "etags ${TCC_TAGFILES}" VERBATIM)
endif()
if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _dist_hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_dist_hash)
        set(_dist "tinycc-mob-${_dist_hash}")
        add_custom_target(dist
            COMMAND "${GIT_EXECUTABLE}" archive --format=tar
                    "--prefix=${_dist}/" HEAD
                    -o "${CMAKE_BINARY_DIR}/${_dist}.tar"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "git archive -> ${_dist}.tar" VERBATIM)
    endif()
endif()


set(CPACK_PACKAGE_NAME "tcc")
set(CPACK_PACKAGE_VENDOR "TinyCC")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_CONTACT "tinycc-devel")
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
