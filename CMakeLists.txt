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
        if(MCC_MINGW_ARCH STREQUAL "i686")
            set(${outvar} "${_MCC_MINGW_I686_GCC}" PARENT_SCOPE)
        elseif(MCC_MINGW_ARCH STREQUAL "arm64")
            set(${outvar} "${_MCC_MINGW_ARM64_GCC}" PARENT_SCOPE)
        else()
            set(${outvar} "${_MCC_MINGW_X86_64_GCC}" PARENT_SCOPE)
        endif()
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

function(mcc_gnu_gcc_links_static cc outvar)
    if(NOT WIN32)
        set(${outvar} TRUE PARENT_SCOPE)
        return()
    endif()
    set(_d "${CMAKE_BINARY_DIR}/mcc-gnu-gcc-probe")
    file(MAKE_DIRECTORY "${_d}")
    file(WRITE "${_d}/probe.c"
        "#include <pthread.h>\n"
        "static void *f(void *a) { return a; }\n"
        "int main(void) { pthread_t t; pthread_create(&t, 0, f, 0); "
        "pthread_join(t, 0); return 0; }\n")
    execute_process(
        COMMAND "${cc}" -static "${_d}/probe.c" -o "${_d}/probe.exe" -lpthread
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    if(_rc EQUAL 0)
        set(${outvar} TRUE PARENT_SCOPE)
    else()
        set(${outvar} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(mcc_compiler_family _exe outvar)
    set(_probe "${CMAKE_BINARY_DIR}/CMakeFiles/mcc_family_probe.c")
    file(WRITE "${_probe}" "int main(void){return 0;}\n")
    execute_process(COMMAND "${_exe}" -dM -E "${_probe}"
        OUTPUT_VARIABLE _defs ERROR_QUIET RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        set(${outvar} "unknown" PARENT_SCOPE)
    elseif(_defs MATCHES "__clang__")
        set(${outvar} "clang" PARENT_SCOPE)
    elseif(_defs MATCHES "__GNUC__")
        set(${outvar} "gnu-gcc" PARENT_SCOPE)
    else()
        set(${outvar} "unknown" PARENT_SCOPE)
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
    if(WIN32 AND COMMAND mcc_mingw_resolve)
        mcc_mingw_resolve()
        if(_MCC_MINGW_X86_64_GCC AND EXISTS "${_MCC_MINGW_X86_64_GCC}")
            set(${outvar} "${_MCC_MINGW_X86_64_GCC}" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(_gnu_probe_dirs "")
    if(WIN32)
        if(CMAKE_C_COMPILER MATCHES "[Gg][Cc][Cc]")
            get_filename_component(_cc_dir "${CMAKE_C_COMPILER}" DIRECTORY)
            list(APPEND _gnu_probe_dirs "${_cc_dir}")
        endif()
        foreach(_r
            "$ENV{LOCALAPPDATA}/Programs/CLion"
            "$ENV{LOCALAPPDATA}/JetBrains/Toolbox/apps/CLion"
            "$ENV{USERPROFILE}/scoop/persist/jetbrains-toolbox/apps/CLion"
            "$ENV{USERPROFILE}/scoop/apps/clion/current"
            "$ENV{ProgramFiles}/JetBrains"
            "$ENV{ProgramW6432}/JetBrains")
            if(_r AND EXISTS "${_r}")
                file(GLOB _clion_gccs "${_r}/*/bin/mingw/bin/gcc.exe"
                     "${_r}/bin/mingw/bin/gcc.exe" "${_r}/mingw/bin/gcc.exe")
                foreach(_g IN LISTS _clion_gccs)
                    get_filename_component(_g_dir "${_g}" DIRECTORY)
                    list(APPEND _gnu_probe_dirs "${_g_dir}")
                endforeach()
            endif()
        endforeach()
    endif()
    if(_gnu_probe_dirs)
        list(REMOVE_DUPLICATES _gnu_probe_dirs)
        find_program(MCC_CLION_GCC NAMES gcc PATHS ${_gnu_probe_dirs} NO_DEFAULT_PATH)
        if(MCC_CLION_GCC)
            execute_process(COMMAND "${MCC_CLION_GCC}" --version
                OUTPUT_VARIABLE _v ERROR_QUIET RESULT_VARIABLE _rc
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(_rc EQUAL 0 AND _v MATCHES "Free Software Foundation"
               AND NOT _v MATCHES "clang")
                mcc_gnu_gcc_links_static("${MCC_CLION_GCC}" _ok)
                if(_ok)
                    set(${outvar} "${MCC_CLION_GCC}" PARENT_SCOPE)
                    return()
                endif()
                message(STATUS "mcc: rejecting ${MCC_CLION_GCC} -- its runtime "
                               "cannot -static link winpthread")
            endif()
        endif()
    endif()
    foreach(_n gcc-16 gcc-15 gcc-14 gcc-13 gcc-12 gcc-11 gcc-10 gcc cc)
        find_program(MCC_GNUGCC_${_n} NAMES ${_n})
        if(MCC_GNUGCC_${_n})
            execute_process(COMMAND "${MCC_GNUGCC_${_n}}" --version
                OUTPUT_VARIABLE _v ERROR_QUIET RESULT_VARIABLE _rc
                OUTPUT_STRIP_TRAILING_WHITESPACE)

            if(_rc EQUAL 0 AND _v MATCHES "Free Software Foundation"
               AND NOT _v MATCHES "clang")
                mcc_gnu_gcc_links_static("${MCC_GNUGCC_${_n}}" _ok)
                if(_ok)
                    set(${outvar} "${MCC_GNUGCC_${_n}}" PARENT_SCOPE)
                    return()
                endif()
                message(STATUS "mcc: rejecting ${MCC_GNUGCC_${_n}} -- its runtime "
                               "cannot -static link winpthread")
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

    if(MCC_BUILD_SANITIZE
       AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|ModernCC|TinyCC"
       AND NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        message(FATAL_ERROR
            "mcc config: MCC_BUILD_SANITIZE needs a GCC/Clang, MSVC, or mcc host "
            "compiler (have '${CMAKE_C_COMPILER_ID}'); disable it with "
            "-DMCC_BUILD_SANITIZE=OFF.")
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
    if(NOT MCC_MCCRT_USE_HOSTCC AND FALSE)
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
            "MCC_CONFIG_UCLIBC, no path effect on modern hosts).")
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

set(_elf"NOT MCC_TARGETOS STREQUAL \"WIN32\" AND NOT MCC_TARGETOS STREQUAL \"Darwin\"")
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

mcc_config_node(MCC_GPU_BACKEND TYPE STRING DEFAULT auto GROUP "Diagnostics" ADVANCED
    HELP "Which compute backend the width-ladder GPU oracle is compiled against: metal (Metal + MSL) or vulkan (Vulkan + SPIR-V). auto picks metal on Darwin and vulkan everywhere else. Only Darwin can build both -- Vulkan there runs over MoltenVK, which the loader dlopens as libvulkan.dylib like any other ICD, so nothing is linked either way and the choice is purely which source arm compiles"
    CHOICES auto metal vulkan)

mcc_config_node(MCC_GPU_REQUIRED TYPE BOOL DEFAULT OFF GROUP "Diagnostics" ADVANCED
    HELP "Fail the gpu/* test cells instead of skipping them when no usable compute device is present. Off by default so a machine without a GPU still passes the suite; set it on a CI cell whose whole purpose is to exercise a device, because otherwise every gpu/* cell reports success on a host where the backend never ran and the suite would stay green if both device backends were deleted")

if(MCC_GPU_BACKEND STREQUAL "auto")
    if(APPLE)
        set(_mcc_gpu_backend metal)
    else()
        set(_mcc_gpu_backend vulkan)
    endif()
elseif(MCC_GPU_BACKEND STREQUAL "metal" AND NOT APPLE)
    message(FATAL_ERROR
        "mcc config: MCC_GPU_BACKEND=metal needs a Darwin host (Metal.framework and "
        "the ObjC runtime); this host would build a backend it can never load")
else()
    set(_mcc_gpu_backend "${MCC_GPU_BACKEND}")
endif()

if(_mcc_gpu_backend STREQUAL "metal")
    set(MCC_GPU_LANG_MSL_VALUE 1)
else()
    set(MCC_GPU_LANG_MSL_VALUE 0)
endif()
# Directory scope, not _mccdefs: the config-defines cell reconstructs that list
# from the target triple with mccbuild --emit-defines, so a define it cannot
# derive would fail the comparison. Every target in this directory picks this up,
# including the single-source mcc_s/mcc_t/mcc_dynamic that compile mccgpu.c
# without going through libmcc.
add_compile_definitions(MCC_GPU_LANG_MSL=${MCC_GPU_LANG_MSL_VALUE})
message(STATUS "mcc compute backend: ${_mcc_gpu_backend} "
               "(MCC_GPU_LANG_MSL=${MCC_GPU_LANG_MSL_VALUE})")

set(MCC_COMPUTE_BACKEND_LIBS "")
if(APPLE AND _mcc_gpu_backend STREQUAL "metal")
    set(MCC_COMPUTE_BACKEND_LIBS objc)
endif()
set(MCC_COMPUTE_BACKEND_LDFLAGS "")
foreach(_cbl IN LISTS MCC_COMPUTE_BACKEND_LIBS)
    list(APPEND MCC_COMPUTE_BACKEND_LDFLAGS "-l${_cbl}")
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
mcc_config_node(MCC_MINGW_ARCH TYPE STRING DEFAULT x86_64 GROUP "Build targets" ADVANCED
    HELP "Arch the `mingw` toolchain profile builds mcc for: x86_64/i686 (winlibs gcc) or arm64 (llvm-mingw aarch64)"
    CHOICES x86_64 i686 arm64)
set(MCC_MINGW_DIR "${MCC_VENDOR_DIR}" CACHE PATH
    "Parent directory the downloaded mingw toolchain trees live in")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/winlibs.cmake")

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
    set(_a "${MCC_MINGW_DIR}/llvm-mingw-${MCC_LLVMMINGW_VER}-aarch64/llvm-mingw-${MCC_LLVMMINGW_VER}-ucrt-aarch64/bin/aarch64-w64-mingw32-gcc.exe")
    set(_MCC_MINGW_X86_64_GCC   "${_x}" PARENT_SCOPE)
    set(_MCC_MINGW_I686_GCC     "${_i}" PARENT_SCOPE)
    set(_MCC_MINGW_ARM64_GCC    "${_a}" PARENT_SCOPE)
    set(_MCC_MINGW_MULTILIB_GCC "${_m}" PARENT_SCOPE)
endfunction()

set(_mingw_entries "")
if(MCC_MINGW_ARCH STREQUAL "arm64")
    list(APPEND _mingw_entries
        "${MCC_LLVMMINGW_AARCH64_URL}|${MCC_LLVMMINGW_AARCH64_SHA256}|${MCC_MINGW_DIR}/llvm-mingw-${MCC_LLVMMINGW_VER}-aarch64")
elseif(MCC_MINGW_SOURCE STREQUAL "winlibs")
    if(MCC_MINGW_ARCH STREQUAL "i686")
        list(APPEND _mingw_entries
            "${MCC_MINGW_WINLIBS_I686_URL}|${MCC_MINGW_WINLIBS_I686_SHA256}|${MCC_MINGW_DIR}/winlibs-mingw-w64-${MCC_MINGW_WINLIBS_VER}-i686")
    else()
        list(APPEND _mingw_entries
            "${MCC_MINGW_WINLIBS_X86_64_URL}|${MCC_MINGW_WINLIBS_X86_64_SHA256}|${MCC_MINGW_DIR}/winlibs-mingw-w64-${MCC_MINGW_WINLIBS_VER}-x86_64")
    endif()
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
# scoop's PATH entry is an extensionless shell shim that Windows cannot exec;
# name the .cmd explicitly or execute_process() fails with "not an application".
find_program(_SCOOP NAMES scoop.cmd scoop.exe)
find_program(_CHOCO NAMES choco.exe)

set(_cmd "")
if(WIN32)

    if(TOOL STREQUAL "wine")
        message(STATUS
            "toolchain-setup(wine): not applicable on a Windows host -- mcc's PE "
            "output executes natively, which is what pe-native-conformance covers. "
            "wine is only needed as a PE runner on Linux/macOS hosts.")
        return()
    endif()
    # scoop's `mingw` ships gcc.exe *and* the x86_64-w64-mingw32-* cross names,
    # so it satisfies both the GNU gcc reference and the mingw PE reference.
    if(_SCOOP)
        set(_cmd "${_SCOOP}" install mingw)
    elseif(_CHOCO)
        set(_cmd "${_CHOCO}" install -y mingw)
    endif()
elseif(_BREW)
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
        "(looked for scoop/choco on Windows, brew/apt-get/dnf/pacman/zypper "
        "elsewhere). Install '${TOOL}' manually.")
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
    cmake_host_system_information(RESULT _sb_ncore QUERY NUMBER_OF_LOGICAL_CORES)
    if(NOT _sb_ncore GREATER 0)
        set(_sb_ncore 1)
    endif()
    set(MCC_SUPERBUILD_CTEST_JOBS "${_sb_ncore}" CACHE STRING
        "ctest --parallel level for each superbuild cell's test step (1 = serial)")
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
                    COMMAND "${CMAKE_CTEST_COMMAND}" ${_cell_ctest_cfg}
                            --parallel ${MCC_SUPERBUILD_CTEST_JOBS} --output-on-failure
                            --output-junit ctest-junit.xml
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

if(MCC_ENABLE_CROSS)
    set(_mcc_cross_default "${CMAKE_BINARY_DIR}")
else()
    set(_mcc_cross_default "${CMAKE_SOURCE_DIR}/cmake-cross")
endif()
set(MCC_CROSS_DIR "${_mcc_cross_default}"
    CACHE PATH "cross-compiler build dir for the cross test cells")
mcc_config_node(MCC_CROSS_REQUIRED TYPE BOOL DEFAULT OFF GROUP "Diagnostics" ADVANCED
    HELP "Fail mcc_cross_build instead of skipping it when MCC_CROSS_DIR holds no configured build. Off by default so a tree without a cross build still passes; set it on a CI cell that is supposed to exercise the cross/qemu/macho cells, because otherwise a missing cross dir turns every one of them into a green skip and nothing reports the loss")
mcc_config_node(MCC_CROSS_OPTIMIZER TYPE BOOL DEFAULT ON GROUP "Build targets" ADVANCED
    VISIBLE_WHEN "MCC_ENABLE_CROSS"
    HELP "Build the cross compilers (mcc-<arch>) with the optimizer. ON (default) makes the cross reemit/JIT parity axes (divmagic/select/promotion/replay-inline/embed-jit) real on every triple, matching native; also appends MCC_EMBED_JIT=1. OFF builds the cross compilers without the AST reemit optimizer (smaller/faster cross build, byte-identical to the pre-ungate cross output) but leaves those per-triple parity axes compiled out so they silently no-op.")
mcc_config_node(MCC_CROSS_TARGETS TYPE STRING DEFAULT "" GROUP "Build targets" ADVANCED
    EMPTY_OK VISIBLE_WHEN "MCC_ENABLE_CROSS"
    HELP "Subset of cross triples to build under MCC_ENABLE_CROSS, comma- or semicolon-separated (e.g. x86_64-osx,arm64-osx). Empty (default) builds every triple. Each entry must name one of: i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm64-win32 arm-win32 riscv64 arm64-osx. Narrowing it lets a CI cell build only the object-format compilers its validators need instead of all eleven.")

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
mcc_config_node(MCC_OPT_LADDER_BENCH TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Register the -O ladder benchmarks (optlevel-bench, selfhost-optbench) as ctest cells; they take tens of minutes and want a quiet box, so they are opt-in and carry the label optbench")
mcc_config_node(MCC_ENABLE_RPATH TYPE BOOL DEFAULT ON GROUP "Build targets"
    HELP "Bake -rpath into binaries linking libmcc.so so they find the shared lib at runtime"
    VISIBLE_WHEN "NOT MCC_BUILD_STATIC_LIB")
mcc_config_node(MCC_SINGLE_SOURCE    TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build libmcc from a single TU (Makefile: MCC_AMALGAMATED)")
mcc_config_node(MCC_BUILD_TESTS   TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build/enable the test suite (tests/Makefile)")
mcc_config_node(MCC_BENCH         TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build the mccbench tool and the `bench` target (compile-speed report). Off by default; CI enables it")
mcc_config_node(MCC_STRATSWEEP_FULL TYPE BOOL DEFAULT OFF GROUP "Build targets" ADVANCED
    HELP "Register the exhaustive optimizer strategy sweep: all 22 rows of ast_strategies[] run in isolation over the whole tests/exec corpus (stratsweep/isofull-*), and all 22*21*20 = 9240 ordered triples over the threads/atomics/float/bitfield/VLA/optimizer-golden subject set, bare and again with rows 0..3 in front (stratsweep/perm3-exhaustive-*, ...-based-*). 86 ctest cells, 6440 sec*proc (1.8 CPU-hours, 7min at -j16). The default gate keeps the same 22 rows and the same triple generator but over the 31-program subject list and a 1/1024 shard of the triples, which is 29 cells and 55-145 sec*proc depending on how loaded the machine is. Filter with `ctest -L stratsweep-full`")
mcc_config_node(MCC_CONFIG_TRACE TYPE BOOL DEFAULT OFF GROUP "Advanced" ADVANCED
    HELP "Compile in the MCC_TRACE() instrumentation macro (emitted at the [TRACE] verbosity bit); off = MCC_TRACE compiles to nothing. Separate from MCC_DIAG: ~12,600 call sites, one in nearly every basic block")
mcc_config_node(MCC_DEV TYPE BOOL DEFAULT OFF GROUP "Development" ADVANCED
    HELP "Compile in the AST side-car coherence oracle: recompute every fast-index answer the slow authoritative way and abort on divergence (def/use table, property memos, structural hash, VLat lattice). CI instrument -- it aborts rather than reports. Separate from MCC_DIAG, which instruments the running compiler and changes allocation layout")
mcc_config_node(MCC_REPLAY_IR_C2 TYPE BOOL DEFAULT OFF GROUP "Development" ADVANCED
    HELP "Compile in the Replay-IR C2 re-emit arm (rir_tot_c2_*). Without it the fourteen ast/rir-c2-* cells probe c2try=0 and skip on every host, so their BANKGAP/BANKFN ratchet asserts nothing. On 2026-08-09 the first run with it ON measured gap 14 at -O1 and 13 at -O2/-O3 against a banked 0; the __builtin_expect code-neutrality fix took those to 13 and 12. The remaining bodies are named, attributed and priced in docs/TODO.md, which is also where the reason this stays OFF is recorded")
mcc_config_node(MCC_DIAG TYPE BOOL DEFAULT OFF GROUP "Diagnostics" ADVANCED
    HELP "Everything-on self-scrutiny, one switch. Compiles in every internal instrument -- the MCC_TRACE() per-basic-block tracer, allocator and tiny-allocator leak tracking, one-allocation-per-Sym, the Replay_IR operation trace -- and builds with the verbose warning/debug-info flag set plus the mcc_s (sanitized) and mcc_c (coverage) targets. None of it can be a runtime flag: MCC_TRACE alone expands ~12,600 times, and the allocator/Sym instruments change struct layout. Runtime options are -f flags, see src/mccopt.h")

if(MCC_DIAG)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            message(STATUS "mcc diagnostics: skipping mcc_s (-fsanitize) with GNU gcc on "
                           "macOS (Homebrew gcc ships no linkable libasan/libubsan here)")
        else()
            mcc_profile_seed(MCC_BUILD_SANITIZE ON)
        endif()
        mcc_profile_seed(MCC_BUILD_COVERAGE ON)
    endif()
endif()

mcc_config_node(MCC_BUILD_COVERAGE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_c (coverage instrumentation)")
mcc_config_node(MCC_BUILD_SANITIZE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_s (sanitized: GCC/Clang ASan+UBSan, MSVC ASan, mingw trap-UBSan)")
mcc_config_node(MCC_BUILD_TSAN TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build mcc_t (ThreadSanitizer: GCC/Clang -fsanitize=thread) for the item-1 optimizer-threading audit")
mcc_config_node(MCC_MCCRT_USE_HOSTCC TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build the native mccrt with the host CC instead of mcc (faster bcheck)")
mcc_config_node(MCC_EMBED_MCCRT TYPE BOOL DEFAULT ON GROUP "Build targets"
    HELP "Bake libmccrt.a into the mcc binary (self-contained; no sidecar .a needed). Forces the native mccrt to be host-CC built to break the build cycle. ELF/Mach-O only.")
mcc_config_node(MCC_EMBED_JIT TYPE BOOL DEFAULT ON GROUP "Build targets" ADVANCED
    HELP "Link the §26 Tier-B runtime-JIT engine slice + intention serializer/recompile seam (src/mccjit_embed.c) into mcc, and enable --embed-jit's real effect. Default ON adds the ~800KB embed; OFF keeps the build byte-identical (no runtime JIT machinery baked into mcc or the programs it builds).")
mcc_config_node(MCC_CONFIG_JIT TYPE BOOL DEFAULT ON GROUP "Build targets" ADVANCED
    HELP "Default runtime-JIT activation baked into every program mcc builds (and mcc's own -run). ON = programs JIT by default; the MCC_JIT env var (0/1) overrides at runtime, and mcc's --jit/--no-jit flags override for -run. Requires MCC_EMBED_JIT for any effect.")
mcc_config_node(MCC_CONFIG_AUTOCORRECT TYPE BOOL DEFAULT OFF GROUP "Build targets" ADVANCED
    HELP "Non-strict mode: auto-correct inert/non-runnable config combinations instead of only warning")

mcc_config_node(MCC_CONFIG_MINGW  TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Build a WIN32/mingw target (input: forces MCC_TARGETOS=WIN32)")
set(_mcc_diag_default off)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_mcc_diag_default bounds)
endif()
mcc_config_node(MCC_CONFIG_PREDEFS  TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "Compile mccdefs.h into the binary (c2str)")
mcc_config_node(MCC_CONFIG_PIE      TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Let mcc generate position independent executables"
    VISIBLE_WHEN "${_elf}")
mcc_config_node(MCC_CONFIG_PIC      TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Position independent code"
    VISIBLE_WHEN "${_elf}")
mcc_config_node(MCC_RUN_MMAP_EXEC    TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Force -run executable memory via mmap (separate RW/RX) instead of mprotect; normally auto-detected at runtime (W^X kernels)")
mcc_config_node(MCC_CONFIG_NEW_DTAGS TYPE BOOL DEFAULT OFF GROUP "mcc features"
    HELP "Use DT_RUNPATH instead of DT_RPATH (mcc-emitted; always relevant on ELF)"
    VISIBLE_WHEN "${_elf}")

mcc_config_node(MCC_AUTO_MCCDIR     TYPE BOOL DEFAULT ON GROUP "mcc features"
    HELP "mcc auto-discovers libmccrt.a + headers relative to its own exe (<exe>/include, <exe>/../include, <exe>/../lib/mcc/include), then MCC_CONFIG_MCCDIR, then /usr/local/lib/mcc and /usr/lib/mcc; -B overrides")

mcc_config_node(MCC_CONFIG_LIBC    TYPE STRING DEFAULT "" GROUP "mcc features"
    HELP "Target libc: '', uClibc, or musl" CHOICES uClibc musl EMPTY_OK
    VISIBLE_WHEN "${_elf}")
mcc_config_node(MCC_CONFIG_DWARF   TYPE STRING DEFAULT "" GROUP "mcc features"
    HELP "DWARF debug version (2..5); empty = stabs" CHOICES 0 2 3 4 5 EMPTY_OK)
mcc_config_node(MCC_CONFIG_SEMLOCK TYPE STRING DEFAULT "" GROUP "mcc features"
    HELP "MCC_CONFIG_SEMLOCK value (free-form numeric; empty = mcc.h default 1)")
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

if("${MCC_TARGET_ARCH}" STREQUAL "" AND MCC_MINGW_ARCH STREQUAL "i686")
    set(MCC_TARGET_ARCH "i386")
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

set(_mcc_target_ptr 8)
if(MCC_CPU STREQUAL "i386" OR MCC_CPU STREQUAL "arm")
    set(_mcc_target_ptr 4)
endif()
set(MCC_TARGET_IS_HOST TRUE)
if(DEFINED CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL _mcc_target_ptr)
    set(MCC_TARGET_IS_HOST FALSE)
endif()

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
# T-lin-10068: wire mcc's gcc-style depfile so mcc-built objects rebuild on a header
# change. Gated on the detected mcc id (gcc/clang/msvc keep their own wiring) and empty
# format (don't clobber a future CMake). REF docs/DETAILS.md#t-lin-10068-a-stage-2-build-dir-does
if(MCC_CC_NAME STREQUAL "mcc" AND NOT CMAKE_C_DEPFILE_FORMAT)
    set(CMAKE_C_DEPFILE_FORMAT gcc)
    set(CMAKE_DEPFILE_FLAGS_C "-MD -MT <OBJECT> -MF <DEP_FILE>")
endif()
if(WIN32 AND NOT MCC_CC_NAME STREQUAL "mcc")
    if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
        if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
            add_link_options(-Wl,/STACK:8388608)
        else()
            add_link_options(/STACK:8388608)
        endif()
    else()
        add_link_options(-Wl,--stack,8388608)
    endif()
endif()
if(MCC_CC_NAME STREQUAL "mcc" AND NOT WIN32
   AND NOT CMAKE_EXECUTABLE_RUNTIME_C_FLAG)
    set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG "-Wl,-rpath,")
    set(CMAKE_EXECUTABLE_RUNTIME_C_FLAG_SEP ":")
    set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-Wl,-rpath,")
    set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG_SEP ":")
endif()
if(MCC_CC_NAME STREQUAL "mcc" AND WIN32)
    set(CMAKE_IMPORT_LIBRARY_PREFIX "lib")
    set(CMAKE_IMPORT_LIBRARY_SUFFIX ".def")
    set(CMAKE_LINK_LIBRARY_SUFFIX "")
endif()
if(MCC_CC_NAME STREQUAL "mcc" AND NOT WIN32
   AND NOT CMAKE_C_COMPILE_OPTIONS_PIC)

    set(CMAKE_C_COMPILE_OPTIONS_PIC "-fPIC")
    set(CMAKE_C_COMPILE_OPTIONS_PIE "-fPIE")
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
    HELP "ARM architecture version 4/5/6/7 (MCC_CONFIG_CPUVER); autodetected (__ARM_ARCH)")

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

    message(STATUS "mcc paths: triplet='${MCC_TRIPLET}'")
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
if(MCC_DIAG)
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
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.7")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.7" CACHE STRING
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
    HELP "Runtime mccdir (MCC_CONFIG_MCCDIR); empty = <prefix>/<libdir>/mcc")

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

if(MCC_CONFIG_JIT AND NOT MCC_EMBED_JIT)
    mcc_autocorrect(MCC_CONFIG_JIT OFF
        "MCC_JIT_DEFAULT is inert without MCC_EMBED_JIT")
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
        list(APPEND _mccdefs "MCC_CONFIG_CPUVER=${MCC_CPUVER}")
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
    list(APPEND _mccdefs "MCC_TARGETOS_ANDROID=1")
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
if(MCC_CONFIG_LIBC STREQUAL "musl")
    list(APPEND _mccdefs "MCC_CONFIG_MUSL=1")
elseif(MCC_CONFIG_LIBC STREQUAL "uClibc")
    list(APPEND _mccdefs "MCC_CONFIG_UCLIBC=1")
endif()

if(MCC_RUN_MMAP_EXEC)
    list(APPEND _mccdefs "MCC_CONFIG_RUN_DUALMAP=1")
endif()
if(MCC_CONFIG_PIE)
    list(APPEND _mccdefs "MCC_CONFIG_PIE=1")
endif()
if(MCC_CONFIG_PIC)
    list(APPEND _mccdefs "MCC_CONFIG_PIC=1")
endif()
if(MCC_CONFIG_NEW_DTAGS)
    list(APPEND _mccdefs "MCC_CONFIG_NEW_DTAGS=1")
endif()
if(MCC_CONFIG_CODESIGN)
    list(APPEND _mccdefs "MCC_CONFIG_CODESIGN=1")
endif()

set(_mcc_diag_bt 1)
set(_mcc_diag_bc 1)
if(MCC_EMBED_JIT)
    list(APPEND _mccdefs "MCC_EMBED_JIT=1")
endif()
if(MCC_CONFIG_JIT)
    list(APPEND _mccdefs "MCC_JIT_DEFAULT=1")
else()
    list(APPEND _mccdefs "MCC_JIT_DEFAULT=0")
endif()
if(MCC_DIAG)
    list(APPEND _mccdefs "MCC_DIAG=1")
endif()
if(MCC_DEV)
    list(APPEND _mccdefs "MCC_DEV=1")
endif()
if(MCC_REPLAY_IR_C2)
    list(APPEND _mccdefs "MCC_REPLAY_IR_C2=1")
endif()
if(MCC_CONFIG_TRACE)
    list(APPEND _mccdefs "MCC_CONFIG_TRACE=1")
endif()
if(MCC_CONFIG_NEW_MACHO STREQUAL "no")
    list(APPEND _mccdefs "MCC_CONFIG_MACHO_CHAINED_FIXUPS=0")
endif()

if(NOT "${MCC_CONFIG_DWARF}" STREQUAL "")
    list(APPEND _mccdefs "MCC_CONFIG_DWARF_VERSION=${MCC_CONFIG_DWARF}")
endif()
if(NOT "${MCC_CONFIG_SEMLOCK}" STREQUAL "")
    list(APPEND _mccdefs "MCC_CONFIG_SEMLOCK=${MCC_CONFIG_SEMLOCK}")
endif()

mcc_def_str(_mccdefs MCC_CONFIG_TRIPLET             "${MCC_TRIPLET}")
mcc_def_str(_mccdefs MCC_CONFIG_OS_RELEASE          "${MCC_OS_RELEASE}")

if(NOT "${MCC_SYSROOT}" STREQUAL "")
    list(APPEND _mccdefs "MCC_CONFIG_SYSROOT=\"${MCC_SYSROOT}\"")
endif()
if(NOT MCC_TARGETOS STREQUAL "WIN32")
    if("${MCC_INSTALL_MCCDIR}" STREQUAL "")
        set(_mccdir "${CMAKE_INSTALL_FULL_LIBDIR}/mcc")
    else()
        set(_mccdir "${MCC_INSTALL_MCCDIR}")
    endif()
    list(APPEND _mccdefs "MCC_CONFIG_MCCDIR=\"${_mccdir}\"")
endif()
set(MCC_RESOLVED_MCCDIR "${_mccdir}")

string(REPLACE ";" "\n" _mccdefs_text "${_mccdefs}")
file(WRITE "${CMAKE_BINARY_DIR}/config-defines.txt" "${_mccdefs_text}\n")

set(_selfhost_libs "")
if(NOT MCC_TARGETOS STREQUAL "WIN32")
    list(APPEND _selfhost_libs m ${CMAKE_DL_LIBS})
endif()
list(APPEND _selfhost_libs ${MCC_COMPUTE_BACKEND_LIBS})
set(MCC_SELFHOST_LINK_LIBS "")
foreach(_shl IN LISTS _selfhost_libs)
    if(_shl MATCHES "^-" OR EXISTS "${_shl}")
        list(APPEND MCC_SELFHOST_LINK_LIBS "${_shl}")
    else()
        list(APPEND MCC_SELFHOST_LINK_LIBS "-l${_shl}")
    endif()
endforeach()
string(REPLACE ";" "\n" _selfhost_libs_text "${MCC_SELFHOST_LINK_LIBS}")
file(WRITE "${CMAKE_BINARY_DIR}/selfhost-link-libs.txt" "${_selfhost_libs_text}\n")

set(_mccdefs_common
    "MCC_VERSION=${MCC_VERSION}"
    "CC_NAME=CC_${MCC_CC_NAME}"
    "GCC_MAJOR=${MCC_GCC_MAJOR}"
    "GCC_MINOR=${MCC_GCC_MINOR}"
    "MCC_CONFIG_PREDEFS=${_mcc_predefs_value}")
add_compile_definitions(${_mccdefs_common})

set(_mccdefs_flags "")
foreach(_d IN LISTS _mccdefs)
    if(NOT _d MATCHES "\"")
        list(APPEND _mccdefs_flags "-D${_d}")
    endif()
endforeach()

message(STATUS "mcc build defines: CC_NAME=CC_${MCC_CC_NAME} predefs=${_mcc_predefs_value} "
               "diag-rt=always")

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
    add_compile_definitions(MCC_CONFIG_AUTO_MCCDIR=1)
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
list(REMOVE_ITEM MCC_CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/mccjit_embed.c")
list(REMOVE_ITEM MCC_CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/mccjit_intent.c")
if(MCC_EMBED_JIT)
    list(APPEND MCC_CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/mccjit_embed.c")
    list(APPEND MCC_CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/mccjit_intent.c")
endif()

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
        target_compile_definitions(${tgt} PRIVATE MCC_AMALGAMATED=0)
    else()
        set_source_files_properties(src/libmcc.c PROPERTIES OBJECT_DEPENDS "${_one_src_deps}")
    endif()
    if(TARGET mcc_mccdefs)
        add_dependencies(${tgt} mcc_mccdefs)
    endif()
    target_link_libraries(${tgt} PUBLIC ${MCC_COMPUTE_BACKEND_LIBS})
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
    if(MCC_DIAG)
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

if(MCC_EMBED_JIT)
    if(NOT MCC_BUILD_STATIC_LIB AND MCC_SINGLE_SOURCE)
        mcc_add_libmcc(libmcc_jitengine STATIC FALSE)
    else()
        add_library(libmcc_jitengine STATIC src/libmcc.c)
        set(_je_amalg_deps "")
        foreach(_s IN LISTS LIBMCC_ALL_SRC)
            if(NOT _s STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/src/libmcc.c")
                list(APPEND _je_amalg_deps "${_s}")
            endif()
        endforeach()
        set_source_files_properties(src/libmcc.c PROPERTIES OBJECT_DEPENDS "${_je_amalg_deps}")
        if(TARGET mcc_mccdefs)
            add_dependencies(libmcc_jitengine mcc_mccdefs)
        endif()
        target_include_directories(libmcc_jitengine PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
        if(MCC_CC_NAME STREQUAL "clang" AND MCC_CPU STREQUAL "x86_64")
            set_target_properties(libmcc_jitengine PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
        if(MCC_DIAG)
            target_compile_options(libmcc_jitengine PRIVATE ${MCC_DIAG_FLAGS})
        endif()
    endif()
    set_target_properties(libmcc_jitengine PROPERTIES
        OUTPUT_NAME "mcc-jitengine" PREFIX "lib")
    # The engine blob is linked at run time by mcc's own in-process linker,
    # which provides no MSVC /GS machinery -- with /GS on, every embed-jit
    # link on Windows dies on __report_rangecheckfailure/__security_cookie.
    if(MSVC)
        target_compile_options(libmcc_jitengine PRIVATE /GS-)
    endif()
endif()

add_executable(mcc src/mcc.c)
if(TARGET libmcc_jitengine)
    add_dependencies(mcc libmcc_jitengine)
endif()
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
target_link_libraries(mcc PRIVATE ${MCC_COMPUTE_BACKEND_LIBS})
add_test(NAME "gpu/ladder-gpu-parity"
    COMMAND "${CMAKE_COMMAND}"
            -DMCC=$<TARGET_FILE:mcc>
            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
            -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ladder_gpu_parity.cmake")
set_tests_properties("gpu/ladder-gpu-parity" PROPERTIES
    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
# --jit-always-gpu moves the equivalence sweep onto the device. The device may
# be faster or may refuse; it may never answer differently. Both arms run the
# same census over the same subject and every semantic panel field must match,
# with the emitted object byte-identical. 77 when there is no device, because
# then the GPU arm is the CPU arm and the comparison proves nothing.
add_test(NAME "gpu/always-gpu-parity"
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu/always_gpu_parity.sh"
            "$<TARGET_FILE:mcc>" "${CMAKE_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/subject.c"
            "${CMAKE_BINARY_DIR}/always-gpu-parity" -O4)
set_tests_properties("gpu/always-gpu-parity" PROPERTIES
    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800)
add_test(NAME "gpu/always-gpu-parity-full-language"
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu/always_gpu_parity.sh"
            "$<TARGET_FILE:mcc>" "${CMAKE_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
            "${CMAKE_BINARY_DIR}/always-gpu-parity-fl" -O1
            "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
            "-I${CMAKE_CURRENT_SOURCE_DIR}" "-I${CMAKE_BINARY_DIR}"
            -DCC_NAME=CC_gcc -DGCC_MAJOR=15)
set_tests_properties("gpu/always-gpu-parity-full-language" PROPERTIES
    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800)
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
if(MCC_DIAG)
    target_compile_options(mcc PRIVATE ${MCC_DIAG_FLAGS})
endif()

if(MCC_BUILD_STATIC_EXE)
    add_executable(mcc_static src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    set_target_properties(mcc_static PROPERTIES OUTPUT_NAME mcc-static)
    target_include_directories(mcc_static PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(mcc_static PRIVATE ${MCC_COMPUTE_BACKEND_LIBS})
    if(NOT MCC_SINGLE_SOURCE)
        target_compile_definitions(mcc_static PRIVATE MCC_AMALGAMATED=0)
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
    if(MCC_DIAG)
        target_compile_options(mcc_static PRIVATE ${MCC_DIAG_FLAGS})
    endif()
endif()

if(MCC_BUILD_DYNAMIC_EXE)
    add_executable(mcc_dynamic src/mcc.c)
    set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
    set_target_properties(mcc_dynamic PROPERTIES OUTPUT_NAME mcc-dynamic)
    target_include_directories(mcc_dynamic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(mcc_dynamic PRIVATE ${MCC_COMPUTE_BACKEND_LIBS})
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
    if(MCC_DIAG)
        target_compile_options(mcc_dynamic PRIVATE ${MCC_DIAG_FLAGS})
    endif()
endif()

if(MCC_BUILD_MUSL AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT APPLE)
    mcc_musl_target("${MCC_CPU}" "${MCC_ARM_HARDFLOAT}" _mi _mt)

    set(_musl_vendored OFF)
    if(NOT MCC_MUSL_SYSROOT STREQUAL "" AND NOT _mi STREQUAL "")
        set(_musl_vendored ON)
    endif()

    set(_mccdefs_musl "")
    foreach(_d IN LISTS _mccdefs)
        if(_d MATCHES "^MCC_CONFIG_TRIPLET="
           OR _d MATCHES "^MCC_CONFIG_MUSL=")
        elseif(_musl_vendored AND _d MATCHES "^MCC_CONFIG_SYSROOT=")
        else()
            list(APPEND _mccdefs_musl "${_d}")
        endif()
    endforeach()
    list(APPEND _mccdefs_musl "MCC_CONFIG_MUSL=1")
    if(NOT _mt STREQUAL "")
        list(APPEND _mccdefs_musl "MCC_CONFIG_TRIPLET=\"${_mt}\"")
    endif()
    if(_musl_vendored)
        list(APPEND _mccdefs_musl "MCC_CONFIG_SYSROOT=\"${MCC_MUSL_SYSROOT}\"")
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
            target_compile_definitions(mcc_static_musl PRIVATE MCC_AMALGAMATED=0)
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
    target_link_libraries(mcc_s PRIVATE ${MCC_COMPUTE_BACKEND_LIBS})
    target_compile_definitions(mcc_s PRIVATE MCC_DIAG=1)
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

if(MCC_BUILD_TSAN)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT MCC_TARGETOS STREQUAL "WIN32")
        message(STATUS "mcc tsan (mcc_t): ThreadSanitizer (-fsanitize=thread)")
        add_executable(mcc_t src/mcc.c)
        set_source_files_properties(src/mcc.c PROPERTIES OBJECT_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mcctools.c")
        target_include_directories(mcc_t PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_options(mcc_t PRIVATE -fsanitize=thread -g -O1)
        target_link_options(mcc_t PRIVATE -fsanitize=thread)
        target_link_libraries(mcc_t PRIVATE m ${MCC_COMPUTE_BACKEND_LIBS})
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(mcc_t PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
        endif()
        if(MCC_GITHASH)
            target_compile_definitions(mcc_t PRIVATE "MCC_GITHASH=\"${MCC_GITHASH}\"")
        endif()
        if(TARGET mcc_mccdefs)
            add_dependencies(mcc_t mcc_mccdefs)
        endif()
    else()
        message(STATUS "mcc tsan (mcc_t): skipped (needs GCC/Clang on a non-Win32 host)")
    endif()
endif()

foreach(_nt mcc mcc_static mcc_dynamic libmcc libmcc_shared libmcc_jitengine mcc_s mcc_t)
    if(TARGET ${_nt})
        target_compile_definitions(${_nt} PRIVATE ${_mccdefs})
    endif()
endforeach()

if(MCC_BUILD_STATIC_EXE AND NOT MSVC AND NOT APPLE
   AND NOT MCC_TARGETOS STREQUAL "WIN32")
    foreach(_st mcc_static mcc_static_musl)
        if(TARGET ${_st})
            target_compile_definitions(${_st} PRIVATE MCC_CONFIG_STATIC)
        endif()
    endforeach()
endif()

set(MCC_MCCRT_EXTRA_O runmain bt-exe bt-dll bt-log bcheck mccasan mccubsan mccasan_win32 mccasan_i386_win32 mccasan_arm64_win32)

function(_mcc_mccrt_src obj outvar)
    set(_lib "${CMAKE_CURRENT_SOURCE_DIR}/runtime/lib")
    set(_win "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib")
    if(obj STREQUAL "crt1w")
        set(${outvar} "${_win}/crt1.c" PARENT_SCOPE)
    elseif(obj STREQUAL "wincrt1w")
        set(${outvar} "${_win}/wincrt1.c" PARENT_SCOPE)
    elseif(obj MATCHES "^(crt1|wincrt1|dllcrt1|dllmain|winex|fenv|mccmath)$")
        set(${outvar} "${_win}/${obj}.c" PARENT_SCOPE)
    elseif(obj STREQUAL "mccasan_win32")
        set(${outvar} "${_win}/mccasan_win32.c" PARENT_SCOPE)
    elseif(obj STREQUAL "mccasan_i386_win32")
        set(${outvar} "${_win}/mccasan_i386_win32.c" PARENT_SCOPE)
    elseif(obj STREQUAL "mccasan_arm64_win32")
        set(${outvar} "${_win}/mccasan_arm64_win32.c" PARENT_SCOPE)
    elseif(obj STREQUAL "chkstk")
        set(${outvar} "${_win}/chkstk.c" PARENT_SCOPE)
    else()
        set(${outvar} "${_lib}/${obj}.c" PARENT_SCOPE)
    endif()
endfunction()

function(mcc_build_mccrt tname mcctgt cpu os native prefix san archive objdir out_extra)
    set(_common stdatomic atomic builtin alloca alloca-bt complex complexabi mccchk float16 int256 int512)
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
    if(cpu STREQUAL "x86_64")
        list(APPEND _objs int128)
    endif()

    set(_win OFF)
    set(_osx OFF)
    if(os STREQUAL "WIN32")
        set(_win ON)
    elseif(os STREQUAL "Darwin")
        set(_osx ON)
    endif()

    if(native)
        list(APPEND _objs runmain tcov loopcensus depthcensus)
        if((cpu STREQUAL "x86_64" OR cpu STREQUAL "arm64" OR cpu STREQUAL "riscv64" OR cpu STREQUAL "i386" OR cpu STREQUAL "arm") AND NOT os STREQUAL "WIN32" AND NOT os STREQUAL "Darwin")
            list(APPEND _objs mccasan)
        endif()
        if(cpu STREQUAL "x86_64"
           OR ((cpu STREQUAL "arm64" OR cpu STREQUAL "riscv64" OR cpu STREQUAL "i386" OR cpu STREQUAL "arm")
               AND NOT os STREQUAL "WIN32" AND NOT os STREQUAL "Darwin"))
            list(APPEND _objs mccubsan)
        endif()
        # Always: a compiler that accepts -b but cannot link it is a worse
        # failure than a slightly larger runtime.
        list(APPEND _objs bt-exe bt-log)
        if(_win)
            list(APPEND _objs bt-dll)
        endif()
        list(APPEND _objs bcheck)
    elseif(san AND NOT _win AND NOT _osx)
        if(cpu STREQUAL "x86_64" OR cpu STREQUAL "arm64" OR cpu STREQUAL "riscv64" OR cpu STREQUAL "i386" OR cpu STREQUAL "arm")
            list(APPEND _objs mccasan)
        endif()
        if(cpu STREQUAL "x86_64" OR cpu STREQUAL "arm64" OR cpu STREQUAL "riscv64"
           OR cpu STREQUAL "i386" OR cpu STREQUAL "arm")
            list(APPEND _objs mccubsan)
        endif()
    endif()

    if(_win)
        list(APPEND _objs crt1 crt1w wincrt1 wincrt1w dllcrt1 dllmain winex fenv mccmath)
        if(cpu STREQUAL "i386" OR cpu STREQUAL "x86_64" OR cpu STREQUAL "arm64")
            list(APPEND _objs chkstk)
        endif()
        if(cpu STREQUAL "i386" OR cpu STREQUAL "arm")
            list(APPEND _objs mccubsan)
        endif()
        if(cpu STREQUAL "x86_64")
            list(APPEND _objs mccasan_win32)
        elseif(cpu STREQUAL "i386")
            list(APPEND _objs mccasan_i386_win32)
        elseif(cpu STREQUAL "arm64")
            list(APPEND _objs mccasan_arm64_win32)
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

    if(NOT native)
        list(APPEND _objs runmain)
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
                    -Wl,-oformat=pe-${cpu}
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
            set(_out "${_extra_dir}/${prefix}${obj}.o")
            list(APPEND _extra "${_out}")
        else()
            set(_out "${objdir}/${obj}.o")
            list(APPEND _archived "${_out}")
        endif()
        set(_srcdep "")
        if(obj STREQUAL "int256" OR obj STREQUAL "int512")
            set(_srcdep "${CMAKE_CURRENT_SOURCE_DIR}/src/wide256_arith.h")
        endif()
        add_custom_command(
            OUTPUT "${_out}"
            COMMAND ${_run} "${_cc}" -c "${_src}" -o "${_out}" ${_flags}
            DEPENDS ${_ccdep} "${_src}" ${_srcdep}
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
mcc_build_mccrt(mccrt mcc "${MCC_CPU}" "${MCC_TARGETOS}" ON "" ON
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

if(MCC_EMBED_JIT AND TARGET libmcc_jitengine)
    if(NOT TARGET bin2c)
        add_executable(bin2c tools/bin2c.c)
    endif()

    set(MCC_EMBED_JIT_MINGW_CC "" CACHE STRING
        "Path to a msvcrt-targeting mingw gcc used to build the --embed-jit blob engine on MSVC hosts.")

    # On an MSVC host, cl.exe cannot produce a linkable --embed-jit blob: its ucrt
    # __imp_* imports and MSVC CRT-startup COMMONs (__isa_available_default,
    # _dowildcard, and the blob's own statics) are mis-placed by mcc's own PE
    # linker, so the standalone embed-jit exe fails to link / crashes at startup
    # (see DETAILS.md T-win-50020). Default the blob compiler to the project's
    # already-provisioned mingw gcc so a plain MSVC configure still yields a
    # working --embed-jit; the mingw blob references the mingw CRT import libs
    # (added by libmcc.c under MCC_EMBED_JIT_MINGW_LIBDIR), which mcc resolves
    # against msvcrt.dll + the api-ms-win-crt-* apiset present on every modern
    # Windows. Override by setting MCC_EMBED_JIT_MINGW_CC explicitly.
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND NOT MCC_EMBED_JIT_MINGW_CC)
        mcc_mingw_resolve()
        if(_MCC_MINGW_X86_64_GCC AND EXISTS "${_MCC_MINGW_X86_64_GCC}")
            set(MCC_EMBED_JIT_MINGW_CC "${_MCC_MINGW_X86_64_GCC}")
            message(STATUS "mcc: embed-jit blob compiler auto-set to the provisioned mingw gcc "
                "(MSVC cannot build a linkable blob): ${MCC_EMBED_JIT_MINGW_CC}")
        else()
            message(WARNING "mcc: MCC_EMBED_JIT on an MSVC host with no mingw gcc found at "
                "'${_MCC_MINGW_X86_64_GCC}'. The cl.exe blob will NOT link a standalone "
                "--embed-jit exe. Provision the mingw toolchain or set MCC_EMBED_JIT_MINGW_CC. "
                "See DETAILS.md T-win-50020.")
        endif()
    endif()

    set(_jitengine_blob_src "$<TARGET_FILE:libmcc_jitengine>")
    set(_jitengine_blob_dep libmcc_jitengine)
    set(_jit_bake_cc "${CMAKE_C_COMPILER}")
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND MCC_EMBED_JIT_MINGW_CC)
        set(_jit_bake_cc "${MCC_EMBED_JIT_MINGW_CC}")
        get_filename_component(_je_bindir "${MCC_EMBED_JIT_MINGW_CC}" DIRECTORY)
        find_program(MCC_EMBED_JIT_MINGW_AR NAMES ar llvm-ar
            PATHS "${_je_bindir}" NO_DEFAULT_PATH)
        get_directory_property(_je_dirdefs COMPILE_DEFINITIONS)
        set(_je_dflags "")
        foreach(_d IN LISTS _je_dirdefs)
            list(APPEND _je_dflags "-D${_d}")
        endforeach()
        set(_je_deps "${CMAKE_CURRENT_SOURCE_DIR}/src/libmcc.c")
        if(TARGET mcc_mccdefs)
            list(APPEND _je_deps mcc_mccdefs)
        endif()
        set(_je_o "${CMAKE_BINARY_DIR}/mccjit_engine_mingw.o")
        set(_je_a "${CMAKE_BINARY_DIR}/libmccjitengine-mingw.a")
        add_custom_command(OUTPUT "${_je_o}"
            COMMAND "${MCC_EMBED_JIT_MINGW_CC}" -c -w -O1
                ${_je_dflags}
                "-D$<JOIN:$<TARGET_PROPERTY:libmcc_jitengine,COMPILE_DEFINITIONS>,;-D>"
                "-I${CMAKE_CURRENT_SOURCE_DIR}" "-I${CMAKE_BINARY_DIR}"
                "-I$<JOIN:$<TARGET_PROPERTY:libmcc_jitengine,INCLUDE_DIRECTORIES>,;-I>"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/libmcc.c"
                -o "${_je_o}"
            DEPENDS ${_je_deps}
            COMMENT "embed-jit: compiling the blob engine with msvcrt-mingw (${MCC_EMBED_JIT_MINGW_CC})"
            VERBATIM COMMAND_EXPAND_LISTS)
        add_custom_command(OUTPUT "${_je_a}"
            COMMAND "${MCC_EMBED_JIT_MINGW_AR}" rcs "${_je_a}" "${_je_o}"
            DEPENDS "${_je_o}"
            COMMENT "embed-jit: archiving the msvcrt-mingw blob engine"
            VERBATIM)
        set(_jitengine_blob_src "${_je_a}")
        set(_jitengine_blob_dep "${_je_a}")
    endif()

    set(MCC_EMBED_JIT_MCC_ENGINE_CC "" CACHE STRING
        "Path to an mcc that emits this target's PE (e.g. mcc-i386-win32), used to build the --embed-jit blob engine with no gcc footprint (fixes the i386-PE embed startup crash).")
    set(_me_cc "${MCC_EMBED_JIT_MCC_ENGINE_CC}")
    set(_me_cc_dep "")
    if(NOT MCC_EMBED_JIT_MCC_ENGINE_CC
       AND MCC_TARGETOS STREQUAL "WIN32" AND MCC_CPU MATCHES "^(i386|x86_64)$"
       AND NOT MSVC AND NOT MCC_CC_NAME STREQUAL "mcc"
       AND NOT CMAKE_CROSSCOMPILING)
        add_executable(mcc_ejboot src/mcc.c)
        set_target_properties(mcc_ejboot PROPERTIES OUTPUT_NAME mcc-ejboot)
        target_include_directories(mcc_ejboot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_definitions(mcc_ejboot PRIVATE ${_mccdefs})
        if(NOT "${MCC_EXTRA_LIBS}" STREQUAL "")
            target_link_libraries(mcc_ejboot PRIVATE ${_mcc_extra_libs})
        endif()
        if(TARGET mcc_mccdefs)
            add_dependencies(mcc_ejboot mcc_mccdefs)
        endif()
        set(_me_cc "$<TARGET_FILE:mcc_ejboot>")
        set(_me_cc_dep mcc_ejboot)
    endif()
    if(_me_cc AND MCC_TARGETOS STREQUAL "WIN32")
        get_directory_property(_me_dirdefs COMPILE_DEFINITIONS)
        set(_me_dflags "")
        foreach(_d IN LISTS _me_dirdefs)
            list(APPEND _me_dflags "-D${_d}")
        endforeach()
        set(_me_deps "${CMAKE_CURRENT_SOURCE_DIR}/src/libmcc.c")
        if(TARGET mcc_mccdefs)
            list(APPEND _me_deps mcc_mccdefs)
        endif()
        if(_me_cc_dep)
            list(APPEND _me_deps ${_me_cc_dep})
        endif()
        set(_me_o "${CMAKE_BINARY_DIR}/mccjit_engine_mcc.o")
        set(_me_a "${CMAKE_BINARY_DIR}/libmccjitengine-mcc.a")
        add_custom_command(OUTPUT "${_me_o}"
            COMMAND "${_me_cc}" -c -w -O1
                "-B${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
                ${_me_dflags}
                "-D$<JOIN:$<TARGET_PROPERTY:libmcc_jitengine,COMPILE_DEFINITIONS>,;-D>"
                "-I${CMAKE_CURRENT_SOURCE_DIR}" "-I${CMAKE_BINARY_DIR}"
                "-I$<JOIN:$<TARGET_PROPERTY:libmcc_jitengine,INCLUDE_DIRECTORIES>,;-I>"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/libmcc.c"
                -o "${_me_o}"
            DEPENDS ${_me_deps}
            COMMENT "embed-jit: compiling the blob engine WITH mcc -- no gcc footprint"
            VERBATIM COMMAND_EXPAND_LISTS)
        add_custom_command(OUTPUT "${_me_a}"
            COMMAND "${_me_cc}" -ar "${_me_a}" "${_me_o}"
            DEPENDS "${_me_o}"
            COMMENT "embed-jit: archiving the mcc-built blob engine"
            VERBATIM)
        set(_jitengine_blob_src "${_me_a}")
        set(_jitengine_blob_dep "${_me_a}")
        target_compile_definitions(mcc PRIVATE MCC_EMBED_JIT_MCC_ENGINE=1)
    endif()

    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/mccjit_blob.c"
        COMMAND "$<TARGET_FILE:bin2c>" "${_jitengine_blob_src}"
                "${CMAKE_BINARY_DIR}/mccjit_blob.c" mccjit_blob
        DEPENDS bin2c ${_jitengine_blob_dep}
        COMMENT "Embedding the static JIT engine into mcc (mccjit_blob.c)"
        VERBATIM)
    target_sources(mcc PRIVATE "${CMAKE_BINARY_DIR}/mccjit_blob.c")
    target_compile_definitions(mcc PRIVATE MCC_EMBED_JIT_BLOB=1)
    if(MCC_CC_NAME STREQUAL "mcc")
    elseif(MCC_TARGETOS STREQUAL "WIN32")
        execute_process(COMMAND "${_jit_bake_cc}" -print-file-name=libmingwex.a
            OUTPUT_VARIABLE _mccjit_mingwex OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND "${_jit_bake_cc}" -print-libgcc-file-name
            OUTPUT_VARIABLE _mccjit_libgcc OUTPUT_STRIP_TRAILING_WHITESPACE)
        get_filename_component(_mccjit_mingw_libdir "${_mccjit_mingwex}" DIRECTORY)
        get_filename_component(_mccjit_gcc_libdir "${_mccjit_libgcc}" DIRECTORY)
        file(TO_CMAKE_PATH "${_mccjit_mingw_libdir}" _mccjit_mingw_libdir)
        file(TO_CMAKE_PATH "${_mccjit_gcc_libdir}" _mccjit_gcc_libdir)
        if(_mccjit_mingw_libdir AND _mccjit_gcc_libdir)
            target_compile_definitions(mcc PRIVATE
                "MCC_EMBED_JIT_MINGW_LIBDIR=\"${_mccjit_mingw_libdir}\""
                "MCC_EMBED_JIT_GCC_LIBDIR=\"${_mccjit_gcc_libdir}\"")
            message(STATUS "mcc: embed-jit runtime libs: ${_mccjit_mingw_libdir} + ${_mccjit_gcc_libdir}")
            if(EXISTS "${_mccjit_mingw_libdir}/libucrt.a")
                file(COPY "${_mccjit_mingw_libdir}/libucrt.a"
                     DESTINATION "${CMAKE_BINARY_DIR}/lib")
            endif()
        endif()
    elseif(NOT WIN32 AND NOT APPLE)
        execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-libgcc-file-name
            OUTPUT_VARIABLE _mccjit_libgcc OUTPUT_STRIP_TRAILING_WHITESPACE)
        get_filename_component(_mccjit_gcc_libdir "${_mccjit_libgcc}" DIRECTORY)
        file(TO_CMAKE_PATH "${_mccjit_gcc_libdir}" _mccjit_gcc_libdir)
        if(_mccjit_gcc_libdir)
            target_compile_definitions(mcc PRIVATE
                "MCC_EMBED_JIT_GCC_LIBDIR=\"${_mccjit_gcc_libdir}\"")
            message(STATUS "mcc: embed-jit runtime libs: ${_mccjit_gcc_libdir}")
        endif()
    endif()
endif()

mcc_config_node(MCC_BUILD_JIT_SELFHOST TYPE BOOL DEFAULT OFF GROUP "Build targets" ADVANCED
    VISIBLE_WHEN "MCC_EMBED_JIT"
    HELP "EXPERIMENTAL. Build mcc-jit: a stage-2 mcc self-compiled with --embed-jit (JITs its own hot functions at runtime). ELF + Apple Mach-O; PE bake path still gated off. The runtime hot-swap path can still SIGBUS on a complex JIT'd variant under heavier promotion — boots + self-compiles crash-free but is not yet proven crash-free under full runtime promotion.")

function(mcc_add_jit_selfhost out_path stage0_mcc)
    get_directory_property(_dirdefs COMPILE_DEFINITIONS)
    set(_dflags "")
    foreach(_d IN LISTS _dirdefs)
        list(APPEND _dflags "-D${_d}")
    endforeach()
    set(_blobs "${CMAKE_BINARY_DIR}/mccjit_blob.c")
    if(EXISTS "${CMAKE_BINARY_DIR}/mccrt_blob.c" OR MCC_EMBED_MCCRT)
        list(APPEND _blobs "${CMAKE_BINARY_DIR}/mccrt_blob.c")
    endif()
    set(_shlibs -lm -lpthread -ldl)
    if(MCC_TARGETOS STREQUAL "WIN32")
        set(_shlibs "")
    endif()
    add_custom_command(OUTPUT "${out_path}"
        COMMAND "$<TARGET_FILE:${stage0_mcc}>" --embed-jit -O1
            ${_dflags}
            "-D$<JOIN:$<TARGET_PROPERTY:${stage0_mcc},COMPILE_DEFINITIONS>,;-D>"
            "-I${CMAKE_CURRENT_SOURCE_DIR}" "-I${CMAKE_BINARY_DIR}"
            "-I$<JOIN:$<TARGET_PROPERTY:${stage0_mcc},INCLUDE_DIRECTORIES>,;-I>"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c" ${_blobs}
            ${_shlibs}
            -o "${out_path}"
        DEPENDS ${stage0_mcc} "${CMAKE_BINARY_DIR}/mccjit_blob.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
        COMMENT "JIT self-host: ${stage0_mcc} --embed-jit -> ${out_path}"
        VERBATIM COMMAND_EXPAND_LISTS)
endfunction()

if(MCC_BUILD_JIT_SELFHOST AND MCC_EMBED_JIT AND TARGET libmcc_jitengine)
    if(WIN32)
        set(_mccjit_out "${CMAKE_BINARY_DIR}/mcc-jit.exe")
    else()
        set(_mccjit_out "${CMAKE_BINARY_DIR}/mcc-jit")
    endif()
    mcc_add_jit_selfhost("${_mccjit_out}" mcc)
    add_custom_target(mcc-jit-selfhost ALL DEPENDS "${_mccjit_out}")
    message(STATUS "mcc: JIT self-host target 'mcc-jit-selfhost' -> ${_mccjit_out} enabled (${MCC_CPU})")
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
                ${MCC_COMPUTE_BACKEND_LDFLAGS}
        DEPENDS mcc mccrt "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
        COMMENT "mcc -ftest-coverage mcc.c -> mcc_c" VERBATIM)
    add_custom_target(mcc_cov ALL DEPENDS "${CMAKE_BINARY_DIR}/mcc_c")
endif()

add_custom_target(mcc-stats
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/mcc-stats-cache"
    COMMAND ${CMAKE_COMMAND} -E env
            "XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/mcc-stats-cache"
            MCC_SEARCH_WORKER=1
            ${MCC_EMULATOR} "$<TARGET_FILE:mcc>" -O13 -fopt-search --stats
            -c "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
            -o "${CMAKE_BINARY_DIR}/mcc-stats.o"
            "-I${CMAKE_BINARY_DIR}" "-I${CMAKE_CURRENT_SOURCE_DIR}"
            "-I${CMAKE_CURRENT_SOURCE_DIR}/src"
            "-I${CMAKE_CURRENT_SOURCE_DIR}/src/formats"
            "-I${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt"
            "-I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386"
            "-I${CMAKE_CURRENT_SOURCE_DIR}/src/arch/${MCC_CPU}"
            "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
            "-B${CMAKE_CURRENT_SOURCE_DIR}" "-B${CMAKE_BINARY_DIR}"
    DEPENDS mcc mccrt
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    VERBATIM USES_TERMINAL
    COMMENT "mcc -O13 --stats: 14s live optimizer/JIT search stats panel over src/mcc.c")

if(MCC_EMBED_JIT)
    find_program(MCC_PYTHON3 NAMES python3 python)
    if(MCC_PYTHON3)
        add_custom_target(mcc-jit-selfcheck
            COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-jit.py"
                    "${CMAKE_BINARY_DIR}" "${MCC_CPU}"
            DEPENDS mcc mccrt
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            VERBATIM USES_TERMINAL
            COMMENT "JIT self-host self-check: mcc --jit -O13 -run src/mcc.c, assert byte-identical to AOT")
    endif()
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
        target_link_libraries(${_vt} PRIVATE ${MCC_COMPUTE_BACKEND_LIBS})
        if(NOT MCC_TARGETOS STREQUAL "WIN32")
            target_link_libraries(${_vt} PRIVATE m)
            find_package(Threads)
            if(Threads_FOUND)
                target_link_libraries(${_vt} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
            endif()
        endif()
        if(_vk STREQUAL "static")
            if(NOT MCC_TARGETOS STREQUAL "WIN32")
                target_compile_definitions(${_vt} PRIVATE MCC_CONFIG_STATIC)
            endif()
            target_link_options(${_vt} PRIVATE -static)
        endif()
        if(MCC_DIAG)
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
              arm64-win32 arm-win32 riscv64 arm64-osx)
    if(NOT "${MCC_CROSS_TARGETS}" STREQUAL "")
        string(REPLACE "," ";" _xwant "${MCC_CROSS_TARGETS}")
        foreach(_w IN LISTS _xwant)
            if(NOT _w IN_LIST MCC_X)
                message(FATAL_ERROR
                    "MCC_CROSS_TARGETS: unknown cross triple '${_w}' (known: ${MCC_X})")
            endif()
        endforeach()
        set(MCC_X ${_xwant})
        message(STATUS "mcc: cross targets narrowed to ${MCC_X}")
        unset(_xwant)
    endif()
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
        elseif(_t STREQUAL "arm-win32")
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
            list(APPEND _cpath_defs "MCC_CONFIG_SYSROOT=\"/usr/${_ctriplet}\"")
        endif()

        set(_cross_defs ${_cdefs} ${_cpath_defs}
            "MCC_CONFIG_CROSSPREFIX=\"${_t}-\"")
        if(MCC_CROSS_OPTIMIZER)
            if(MCC_EMBED_JIT)
                list(APPEND _cross_defs "MCC_EMBED_JIT=1")
            endif()
        endif()
        if(_cos STREQUAL "WIN32")
            list(APPEND _cross_defs "MCC_CONFIG_MCCDIR=\"${_mccdir_install}/win32\"")
        elseif(_cos STREQUAL "Darwin")
            list(APPEND _cross_defs "MCC_CONFIG_CODESIGN=1")
        endif()
        mcc_add_cross_mcc(mcc-${_t} "mcc-${_t}"
                          mcc-${_t}-static "mcc-${_t}-static" _cross_defs)

        set(_csan OFF)
        if(NOT _ctriplet STREQUAL "" AND _cos STREQUAL "Linux"
           AND EXISTS "/usr/${_ctriplet}/include/stdio.h")
            set(_csan ON)
        endif()
        mcc_build_mccrt(${_t}-mccrt mcc-${_t} "${_ccpu}" "${_cos}" OFF "${_t}-" ${_csan}
            "${CMAKE_BINARY_DIR}/${_t}-libmccrt.a"
            "${CMAKE_BINARY_DIR}/lib-${_t}" _cross_extra)
        install(FILES "${CMAKE_BINARY_DIR}/${_t}-libmccrt.a"
            DESTINATION "${_mccdir_install}")
        if(NOT "${_cross_extra}" STREQUAL "")
            install(FILES ${_cross_extra}
                DESTINATION "${_mccdir_install}")
        endif()

        if(MCC_BUILD_MUSL AND NOT _ctriplet STREQUAL "" AND _cos STREQUAL "Linux")
            mcc_musl_target("${_ccpu}" ON _cmi _cmt)
            set(_cpath_defs_musl "MCC_CONFIG_SYSROOT=\"/usr/${_cmt}\"")
            set(_cross_defs_musl ${_cdefs} ${_cpath_defs_musl}
                "MCC_CONFIG_MUSL=1" "MCC_CONFIG_CROSSPREFIX=\"${_t}-\"")
            mcc_add_cross_mcc(mcc-${_t}-musl "mcc-${_t}-musl"
                              mcc-${_t}-static-musl "mcc-${_t}-static-musl" _cross_defs_musl)
        endif()
    endforeach()

    # T-lin-10047: register tools/arm64pe_diff.py as a live cell with a floor.
    # It byte-diffs the arm64-PE (arm64-win32) mcc's emission against the proven
    # native-arm64 ELF oracle, so it needs both cross compilers plus python's
    # `capstone` disassembler.  The known-positive cell strips the datamodel
    # opt-out marker from a `long`-width source and REQUIRES the harness to flag
    # it -- proving the cell is not blind.
    if(MCC_PYTHON3 AND ("arm64" IN_LIST MCC_X) AND ("arm64-win32" IN_LIST MCC_X))
        add_test(NAME "cross/arm64pe-diff"
            COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arm64pe_diff.py"
                    --corpus --mcc-elf "$<TARGET_FILE:mcc-arm64>"
                    --mcc-pe "$<TARGET_FILE:mcc-arm64-win32>")
        set_tests_properties("cross/arm64pe-diff" PROPERTIES
            LABELS "native;cross" SKIP_RETURN_CODE 77)
        add_test(NAME "cross/arm64pe-diff-known-positive"
            COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/arm64pe_diff.py"
                    --known-positive --mcc-elf "$<TARGET_FILE:mcc-arm64>"
                    --mcc-pe "$<TARGET_FILE:mcc-arm64-win32>")
        set_tests_properties("cross/arm64pe-diff-known-positive" PROPERTIES
            LABELS "native;cross" SKIP_RETURN_CODE 77)
        message(STATUS "mcc: registered cross/arm64pe-diff (+known-positive) codegen cell")
    else()
        # Gate-symmetric registration (ci/registration-stubs): where python3 or a
        # cross compiler is missing the cells must still appear to ctest -N under
        # the same names, as a skip with a reason -- not vanish.  mcc_skip_test is
        # not defined this early, so inline what it expands to.
        set(_a64pe_why "requires python3 (capstone) + the arm64 and arm64-win32 cross compilers -- build with cmake --preset cross, or narrow MCC_CROSS_TARGETS to include both")
        add_test(NAME "cross/arm64pe-diff"
            COMMAND "${CMAKE_COMMAND}" -E echo "SKIP: ${_a64pe_why}")
        set_tests_properties("cross/arm64pe-diff" PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP:" LABELS "native;cross")
        add_test(NAME "cross/arm64pe-diff-known-positive"
            COMMAND "${CMAKE_COMMAND}" -E echo "SKIP: ${_a64pe_why}")
        set_tests_properties("cross/arm64pe-diff-known-positive" PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIP:" LABELS "native;cross")
    endif()
else()
    # Gate-symmetry (ci/registration-stubs): with cross disabled the arm64-win32
    # compiler does not exist, so cross/arm64pe-diff cannot run; register it under
    # the same names as a skip so ctest -N counts it either way.
    set(_a64pe_why2 "requires MCC_ENABLE_CROSS (the arm64-win32 cross compiler) + python3 capstone -- configure with cmake --preset cross")
    add_test(NAME "cross/arm64pe-diff"
        COMMAND "${CMAKE_COMMAND}" -E echo "SKIP: ${_a64pe_why2}")
    set_tests_properties("cross/arm64pe-diff" PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP:" LABELS "native;cross")
    add_test(NAME "cross/arm64pe-diff-known-positive"
        COMMAND "${CMAKE_COMMAND}" -E echo "SKIP: ${_a64pe_why2}")
    set_tests_properties("cross/arm64pe-diff-known-positive" PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIP:" LABELS "native;cross")
endif()

if(MCC_TARGETOS STREQUAL "WIN32" AND _libmcc_shared
   AND NOT (WIN32 AND MCC_CC_NAME STREQUAL "mcc"))
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
    target_compile_definitions(ci PRIVATE
        "MCC_CMAKE_COMMAND=\"${CMAKE_COMMAND}\"")

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
        if(NOT MCC_TARGETOS STREQUAL "WIN32")
            target_link_libraries(mccbench m)
        endif()
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
            COMMENT "Benchmarking compilers (self-hosted mcc per -O level vs gcc/clang/mingw/msvc) via mccbench")
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

    # The two independent reference compilers. Resolved here, before the first
    # consumer: the cref-oracle corpora gate on these, and a resolution placed
    # after them registered four skips on a clean tree and real cells only on
    # the second configure, once find_program's cache entry existed.
    set(MCC_DIFF3_GCC "" CACHE FILEPATH
        "Explicit gcc/mingw reference compiler for the diff3/preprocess suites (overrides auto-detect)")
    set(MCC_DIFF3_CLANG "" CACHE FILEPATH
        "Explicit clang reference compiler for the diff3/preprocess suites (overrides auto-detect)")

    if(MCC_DIFF3_GCC)
        set(DIFF3_GCC "${MCC_DIFF3_GCC}")
    else()
        # A real GNU gcc first, and only then `gcc`/`cc` from PATH: on macOS
        # those two are Apple clang, and clang adjudicating clang is not a
        # cross oracle. Probing in this order also keeps the cache entry
        # honest -- find_program would otherwise record the shim that the
        # normal-variable set() below it shadows.
        mcc_find_gnu_gcc(_diff3_gnu_gcc)
        if(_diff3_gnu_gcc)
            set(DIFF3_GCC "${_diff3_gnu_gcc}")
        else()
            find_program(DIFF3_GCC NAMES gcc cc)
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
    set(MCC_DIFF3_SAME_FAMILY OFF)
    if(DIFF3_GCC AND DIFF3_CLANG)
        mcc_compiler_family("${DIFF3_GCC}" _diff3_gcc_family)
        mcc_compiler_family("${DIFF3_CLANG}" _diff3_clang_family)
        if(_diff3_gcc_family STREQUAL _diff3_clang_family)
            set(MCC_DIFF3_SAME_FAMILY ON)
            message(STATUS
                "mcc diff3 oracles: ${DIFF3_GCC} and ${DIFF3_CLANG} are BOTH "
                "${_diff3_gcc_family} -- these two references are one compiler, so a "
                "three-way cell here cannot tell 'mcc is wrong' from 'the references "
                "disagree and mcc picked one'. Every diff3/cref-oracle verdict on this "
                "host is mcc-vs-${_diff3_gcc_family}, not a cross oracle. Set "
                "MCC_DIFF3_GCC to a real GNU gcc to restore it.")
        else()
            message(STATUS "mcc diff3 oracles: gcc=${DIFF3_GCC} (${_diff3_gcc_family}) "
                           "clang=${DIFF3_CLANG} (${_diff3_clang_family})")
        endif()
    endif()

    # Resolved here, before the first consumer, for the reason the diff3 pair
    # above documents: the compile.ex* cells and slice-census's `wide` corpus
    # both need it, slice-census registers first, and a find_path placed after
    # it handed the cell an empty string on a clean tree -- so the X11 examples
    # were dropped from the census on the first configure and included on the
    # second, from the cache.
    set(_x11_search "${MCC_SYSROOT}/usr/include" /usr/include /opt/X11/include
                    /usr/X11R6/include /usr/local/include /opt/homebrew/include)
    unset(MCC_X11_INCLUDE_DIR CACHE)
    find_path(MCC_X11_INCLUDE_DIR NAMES X11/Xlib.h PATHS ${_x11_search} NO_DEFAULT_PATH)

    function(mcc_skip_test _name _reason)
        add_test(NAME ${_name} COMMAND "${CMAKE_COMMAND}" -E echo "SKIP: ${_reason}")
        set_tests_properties(${_name} PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:" ${ARGN})
    endfunction()

    function(mcc_cross_cc _arch _out _fixout)
        if(TARGET mcc-${_arch})
            set(${_out} "$<TARGET_FILE:mcc-${_arch}>" PARENT_SCOPE)
            set(${_fixout} "MCC_BUILT" PARENT_SCOPE)
        elseif(EXISTS "${MCC_CROSS_DIR}/mcc-${_arch}${MCC_EXESUF}")
            set(${_out} "${MCC_CROSS_DIR}/mcc-${_arch}${MCC_EXESUF}" PARENT_SCOPE)
            set(${_fixout} "MCC_BUILT;MCC_CROSS_BUILT" PARENT_SCOPE)
        else()
            set(${_out} "" PARENT_SCOPE)
            set(${_fixout} "MCC_BUILT" PARENT_SCOPE)
        endif()
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

        file(GLOB_RECURSE _selfcheck_srcs "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/*.c")
        string(REPLACE ";" "\n" _selfcheck_list "${_selfcheck_srcs}")
        file(WRITE "${CMAKE_BINARY_DIR}/sanitize-selfcheck.list" "${_selfcheck_list}\n")
        add_test(NAME sanitize-selfcheck COMMAND "${CMAKE_COMMAND}"
            "-DMCC=$<TARGET_FILE:mcc_s>" "-DBDIR=${_bdir}" "-DIDIR=${_idir}"
            "-DSRCLIST=${CMAKE_BINARY_DIR}/sanitize-selfcheck.list"
            "-DOUT=${CMAKE_BINARY_DIR}/sanitize-selfcheck"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/sanitize/run_selfcheck.cmake")
        set_tests_properties(sanitize-selfcheck PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            TIMEOUT 600)
    else()
        mcc_skip_test(sanitize-smoke
            "needs the ASan/UBSan build of mcc (mcc_s); configure with -DMCC_BUILD_SANITIZE=ON, e.g. cmake --preset sanitize")
        mcc_skip_test(sanitize-selfcheck
            "needs the ASan/UBSan build of mcc (mcc_s); configure with -DMCC_BUILD_SANITIZE=ON, e.g. cmake --preset sanitize")
    endif()

    set(_ubsan_run OFF)
    if(MCC_CPU STREQUAL "x86_64" AND NOT MCC_EMULATOR)
        set(_ubsan_run ON)
    elseif((NOT MCC_TARGETOS STREQUAL "WIN32") AND (NOT MCC_TARGETOS STREQUAL "Darwin")
           AND MCC_CPU MATCHES "^(x86_64|arm64|i386|arm|riscv64)$"
           AND (MCC_EMULATOR OR NOT CMAKE_CROSSCOMPILING))
        set(_ubsan_run ON)
    endif()
    set(_ubsan_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/sanitize/ubsan")
    file(GLOB _ubsan_traps RELATIVE "${_ubsan_dir}" "${_ubsan_dir}/trap_*.c")
    if(_ubsan_run)
        foreach(_uf IN LISTS _ubsan_traps)
            string(REGEX REPLACE "\\.c$" "" _un "${_uf}")
            add_test(NAME "ubsan/${_un}" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}" "-DEMU=${MCC_EMULATOR}"
                "-DSRC=${_ubsan_dir}/${_uf}"
                "-DOUT=${CMAKE_BINARY_DIR}/ubsan-${_un}${MCC_EXESUF}"
                "-DMODE=trap"
                -P "${_ubsan_dir}/run_ubsan.cmake")
            set_tests_properties("ubsan/${_un}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()
        add_test(NAME "ubsan/clean_arith" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}" "-DEMU=${MCC_EMULATOR}"
            "-DSRC=${_ubsan_dir}/clean_arith.c"
            "-DOUT=${CMAKE_BINARY_DIR}/ubsan-clean_arith${MCC_EXESUF}"
            "-DMODE=clean"
            "-DEXPECT=70000 -10000 1200000000 1200000000 1073741824 40000 0"
            -P "${_ubsan_dir}/run_ubsan.cmake")
        set_tests_properties("ubsan/clean_arith" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "ubsan/clean_unsigned_wrap" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}" "-DEMU=${MCC_EMULATOR}"
            "-DSRC=${_ubsan_dir}/clean_unsigned_wrap.c"
            "-DOUT=${CMAKE_BINARY_DIR}/ubsan-clean_unsigned_wrap${MCC_EXESUF}"
            "-DMODE=clean"
            "-DEXPECT=3705032704 1983905792 2147483648"
            -P "${_ubsan_dir}/run_ubsan.cmake")
        set_tests_properties("ubsan/clean_unsigned_wrap" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "ubsan/recover_signed_add" COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}" "-DEMU=${MCC_EMULATOR}"
            "-DSRC=${_ubsan_dir}/recover_signed_add.c"
            "-DOUT=${CMAKE_BINARY_DIR}/ubsan-recover_signed_add${MCC_EXESUF}"
            "-DMODE=recover"
            "-DEXPECT=survived"
            -P "${_ubsan_dir}/run_ubsan.cmake")
        set_tests_properties("ubsan/recover_signed_add" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    else()
        set(_ubsan_why "-fsanitize=undefined trap/recover validated on x86_64 (ELF/Mach-O/PE) + ELF arm64/i386/arm/riscv64 (native or qemu); this target/format not covered or lacks an emulator")
        foreach(_uf IN LISTS _ubsan_traps)
            string(REGEX REPLACE "\\.c$" "" _un "${_uf}")
            mcc_skip_test("ubsan/${_un}" "${_ubsan_why}")
        endforeach()
        mcc_skip_test("ubsan/clean_arith" "${_ubsan_why}")
        mcc_skip_test("ubsan/clean_unsigned_wrap" "${_ubsan_why}")
        mcc_skip_test("ubsan/recover_signed_add" "${_ubsan_why}")
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

        add_executable(ckretired tools/ckretired.c tools/toolsupport.c)
        target_link_libraries(ckretired ${CMAKE_DL_LIBS})
        add_test(NAME retired-macro-invariant
            COMMAND "$<TARGET_FILE:ckretired>" src tools runtime CMakeLists.txt
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(retired-macro-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_executable(targetgate tools/targetgate.c tools/toolsupport.c)
        target_link_libraries(targetgate ${CMAKE_DL_LIBS})
        add_test(NAME target-gate-invariant
            COMMAND "$<TARGET_FILE:targetgate>" src tools
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(target-gate-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_executable(deadgate tools/deadgate.c tools/toolsupport.c)
        target_link_libraries(deadgate ${CMAKE_DL_LIBS})
        add_test(NAME dead-gate-invariant
            COMMAND "$<TARGET_FILE:deadgate>" src tools
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(dead-gate-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_executable(idiomgate tools/idiomgate.c tools/toolsupport.c)
        target_link_libraries(idiomgate ${CMAKE_DL_LIBS})
        add_test(NAME idiom-gate-invariant
            COMMAND "$<TARGET_FILE:idiomgate>" --registry src tools runtime include
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(idiom-gate-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME idiom-gate-known-positive
            COMMAND "${CMAKE_COMMAND}"
                    -DGATE=$<TARGET_FILE:idiomgate>
                    -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/idiomgate_known_positive.cmake")
        set_tests_properties(idiom-gate-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        add_executable(tracegate tools/tracegate.c tools/toolsupport.c)
        target_link_libraries(tracegate ${CMAKE_DL_LIBS})
        add_test(NAME trace-gate-invariant
            COMMAND "$<TARGET_FILE:tracegate>" src
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(trace-gate-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME trace-gate-known-positive
            COMMAND "${CMAKE_COMMAND}"
                    -DGATE=$<TARGET_FILE:tracegate>
                    -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tracegate_known_positive.cmake")
        set_tests_properties(trace-gate-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        add_executable(schemagate tools/schemagate.c tools/toolsupport.c)
        target_link_libraries(schemagate ${CMAKE_DL_LIBS})
        add_test(NAME schema-gate-invariant
            COMMAND "$<TARGET_FILE:schemagate>" src
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(schema-gate-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_executable(fixpointgate tools/fixpointgate.c tools/toolsupport.c)
        target_link_libraries(fixpointgate ${CMAKE_DL_LIBS})
        if(NOT WIN32 AND CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
            add_test(NAME fixpoint-invariant
                COMMAND "$<TARGET_FILE:fixpointgate>" "${CMAKE_BINARY_DIR}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
            set_tests_properties(fixpoint-invariant PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endif()
    endif()

    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(asttool tools/asttool.c)
        target_include_directories(asttool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        if(MCC_DEV)
            target_compile_definitions(asttool PRIVATE MCC_DEV=1)
        endif()
        foreach(_an arena clone wide validate dump cfg template intention color forecast gatemap magic vlat combo_walk slice_ident slice_window slice_persist slice_graduate slice_splice slice_locate)
            add_test(NAME "ast/${_an}" COMMAND asttool "${_an}")
            set_tests_properties("ast/${_an}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()

        if(MCC_GPU_REQUIRED)
            set(_gpu_skip77 "")
        else()
            set(_gpu_skip77 SKIP_RETURN_CODE 77)
        endif()

        find_package(Vulkan QUIET)
        if(NOT Vulkan_FOUND AND APPLE AND NOT Vulkan_INCLUDE_DIR)
            find_path(Vulkan_INCLUDE_DIR vulkan/vulkan.h
                PATHS /opt/homebrew/include /usr/local/include
                      /opt/homebrew/opt/vulkan-headers/include
                      /usr/local/opt/vulkan-headers/include)
        endif()
        if(NOT Vulkan_FOUND AND APPLE AND Vulkan_INCLUDE_DIR)
            find_library(MCC_MOLTENVK_LIB MoltenVK
                PATHS /opt/homebrew/lib /usr/local/lib
                      /opt/homebrew/opt/molten-vk/lib /usr/local/opt/molten-vk/lib)
            if(MCC_MOLTENVK_LIB)
                set(Vulkan_LIBRARY "${MCC_MOLTENVK_LIB}")
                set(Vulkan_LIBRARIES "${MCC_MOLTENVK_LIB}")
                set(Vulkan_INCLUDE_DIRS "${Vulkan_INCLUDE_DIR}")
                set(Vulkan_FOUND TRUE)
                message(STATUS
                    "mcc: Vulkan headers without a loader; linking spvgate against "
                    "${MCC_MOLTENVK_LIB}")
            endif()
        endif()
        if(Vulkan_FOUND)
            add_executable(spvgate tools/spvgate.c tools/spvgate_arena.c)
            target_include_directories(spvgate PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/formats"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms"
                "${CMAKE_CURRENT_SOURCE_DIR}/include"
                ${Vulkan_INCLUDE_DIRS})
            target_link_libraries(spvgate ${Vulkan_LIBRARIES})
            add_test(NAME "gpu/spv-slice-differential" COMMAND spvgate)
            set_tests_properties("gpu/spv-slice-differential" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT ${_gpu_skip77})
            add_test(NAME "gpu/spv-slice-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DCMD=$<TARGET_FILE:spvgate>
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvgate_mutate.cmake")
            set_tests_properties("gpu/spv-slice-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "gpu/spv-slice-real"
                COMMAND "${CMAKE_COMMAND}"
                        -DGATE=$<TARGET_FILE:spvgate>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvgate_real.cmake")
            set_tests_properties("gpu/spv-slice-real" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)

            add_test(NAME "gpu/spv-mem-binding" COMMAND spvgate --mem)
            set_tests_properties("gpu/spv-mem-binding" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT ${_gpu_skip77})
            add_test(NAME "gpu/spv-mem-binding-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DCMD=$<TARGET_FILE:spvgate>
                        -DGATEARGS=--mem
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvgate_mutate.cmake")
            set_tests_properties("gpu/spv-mem-binding-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)

            find_program(MCC_SPIRV_VAL NAMES spirv-val)
            if(MCC_SPIRV_VAL)
                message(STATUS "mcc: spirv-val = ${MCC_SPIRV_VAL}")
            else()
                message(STATUS "mcc: no spirv-val; gpu/spv-validate* will skip")
            endif()
            add_test(NAME "gpu/spv-validate"
                COMMAND "${CMAKE_COMMAND}"
                        -DGATE=$<TARGET_FILE:spvgate>
                        -DSPIRV_VAL=${MCC_SPIRV_VAL}
                        -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/spv-validate
                        -DEXPECT=176
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvval.cmake")
            set_tests_properties("gpu/spv-validate" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "gpu/spv-const-cache"
                COMMAND "${CMAKE_COMMAND}"
                        -DGATE=$<TARGET_FILE:spvgate>
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/const_cache_lowers.cmake")
            set_tests_properties("gpu/spv-const-cache" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME "gpu/spv-validate-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DGATE=$<TARGET_FILE:spvgate>
                        -DSPIRV_VAL=${MCC_SPIRV_VAL}
                        -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/spv-validate-kp
                        -DEXPECT=176
                        -DCORRUPT=6
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvval.cmake")
            set_tests_properties("gpu/spv-validate-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
        else()
            set(_spv_why "no Vulkan headers/loader on this host, so the spvgate differential tool cannot be built (install the Vulkan SDK, or MoltenVK on Darwin)")
            mcc_skip_test("gpu/spv-slice-differential" "${_spv_why}")
            mcc_skip_test("gpu/spv-slice-known-positive" "${_spv_why}")
            mcc_skip_test("gpu/spv-slice-real" "${_spv_why}")
            mcc_skip_test("gpu/spv-validate" "${_spv_why}")
            mcc_skip_test("gpu/spv-validate-known-positive" "${_spv_why}")
            mcc_skip_test("gpu/spv-const-cache" "${_spv_why}")
            mcc_skip_test("gpu/spv-mem-binding" "${_spv_why}")
            mcc_skip_test("gpu/spv-mem-binding-known-positive" "${_spv_why}")
        endif()

        if(APPLE AND MCC_GPU_LANG_MSL_VALUE)
            add_executable(mslgate tools/spvgate.c tools/spvgate_arena.c
                                   src/mccgpu.c)
            target_compile_definitions(mslgate PRIVATE SPVGATE_MSL=1)
            target_include_directories(mslgate PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/formats"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms"
                "${CMAKE_CURRENT_SOURCE_DIR}/include")
            target_link_libraries(mslgate ${MCC_COMPUTE_BACKEND_LIBS})
            add_test(NAME "gpu/msl-slice-differential" COMMAND mslgate)
            set_tests_properties("gpu/msl-slice-differential" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT ${_gpu_skip77})
            add_test(NAME "gpu/msl-slice-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DCMD=$<TARGET_FILE:mslgate>
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvgate_mutate.cmake")
            set_tests_properties("gpu/msl-slice-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "gpu/msl-slice-real"
                COMMAND "${CMAKE_COMMAND}"
                        -DGATE=$<TARGET_FILE:mslgate>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvgate_real.cmake")
            set_tests_properties("gpu/msl-slice-real" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "gpu/msl-const-cache"
                COMMAND "${CMAKE_COMMAND}"
                        -DGATE=$<TARGET_FILE:mslgate>
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/const_cache_lowers.cmake")
            set_tests_properties("gpu/msl-const-cache" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME "gpu/msl-mem-binding" COMMAND mslgate --mem)
            set_tests_properties("gpu/msl-mem-binding" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT ${_gpu_skip77})
            add_test(NAME "gpu/msl-mem-binding-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DCMD=$<TARGET_FILE:mslgate>
                        -DGATEARGS=--mem
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spvgate_mutate.cmake")
            set_tests_properties("gpu/msl-mem-binding-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
        else()
            set(_msl_why "the MSL slice differential needs a Darwin host with the Metal backend selected (-DMCC_GPU_BACKEND=metal)")
            mcc_skip_test("gpu/msl-slice-differential" "${_msl_why}")
            mcc_skip_test("gpu/msl-slice-known-positive" "${_msl_why}")
            mcc_skip_test("gpu/msl-slice-real" "${_msl_why}")
            mcc_skip_test("gpu/msl-const-cache" "${_msl_why}")
            mcc_skip_test("gpu/msl-mem-binding" "${_msl_why}")
            mcc_skip_test("gpu/msl-mem-binding-known-positive" "${_msl_why}")
        endif()

        # slicerun: the scheduler cells. Unlike spvgate it brings no Vulkan of
        # its own -- it dispatches through src/mccgpu.c, which dlopens the ICD --
        # so it needs no SDK, builds on every host, and is the only cell that
        # covers the production device layer's dispatch path directly.
        if(NOT CMAKE_CROSSCOMPILING)
            add_executable(slicerun tools/slicerun.c tools/slicerun_arena.c src/mccgpu.c src/mccrt.c)
            target_include_directories(slicerun PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/formats"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms"
                "${CMAKE_CURRENT_SOURCE_DIR}/include")
            if(MCC_COMPUTE_BACKEND_LIBS)
                target_link_libraries(slicerun ${MCC_COMPUTE_BACKEND_LIBS})
            endif()
            if(NOT WIN32)
                target_link_libraries(slicerun ${CMAKE_DL_LIBS})
            endif()
            # SKIP_RETURN_CODE covers `frame`, whose kernel builder has no Metal
            # arm (TODO.md §5 stage M2): there the suite exits 77 rather than
            # asserting that lowering succeeded. The other four never return 77.
            foreach(_sr task work cpu sched frame effect thread)
                add_test(NAME "slice/${_sr}" COMMAND slicerun "${_sr}")
                set_tests_properties("slice/${_sr}" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            endforeach()
            # T-lin-10405: the CPU<->GPU transaction demonstration -- the CPU
            # seeds a statement run's live-in, the GPU executes it, and the CPU
            # consumes the live-out (c=17, a=170 for a=10,b=7). Asserts the
            # consumed device result, proving cross-slice run+communicate.
            add_test(NAME "slice/txn" COMMAND slicerun txn)
            set_tests_properties("slice/txn" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                PASS_REGULAR_EXPRESSION "gpu-txn.*cpu consumed c=17 a=170")
            # T-lin-10405 Phase C/D: the frame-equivalence-gated accumulator
            # transaction. The bounded-loop accumulator frame is certified
            # GPU==CPU over an input range (Phase C), delegated to the device
            # and its returned value consumed (Phase D, GPU-taken + parity), and
            # a mcc_slice_set_mutate known-positive proves the gate is live: a
            # perturbed device kernel still dispatches but is refused certification.
            set(_jittxn_args jittxn)
            if(MCC_GPU_REQUIRED)
                list(APPEND _jittxn_args --require-device)
            endif()
            add_test(NAME "slice/jit-txn" COMMAND slicerun ${_jittxn_args})
            set_tests_properties("slice/jit-txn" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                PASS_REGULAR_EXPRESSION "accumulator delegated to GPU.*bit-identical to CPU")
            add_test(NAME "slice/effect-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_effect_mutate.cmake")
            set_tests_properties("slice/effect-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME "slice/thread-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_thread_mutate.cmake")
            set_tests_properties("slice/thread-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
            foreach(_sg gpu wide64 f64 ops mem bytes deref ext arrow fmt hostimport rwstore)
                set(_sg_args "${_sg}")
                if(MCC_GPU_REQUIRED)
                    list(APPEND _sg_args --require-device)
                endif()
                if(_sg STREQUAL "fmt")
                    list(APPEND _sg_args --fmt-cost-report)
                endif()
                add_test(NAME "slice/${_sg}" COMMAND slicerun ${_sg_args})
                set_tests_properties("slice/${_sg}" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            endforeach()
            # T-lin-10035: the physical device is scored, not devs[0]. Asserts
            # the initialised device is the highest-scored one; skips (77) on a
            # host with fewer than two usable devices.
            add_test(NAME "gpu/device-score"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu/device-score.sh"
                        $<TARGET_FILE:slicerun>)
            set_tests_properties("gpu/device-score" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            # The 64-bit differential must be able to fail, or "0 mismatches"
            # is indistinguishable from "0 tuples compared".
            foreach(_sk wide64 f64 ops frame gpu sched bytes deref ext arrow fmt hostimport)
                add_test(NAME "slice/${_sk}-known-positive"
                    COMMAND "${CMAKE_COMMAND}"
                            -DRUNNER=$<TARGET_FILE:slicerun>
                            -DSUITE=${_sk}
                            -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_mutate.cmake")
                set_tests_properties("slice/${_sk}-known-positive" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            endforeach()
            add_test(NAME "slice/f64cross"
                COMMAND $<TARGET_FILE:slicerun> f64cross --device-or-skip)
            set_tests_properties("slice/f64cross" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 600
                ENVIRONMENT "MCC_XORACLE_GCC=${DIFF3_GCC};MCC_XORACLE_CLANG=${DIFF3_CLANG}")
            add_test(NAME "slice/f64cross-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_f64cross.cmake")
            set_tests_properties("slice/f64cross-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 600
                ENVIRONMENT "MCC_XORACLE_GCC=${DIFF3_GCC};MCC_XORACLE_CLANG=${DIFF3_CLANG}")
            # T-win-50022 slice 1b: mcc holds every device at once and routes to each.
            # On a single-device host this routes over 1 and still checks it; on a
            # multi-GPU host (e.g. win RTX 2060 + Radeon) it dispatches to both.
            add_test(NAME "slice/route"
                COMMAND $<TARGET_FILE:slicerun> route --device-or-skip)
            set_tests_properties("slice/route" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)
            add_test(NAME "slice/route-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DSUITE=route
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_mutate.cmake")
            set_tests_properties("slice/route-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)
            add_test(NAME "gpu/device-cap"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu/run-gpu-devcap.sh"
                        $<TARGET_FILE:slicerun>)
            set_tests_properties("gpu/device-cap" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)
            add_test(NAME "gpu/vram-budget"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu/run-gpu-vrambudget.sh"
                        $<TARGET_FILE:slicerun>)
            set_tests_properties("gpu/vram-budget" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)
            foreach(_sk ops frame)
                add_test(NAME "slice/${_sk}-lohi-fallback"
                    COMMAND "${CMAKE_COMMAND}"
                            -DRUNNER=$<TARGET_FILE:slicerun>
                            -DSUITE=${_sk}
                            -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_mutate.cmake")
                set_tests_properties("slice/${_sk}-lohi-fallback" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    ENVIRONMENT "MCC_SLICE_NO_LOHI=1")
            endforeach()
            # Runs alone: a stranded dispatch disables the device for the rest
            # of the process, which is the behaviour under test.
            # List, not a genex -- see the slice/${_sg} loop above for why an
            # empty generator expression silently disarmed these cells.
            set(_fault_args fault)
            if(MCC_GPU_REQUIRED)
                list(APPEND _fault_args --require-device)
            endif()
            add_test(NAME "slice/fault" COMMAND slicerun ${_fault_args})
            set_tests_properties("slice/fault" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            # L2'(ii)/(iii). Also runs alone, and for a stronger reason than
            # slice/fault: it loses the device on purpose and then tears it
            # down, so nothing else in the process has a device afterwards.
            set(_lost_args lost)
            if(MCC_GPU_REQUIRED)
                list(APPEND _lost_args --require-device)
            endif()
            add_test(NAME "slice/device-lost" COMMAND slicerun ${_lost_args})
            set_tests_properties("slice/device-lost" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            # The healthy teardown, which the two cells above cannot reach:
            # they quiesce a device that is lost or stranded, where destroying
            # nothing is correct. Alone for the same reason -- the device is
            # destroyed on purpose.
            set(_quiesce_args quiesce)
            if(MCC_GPU_REQUIRED)
                list(APPEND _quiesce_args --require-device)
            endif()
            add_test(NAME "slice/quiesce" COMMAND slicerun ${_quiesce_args})
            set_tests_properties("slice/quiesce" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            # H6, Phase 0a: the cost table. Informational, but it must produce
            # rows and at least one slice size at which the device wins, or the
            # measurement silently stopped measuring.
            add_test(NAME "slice/cost" COMMAND slicerun --cost-synth)
            set_tests_properties("slice/cost" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                PASS_REGULAR_EXPRESSION "win-within-[0-9]+-lanes=[1-9]")
            add_test(NAME "slice/musl"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_musl.cmake")
            set_tests_properties("slice/musl" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 600)
            add_test(NAME "slice/real"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_real.cmake")
            set_tests_properties("slice/real" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "slice/src"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_src.cmake")
            set_tests_properties("slice/src" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 600)

                # name@oracle@suite@MINPROG@MINTUPLE@cflags. The floors are
                # coverage guards: they exist so that "0 mismatches" cannot mean
                # "0 tuples compared". Two of them were briefly lowered here on
                # the evidence of a Darwin run that measured 2111 tuples against
                # 4000 for unittests and 32 against 40 for compiler-rt-builtins.
                # That evidence was an artefact of the cref quarantine, which
                # withheld most slices from the oracle; wt/uacfix lifted it and
                # the same corpora now yield 5043 and 2096. The original values
                # stand -- do not lower them again without checking that the
                # quarantine is not what is suppressing the count.
                set(_corpora
                    "gcc-c-torture-execute@CLANG@GCC@1000@50000@-w,-std=gnu11,-fpermissive,-lm"
                    "llvm-test-suite-regression-c@GCC@CLANG@700@10000@-w,-std=gnu11,-fpermissive,-lm"
                    "llvm-test-suite-unittests@GCC@CLANG@120@4000@-w,-std=gnu11,-fpermissive,-lm"
                    "compiler-rt-builtins-unit@GCC@CLANG@100@40@-w,-std=gnu11,-Ivendor/compiler-rt-builtins-unit,-Ivendor/compiler-rt-lib-builtins,-lm")
            if(MCC_DIFF3_SAME_FAMILY)
                set(_cref_why "${DIFF3_GCC} and ${DIFF3_CLANG} are the same compiler family, so these cells would adjudicate one implementation against itself; set MCC_DIFF3_GCC to a real GNU gcc")
            else()
                set(_cref_why "needs python3 plus both gcc and clang as independent oracles; a single-compiler run would not be a cross oracle")
            endif()
            if(MCC_PYTHON3 AND DIFF3_GCC AND DIFF3_CLANG AND NOT MCC_DIFF3_SAME_FAMILY)
                add_test(NAME "slice/cref-oracle"
                    COMMAND "${CMAKE_COMMAND}"
                            -DPY=${MCC_PYTHON3}
                            -DTOOL=${CMAKE_CURRENT_SOURCE_DIR}/tools/gpuconform.py
                            -DRUNNER=$<TARGET_FILE:slicerun>
                            -DMCC=$<TARGET_FILE:mcc>
                            -DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu/cref
                            -DORACLE_CC=${DIFF3_CLANG}
                            -DSUITE_CC=${DIFF3_GCC}
                            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cref-oracle
                            -DMINPROG=7
                            -DMINTUPLE=300
                            "-DCFLAGS=-w,-std=gnu11"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gpuconform_cref.cmake")
                set_tests_properties("slice/cref-oracle" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT TIMEOUT 900
                    SKIP_REGULAR_EXPRESSION "SKIP:")

                foreach(_spec IN LISTS _corpora)
                    string(REPLACE "@" ";" _f "${_spec}")
                    list(GET _f 0 _cc)
                    list(GET _f 1 _o)
                    list(GET _f 2 _su)
                    list(GET _f 3 _mp)
                    list(GET _f 4 _mt)
                    list(GET _f 5 _ccf)
                    if(_o STREQUAL "CLANG")
                        set(_ocl "${DIFF3_CLANG}")
                        set(_scl "${DIFF3_GCC}")
                    else()
                        set(_ocl "${DIFF3_GCC}")
                        set(_scl "${DIFF3_CLANG}")
                    endif()
                    string(REPLACE "vendor/" "${MCC_VENDOR_DIR}/" _ccf "${_ccf}")
                    if(EXISTS "${MCC_VENDOR_DIR}/${_cc}")
                        add_test(NAME "slice/cref-oracle-${_cc}"
                            COMMAND "${CMAKE_COMMAND}"
                                    -DPY=${MCC_PYTHON3}
                                    -DTOOL=${CMAKE_CURRENT_SOURCE_DIR}/tools/gpuconform.py
                                    -DRUNNER=$<TARGET_FILE:slicerun>
                                    -DMCC=$<TARGET_FILE:mcc>
                                    -DCORPUS=${MCC_VENDOR_DIR}/${_cc}
                                    -DORACLE_CC=${_ocl}
                                    -DSUITE_CC=${_scl}
                                    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cref-${_cc}
                                    -DMINPROG=${_mp}
                                    -DMINTUPLE=${_mt}
                                    "-DCFLAGS=${_ccf}"
                                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gpuconform_cref.cmake")
                        set_tests_properties("slice/cref-oracle-${_cc}" PROPERTIES
                            FIXTURES_REQUIRED MCC_BUILT TIMEOUT 7200
                            SKIP_REGULAR_EXPRESSION "SKIP:"
                            LABELS "crefcorpus")
                    else()
                        mcc_skip_test("slice/cref-oracle-${_cc}"
                            "no corpus at ${MCC_VENDOR_DIR}/${_cc} (symlink an upstream checkout there; it is deliberately not vendored)"
                            LABELS "crefcorpus")
                    endif()
                endforeach()
            else()
                mcc_skip_test("slice/cref-oracle" "${_cref_why}")
                foreach(_spec IN LISTS _corpora)
                    string(REPLACE "@" ";" _f "${_spec}")
                    list(GET _f 0 _cc)
                    mcc_skip_test("slice/cref-oracle-${_cc}" "${_cref_why}"
                        LABELS "crefcorpus")
                endforeach()
            endif()

            add_test(NAME "slice/inline"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_inline.cmake")
            set_tests_properties("slice/inline" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "slice/mcc-leaf-graft"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mcc_leaf_graft.cmake")
            set_tests_properties("slice/mcc-leaf-graft" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "slice/depth-bailout"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mcc_depth_bailout.cmake")
            set_tests_properties("slice/depth-bailout" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            # No capability gate: -fdepth-census instruments and the runtime
            # helper links on every native host, so there is nothing to probe
            # for and nothing to stub.
            add_test(NAME "depth-census-control"
                COMMAND "${CMAKE_COMMAND}"
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/depth_census_control.cmake")
            set_tests_properties("depth-census-control" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
            # The census counts blocks and callee classes over a compiled corpus,
            # so every figure is a property of the AST/RIR shape. The corpus is
            # header-free (tests/census, compiled -nostdinc), so one column serves
            # every platform -- no per-{arch,os} key is needed (T-lin-10391).
            add_test(NAME "slice/census"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_census.cmake")
            set_tests_properties("slice/census" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "slice/refusal-classes"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_refclass.cmake")
            set_tests_properties("slice/refusal-classes" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "slice/noslot-classes"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DMCC_GPU_REQUIRED=${MCC_GPU_REQUIRED}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_noslot.cmake")
            set_tests_properties("slice/noslot-classes" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            add_test(NAME "slice/arena-intern-cap"
                COMMAND "${CMAKE_COMMAND}"
                        -DRUNNER=$<TARGET_FILE:slicerun>
                        -DMCC=$<TARGET_FILE:mcc>
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slicerun_interncap.cmake")
            set_tests_properties("slice/arena-intern-cap" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
            find_program(MCC_PYTHON3 NAMES python3 python)
            if(MCC_PYTHON3)
                add_test(NAME "fmt/census-oracle"
                    COMMAND "${MCC_PYTHON3}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt-census.py"
                            --selfcheck
                            "--oracle=$<TARGET_FILE:slicerun>"
                            "--root=${CMAKE_CURRENT_SOURCE_DIR}")
                set_tests_properties("fmt/census-oracle" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT)
                add_test(NAME "fmt/census-oracle-known-positive"
                    COMMAND "${CMAKE_COMMAND}"
                            -DPY=${MCC_PYTHON3}
                            -DCENSUS=${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt-census.py
                            -DORACLE=$<TARGET_FILE:slicerun>
                            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fmt_census_mutate.cmake")
                set_tests_properties("fmt/census-oracle-known-positive" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT)
                # The arena census reads the build's own flags for src/mcc.c back
                # out of compile_commands.json. CMAKE_EXPORT_COMPILE_COMMANDS is on,
                # but the Visual Studio (MSBuild) generator ignores it and emits no
                # such file, so the take cannot run there -- skip rather than hard
                # -fail, exactly as the GPU cells skip with no device. Ninja-based
                # Windows builds (the mingw preset) still run it.
                if(CMAKE_GENERATOR MATCHES "Visual Studio")
                    set(_arena_why "the arena census needs compile_commands.json to reproduce the build's flags for src/mcc.c; the Visual Studio generator does not emit one -- configure with Ninja (e.g. --preset mingw) to run this cell")
                    mcc_skip_test("fmt/arena-census-bank" "${_arena_why}")
                    mcc_skip_test("fmt/arena-census-known-positive" "${_arena_why}")
                else()
                    add_test(NAME "fmt/arena-census-bank"
                        COMMAND "${MCC_PYTHON3}"
                                "${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt-census.py"
                                "--arena-check=${CMAKE_CURRENT_BINARY_DIR}"
                                "--root=${CMAKE_CURRENT_SOURCE_DIR}")
                    set_tests_properties("fmt/arena-census-bank" PROPERTIES
                        FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)
                    add_test(NAME "fmt/arena-census-known-positive"
                        COMMAND "${CMAKE_COMMAND}"
                                -DPY=${MCC_PYTHON3}
                                -DCENSUS=${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt-census.py
                                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                                -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fmt_arena_census_mutate.cmake")
                    set_tests_properties("fmt/arena-census-known-positive"
                        PROPERTIES FIXTURES_REQUIRED MCC_BUILT TIMEOUT 600)
                endif()
            else()
                set(_fmtcensus_why "tools/fmt-census.py needs a python3 interpreter and none is on PATH")
                mcc_skip_test("fmt/census-oracle" "${_fmtcensus_why}")
                mcc_skip_test("fmt/census-oracle-known-positive" "${_fmtcensus_why}")
                mcc_skip_test("fmt/arena-census-bank" "${_fmtcensus_why}")
                mcc_skip_test("fmt/arena-census-known-positive" "${_fmtcensus_why}")
            endif()
        endif()

        # Sweep row 17's correctness half. The cell carries its own
        # known-positive rather than needing a second one: it fails at any
        # commit before the ast_reemit FDE emit, naming each re-emitted
        # function, and its ast.orphan_fn floor is what stops it passing
        # vacuously if the subject ever stops triggering the re-emit.
        if(UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR
           AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TARGETOS STREQUAL "Darwin")
            add_test(NAME "osx/headers-parse"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/osx/headers-parse.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        --min-files 201)
            set_tests_properties("osx/headers-parse" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 600)

            add_test(NAME "osx/headers-parse-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/tests/osx/headers-parse.sh
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DMINFILES=201
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/osx_headers_mutate.cmake")
            set_tests_properties("osx/headers-parse-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 900)

            add_test(NAME "ast/reemit-cfi"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/reemit-cfi.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/reemit-cfi")
            set_tests_properties("ast/reemit-cfi" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 120)

            add_test(NAME "ast/inline-divguard"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/inline-divguard.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/inline-divguard")
            set_tests_properties("ast/inline-divguard" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 120)

            add_test(NAME "ast/dse-typecov"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/dse-typecov.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/dse-typecov")
            set_tests_properties("ast/dse-typecov" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 120)

            add_test(NAME "ast/cse-typecov"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/cse-typecov.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/cse-typecov")
            set_tests_properties("ast/cse-typecov" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 120)

            add_test(NAME "ast/licm-typecov"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/licm-typecov.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/licm-typecov")
            set_tests_properties("ast/licm-typecov" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 120)

            add_test(NAME "ast/vecpacked-typecov"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/vecpacked-typecov.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/vecpacked-typecov")
            set_tests_properties("ast/vecpacked-typecov" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 120 SKIP_RETURN_CODE 77)

            add_test(NAME "ast/pre-typecov"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/pre-typecov.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_CURRENT_BINARY_DIR}/pre-typecov")
            set_tests_properties("ast/pre-typecov" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 120)
        else()
            if(MCC_TARGETOS STREQUAL "Darwin")
                set(_osxhp_why "osx/headers-parse's --min-files floor is measured by the Linux-host mcc parsing runtime/osx/include; a native Darwin mcc reads different type sizes and compiles a smaller corpus subset than that host, so the floor is host-specific and the cell is scoped to the host that banks it")
            else()
                set(_osxhp_why "osx/headers-parse needs a native, non-cross, non-emulated UNIX host mcc to parse the runtime/osx header set against its banked --min-files floor")
            endif()
            mcc_skip_test("osx/headers-parse" "${_osxhp_why}")
            mcc_skip_test("osx/headers-parse-known-positive" "${_osxhp_why}")
        endif()

        if(UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR
           AND NOT MCC_TARGETOS STREQUAL "WIN32")
            if(MCC_ENABLE_CROSS)
                set(_o0ab_min 7)
            else()
                set(_o0ab_min 1)
            endif()
            add_test(NAME "ast/o0-baseline"
                COMMAND "${CMAKE_COMMAND}" -E env
                        "C2_NO_EXTRA=1" "O0_AB_CHECK=1"
                        "O0_AB_MIN_KEYS=${_o0ab_min}"
                        sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/o0_ab.sh"
                        "${CMAKE_BINARY_DIR}" measurable
                        "${CMAKE_BINARY_DIR}/o0ab")
            set_tests_properties("ast/o0-baseline" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 1800)

            add_test(NAME "ast/o0-baseline-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/tools/o0_ab.sh
                        -DBUILD=${CMAKE_BINARY_DIR}
                        -DOUTDIR=${CMAKE_BINARY_DIR}/o0ab-kp
                        -DMINKEYS=${_o0ab_min}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/o0_ab_mutate.cmake")
            set_tests_properties("ast/o0-baseline-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 3600)

            add_test(NAME "ast/o0-baseline-gated"
                COMMAND "${CMAKE_COMMAND}" -E env
                        "C2_NO_EXTRA=1" "O0_AB_CHECK=1" "O0_AB_GATES=1" "MCC_DEV=1"
                        "O0_AB_MIN_KEYS=${_o0ab_min}"
                        sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/o0_ab.sh"
                        "${CMAKE_BINARY_DIR}" measurable
                        "${CMAKE_BINARY_DIR}/o0ab-gated")
            set_tests_properties("ast/o0-baseline-gated" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 1800)

            add_test(NAME "ast/o0-baseline-gated-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/tools/o0_ab.sh
                        -DBUILD=${CMAKE_BINARY_DIR}
                        -DOUTDIR=${CMAKE_BINARY_DIR}/o0ab-gated-kp
                        -DMINKEYS=${_o0ab_min}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/o0_ab_gates_mutate.cmake")
            set_tests_properties("ast/o0-baseline-gated-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 3600)
        else()
            set(_o0ab_why "the -O0 object bank and the fragment check need a native UNIX host that is neither cross-compiling nor emulated")
            mcc_skip_test("ast/o0-baseline" "${_o0ab_why}")
            mcc_skip_test("ast/o0-baseline-known-positive" "${_o0ab_why}")
            mcc_skip_test("ast/o0-baseline-gated" "${_o0ab_why}")
            mcc_skip_test("ast/o0-baseline-gated-known-positive" "${_o0ab_why}")
        endif()

        if(UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR
           AND NOT MCC_TARGETOS STREQUAL "WIN32")
            set(_w256_gmp_args "")
            if(APPLE)
                find_path(MCC_GMP_INC gmp.h
                    PATHS /opt/homebrew/include /usr/local/include
                          /opt/homebrew/opt/gmp/include /usr/local/opt/gmp/include)
                find_library(MCC_GMP_LIB gmp
                    PATHS /opt/homebrew/lib /usr/local/lib
                          /opt/homebrew/opt/gmp/lib /usr/local/opt/gmp/lib)
                if(MCC_GMP_INC AND MCC_GMP_LIB)
                    get_filename_component(_gmp_libdir "${MCC_GMP_LIB}" DIRECTORY)
                    set(_w256_gmp_args -DGMP_INC=${MCC_GMP_INC} -DGMP_LIBDIR=${_gmp_libdir})
                    message(STATUS "mcc: gmp = ${MCC_GMP_LIB} (${MCC_GMP_INC})")
                endif()
            endif()

            add_test(NAME "build/fragments-are-not-tus"
                COMMAND "${CMAKE_COMMAND}"
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_BINARY_DIR}
                        "-DPINNED=mccasm.c;mccast.c;mccgen.c;mccircap.c;mccpp.c;mccrir.c"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/frag_not_tu.cmake")
            set_tests_properties("build/fragments-are-not-tus" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 900 SKIP_RETURN_CODE 77)

            add_test(NAME "build/fragments-are-not-tus-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/frag_not_tu.cmake
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_BINARY_DIR}
                        "-DPINNED=mccasm.c;mccast.c;mccgen.c;mccircap.c;mccpp.c;mccrir.c"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/frag_not_tu_mutate.cmake")
            set_tests_properties("build/fragments-are-not-tus-known-positive"
                PROPERTIES FIXTURES_REQUIRED MCC_BUILT TIMEOUT 900
                SKIP_RETURN_CODE 77)

            add_test(NAME "wide256/gmp-diff"
                COMMAND "${CMAKE_COMMAND}"
                        -DMCC=${_mcc_exe}
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_BINARY_DIR}
                        -DHOSTCC=${CMAKE_C_COMPILER}
                        ${_w256_gmp_args}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/wide256_diff.cmake")
            set_tests_properties("wide256/gmp-diff" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 900 SKIP_RETURN_CODE 77)

            add_test(NAME "wide256/gmp-diff-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/wide256_diff.cmake
                        -DMCC=${_mcc_exe}
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_BINARY_DIR}
                        -DHOSTCC=${CMAKE_C_COMPILER}
                        ${_w256_gmp_args}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/wide256_diff_mutate.cmake")
            set_tests_properties("wide256/gmp-diff-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 1800 SKIP_RETURN_CODE 77)
        else()
            set(_w256_why "the GMP differential and the fragment check both need a native UNIX host: the oracle is libgmp built by the host C compiler, and the fragment check reads the build's own flags back out of compile_commands.json")
            mcc_skip_test("wide256/gmp-diff" "${_w256_why}")
            mcc_skip_test("wide256/gmp-diff-known-positive" "${_w256_why}")
            mcc_skip_test("build/fragments-are-not-tus" "${_w256_why}")
            mcc_skip_test("build/fragments-are-not-tus-known-positive" "${_w256_why}")
        endif()

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
                add_test(NAME "ast/replay-ld_value" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/ld_value.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                    "-DEXPECT_RC=42" "-DREPLAYED=main"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-ld_value" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            endif()

            add_test(NAME "ast/replay-struct_return" COMMAND "${CMAKE_COMMAND}"
                "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/struct_return.c"
                "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                "-DEXPECT_RC=42" "-DREPLAYED=make"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
            set_tests_properties("ast/replay-struct_return" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            # T-lin-10018: the for-condition store `for (; (p=*pp); )` must be
            # emitted, not left as a value-only StoreVal, or the replay producer
            # ships a body that dereferences an uninitialised p. Reduced
            # reproducer, run through the replay producer with the fallback off.
            add_test(NAME "rir/loopcond-store"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/rir/run-loopcond-store.sh"
                        "$<TARGET_FILE:mcc>" "${CMAKE_BINARY_DIR}")
            set_tests_properties("rir/loopcond-store" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 120)

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

            if(MCC_CPU STREQUAL "x86_64" OR MCC_CPU STREQUAL "arm64")
                add_test(NAME "ast/replay-promote" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/promote.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay"
                    "-DEXPECT_RC=42" "-DREPLAYED=loopy" "-DPROMOTE=1" "-DPROMOTES=loopy,callful,sumptr,fdot"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-promote" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
                add_test(NAME "ast/replay-promote-color" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/promote.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-replay-color"
                    "-DEXPECT_RC=42" "-DREPLAYED=loopy" "-DPROMOTE=1" "-DCOLOR=1" "-DPROMOTES=loopy,callful,sumptr,fdot"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay.cmake")
                set_tests_properties("ast/replay-promote-color" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test("ast/replay-promote"
                    "Tier-3 register promotion is wired for x86_64 + arm64; on ${MCC_CPU} it is a no-op — coverage stays via exec-replay-promote")
            endif()

            if(MCC_CPU STREQUAL "x86_64" OR MCC_CPU STREQUAL "arm64")
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

                add_test(NAME "ast/inline-frame" COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=$<TARGET_FILE:mcc>" "-DBDIR=${CMAKE_BINARY_DIR}"
                    "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/replay/inline.c"
                    "-DOUT=${CMAKE_BINARY_DIR}/ast-inline-frame" "-DEXPECT_RC=42"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/frame_regress.cmake")
                set_tests_properties("ast/inline-frame" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            elseif(MCC_CPU STREQUAL "riscv64")
                mcc_skip_test("ast/replay-inline"
                    "Tier-4 graft-inline is qemu-validated on riscv64 (ctest riscv64-inline-docker: scalar/pointer/double/8-byte-reg-pair-struct AND the >16B indirect-ABI struct sumbig all graft, generated code runs RC=42, differential 42429/0 vs gcc -O2 — the indirect-param capture now models the riscv64 hidden-pointer class); the MCC_CPU-gated inline.c fixture stays skipped here because no riscv64-host CI exercises it and inline.c's main() replay in that exact fixture context is not yet arch-verified for riscv64")
                mcc_skip_test("ast/replay-inline-spec"
                    "per-site specialization grafts+specializes choose/clampk/mul/addk on riscv64 and main replays there (RC=42, qemu-validated), so this fixture's assertions do hold; it stays skipped only because no riscv64-host CI exercises the MCC_CPU-gated build path -- live coverage is via ctest riscv64-inline-docker")
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

    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(csttool tools/csttool.c)
        target_include_directories(csttool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
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

    # tests/smoke: the fast inner-loop goldens suite and the engine-bail ratchet.
    # One table-driven subject compiled once per -O level, from -O0 up to and
    # including the highest rung an ungated knob is reached by -- -O4, since the
    # consolidation. smokerun derives that bound by walking MCC_OPT_LIST in
    # src/mccopt.h, so the ladder cannot drift from the compiler. -O5..-O12 are
    # a hard error and -O13 is the search rung, which is a different subject and
    # not a value-coverage tier; dropping it dropped its bail ratchet rows too,
    # see tests/smoke/bails.txt and docs/TODO.md.
    #
    # The floor below is a COVERAGE floor, not a runtime one: the retarget from
    # 0..13 to 0..4 cut the case count from 6,308,092 to 2,759,779, and the
    # float, _Complex, long double and _Float16 tables were deepened until the
    # total came back over the old number. Fewer levels must not mean fewer
    # cases. Adding a value case means one line in tests/smoke/cases.h or
    # tests/smoke/fcases.h and never a new cell. The second float deepening
    # (per-type IEEE boundary sweep, conversion sweep, three _Complex widths)
    # took it 6,772,291 -> 12,875,605.
    if(NOT CMAKE_CROSSCOMPILING)
        add_test(NAME "bitint/scast-signext"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/bitint/scast_signext.c")
        set_tests_properties("bitint/scast-signext" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "bitint-scast checks=80 fails=0")
        add_test(NAME "bitint/scast-signext-known-positive"
            COMMAND ${_mcc_exe} -DMCC_SCAST_MUTATE -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/bitint/scast_signext.c")
        set_tests_properties("bitint/scast-signext-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        add_test(NAME "fold/int64-float"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fold/int64_float.c")
        set_tests_properties("fold/int64-float" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "i2f checks=9 fails=0")
        add_test(NAME "fold/int64-float-known-positive"
            COMMAND ${_mcc_exe} -DMCC_I2F_MUTATE -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fold/int64_float.c")
        set_tests_properties("fold/int64-float-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        add_test(NAME "switch/jumptable-exhaustive"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/switch/jumptable_exhaustive.c")
        set_tests_properties("switch/jumptable-exhaustive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT ENVIRONMENT "MCC_SWITCH_JUMPTABLE=1"
            PASS_REGULAR_EXPRESSION "jt-exhaustive checks=11 fails=0")
        add_test(NAME "switch/jumptable-falloff-known-positive"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/switch/jumptable_falloff.c"
                    -o "${CMAKE_BINARY_DIR}/jtf.o")
        set_tests_properties("switch/jumptable-falloff-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT ENVIRONMENT "MCC_SWITCH_JUMPTABLE=1"
            WILL_FAIL TRUE)

        add_test(NAME "pp/if-64bit"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cond/if64.c")
        set_tests_properties("pp/if-64bit" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "if64 checks=7 ok=5")

        add_test(NAME "pp/has-embed-ifdef"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cond/has_embed_ifdef.c")
        set_tests_properties("pp/has-embed-ifdef" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "has_embed_ifdef a=1 b=1 c=1 d=1")

        add_test(NAME "builtins/clz-ctz-zero"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/builtins/clz_ctz_zero.c")
        set_tests_properties("builtins/clz-ctz-zero" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "clzctz0 clz=32 clzll=64 ctz=32 ctzll=64 fails=0")

        add_test(NAME "diag/implicit-int-var"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/implicit_int_var.c"
                    -o "${CMAKE_BINARY_DIR}/iiv.o")
        set_tests_properties("diag/implicit-int-var" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "diag/implicit-int-var-c89"
            COMMAND ${_mcc_exe} -std=c89 -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/implicit_int_var.c"
                    -o "${CMAKE_BINARY_DIR}/iiv89.o")
        set_tests_properties("diag/implicit-int-var-c89" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "diag/excess-string-init-werror"
            COMMAND ${_mcc_exe} -Werror=excess-initializers -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/excess_string_init.c"
                    -o "${CMAKE_BINARY_DIR}/esi.o")
        set_tests_properties("diag/excess-string-init-werror" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "diag/excess-string-init-nowarn"
            COMMAND ${_mcc_exe} -Wno-excess-initializers -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/excess_string_init.c"
                    -o "${CMAKE_BINARY_DIR}/esin.o")
        set_tests_properties("diag/excess-string-init-nowarn" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME "builtins/expect-assume"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/builtins/expect_assume.c")
        set_tests_properties("builtins/expect-assume" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "expect_assume fails=0")
        add_test(NAME "builtins/assume-typecheck"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/builtins/assume_typecheck.c"
                    -o "${CMAKE_BINARY_DIR}/at.o")
        set_tests_properties("builtins/assume-typecheck" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "diag/typeof-bitfield"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/typeof_bitfield.c"
                    -o "${CMAKE_BINARY_DIR}/tbf.o")
        set_tests_properties("diag/typeof-bitfield" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        add_test(NAME "diag/static-assert-wide"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/static_assert_wide.c"
                    -o "${CMAKE_BINARY_DIR}/saw.o")
        set_tests_properties("diag/static-assert-wide" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "diag/static-assert-false"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/static_assert_false.c"
                    -o "${CMAKE_BINARY_DIR}/saf.o")
        set_tests_properties("diag/static-assert-false" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        add_test(NAME "attr/weakref-defined"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/attr/weakref_defined.c")
        set_tests_properties("attr/weakref-defined" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "weakref=42")

        add_test(NAME "ext/extension-declspec"
            COMMAND ${_mcc_exe} -std=c89 -pedantic-errors -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/ext/extension_declspec.c"
                    -o "${CMAKE_BINARY_DIR}/extd.o")
        set_tests_properties("ext/extension-declspec" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "ext/extension-noleak"
            COMMAND ${_mcc_exe} -std=c89 -pedantic-errors -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/ext/extension_noleak.c"
                    -o "${CMAKE_BINARY_DIR}/extn.o")
        set_tests_properties("ext/extension-noleak" PROPERTIES FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        add_test(NAME "pack/pop-unmatched"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/pack/pop_unmatched.c"
                    -o "${CMAKE_BINARY_DIR}/pu.o")
        set_tests_properties("pack/pop-unmatched" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        add_test(NAME "diag/generic-enum-tag"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/generic_enum_tag.c")
        set_tests_properties("diag/generic-enum-tag" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "generic_enum_tag fails=0")

        add_test(NAME "diag/tls-addr-static-init"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/tls_addr_static_init.c"
                    -o "${CMAKE_BINARY_DIR}/tlsaddr.o")
        set_tests_properties("diag/tls-addr-static-init" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        # T-mac-30103: shadowed locals must land in nested DW_TAG_lexical_block
        # DIEs (inner block inside outer), not one level too shallow. Needs
        # dwarfdump to read the DIE tree; skips (77) where it is absent.
        add_test(NAME "debug/lexblock-nest"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/debug/lexblock-nest.sh"
                    ${_mcc_exe}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/debug/lexblock_nest.c"
                    "${CMAKE_BINARY_DIR}/lexblock-nest.o")
        set_tests_properties("debug/lexblock-nest" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
            PASS_REGULAR_EXPRESSION "nested")

        # T-mac-30122(1): a discarded volatile lvalue read must still emit the
        # load (read-to-clear MMIO); a non-volatile discard must not (bite).
        add_test(NAME "volatile/discard-read"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/volatile/discard-read.sh"
                    ${_mcc_exe}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/volatile/discard_read.c")
        set_tests_properties("volatile/discard-read" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "fix is volatile-specific")

        # T-mac-30151: initializing a FAM of an automatic-storage object is a
        # hard error (both oracles reject); static/file-scope FAM init stays a
        # GNU extension (known-positive).
        add_test(NAME "fam/auto-init-error"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fam/auto_init.c"
                    -o "${CMAKE_BINARY_DIR}/fam-auto.o")
        set_tests_properties("fam/auto-init-error" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "fam/static-init-ok"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fam/static_init.c")
        set_tests_properties("fam/static-init-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30150: typeof_unqual is C23-only, not a GNU extension — accepted
        # in C23, rejected as a keyword in a pre-C23 GNU dialect (the bite).
        add_test(NAME "typeof/unqual-c23"
            COMMAND ${_mcc_exe} -std=c23 -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/typeof/unqual_c23.c")
        set_tests_properties("typeof/unqual-c23" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "typeof/unqual-pre-c23-rejected"
            COMMAND ${_mcc_exe} -std=gnu17 -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/typeof/unqual_pre_c23.c"
                    -o "${CMAKE_BINARY_DIR}/tuq-pre.o")
        set_tests_properties("typeof/unqual-pre-c23-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        # T-mac-30143: 'stdcall' is meaningless off i386 — warn-and-ignore like
        # gcc/clang (pre-fix it was silent, so the warning match is the bite).
        add_test(NAME "attr/stdcall-ignored"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/attr/stdcall_ignored.c"
                    -o "${CMAKE_BINARY_DIR}/stdcall-ignored.o")
        set_tests_properties("attr/stdcall-ignored" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "'stdcall' attribute ignored")

        # T-mac-30146: a surrogate/out-of-range UCN is invalid in every mode and
        # for wide as well as narrow literals — reject it (was silently emitted
        # under C23); valid wide UCNs must still work (known-positive).
        add_test(NAME "lexical/ucn-surrogate-rejected"
            COMMAND ${_mcc_exe} -std=c23 -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexical/ucn_surrogate.c"
                    -o "${CMAKE_BINARY_DIR}/ucn-surr.o")
        set_tests_properties("lexical/ucn-surrogate-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "lexical/ucn-valid-wide"
            COMMAND ${_mcc_exe} -std=c23 -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexical/ucn_valid_wide.c")
        set_tests_properties("lexical/ucn-valid-wide" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30147: a 4-byte wide-string init must match the array element in
        # signedness (int[]=U"" / unsigned[]=L"" are cross-type, both oracles
        # error); correctly-signed inits still work (known-positive).
        add_test(NAME "lexical/widestr-signedness-rejected"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexical/widestr_signedness.c"
                    -o "${CMAKE_BINARY_DIR}/widestr-sign.o")
        set_tests_properties("lexical/widestr-signedness-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "lexical/widestr-valid"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexical/widestr_valid.c")
        set_tests_properties("lexical/widestr-valid" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30213: a radix prefix (0x/0b/0o) with zero following digits is
        # invalid (clang/gcc "invalid suffix"); mcc silently made it 0. Reject
        # it, and keep every well-formed radix literal (incl. hex floats).
        add_test(NAME "lexical/empty-radix-rejected"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexical/empty_radix.c"
                    -o "${CMAKE_BINARY_DIR}/empty-radix.o")
        set_tests_properties("lexical/empty-radix-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "lexical/empty-radix-valid"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/lexical/empty_radix_valid.c")
        set_tests_properties("lexical/empty-radix-valid" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30191: `return;` with no value in a non-void K&R `int f()` was
        # silently synthesized to `return 0`; gcc/clang error. Reject it (error
        # by default), and keep every well-formed return.
        add_test(NAME "diag/knr-return-no-value-rejected"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/knr_return_no_value.c"
                    -o "${CMAKE_BINARY_DIR}/knr-return.o")
        set_tests_properties("diag/knr-return-no-value-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "diag/knr-return-valid"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/knr_return_valid.c")
        set_tests_properties("diag/knr-return-valid" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30215: -Wswitch must warn on a case value not in the enum type;
        # a switch over only enumerators stays clean (even under -Werror).
        add_test(NAME "diag/wswitch-case-not-in-enum"
            COMMAND ${_mcc_exe} -Wswitch -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/wswitch_case_enum.c"
                    -o "${CMAKE_BINARY_DIR}/wswitch-case.o")
        set_tests_properties("diag/wswitch-case-not-in-enum" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "case value '99' not in enumerated type")
        add_test(NAME "diag/wswitch-case-valid"
            COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/wswitch_case_enum_valid.c")
        set_tests_properties("diag/wswitch-case-valid" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30138: a block-scope `static` initialized by a constant-valued
        # statement expression (`static int s = ({7;});`) was rejected outright;
        # gcc/clang fold it. Accept it when the stmt-expr's result is a constant
        # (integer or address); a non-constant stmt-expr still errors.
        add_test(NAME "diag/static-stmtexpr-const-valid"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/static_stmtexpr_const_valid.c")
        set_tests_properties("diag/static-stmtexpr-const-valid" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "diag/static-stmtexpr-nonconst-rejected"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/static_stmtexpr_nonconst.c"
                    -o "${CMAKE_BINARY_DIR}/static-stmtexpr-nc.o")
        set_tests_properties("diag/static-stmtexpr-nonconst-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "initializer element is not constant")

        # T-lin-10417: the documented-unsafe -fopt-search-threads (pool workers
        # race on ast_cur / ast_*_env) must not deadlock — it now runs the search
        # serially unless MCC_OPT_SEARCH_THREADS_UNSAFE is set. Compiling the
        # threaded atomic_counter under the flag must complete fast.
        add_test(NAME "opt/search-threads-no-deadlock"
            COMMAND ${_mcc_exe} -O13 -fopt-search -fopt-search-threads -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/features_c99_c11/atomic_counter.c"
                    -o "${CMAKE_BINARY_DIR}/search-threads-nodl.o")
        set_tests_properties("opt/search-threads-no-deadlock" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT TIMEOUT 60
            ENVIRONMENT "MCC_DEV=1;MCC_SEARCH_WORKER=1")

        # T-mac-30145: a `static`/`constexpr` compound literal must have a
        # constant initializer (like any static-storage object); mcc accepted a
        # non-constant one. The happy path (constant init) still compiles + runs.
        add_test(NAME "diag/static-constexpr-cl-valid"
            COMMAND ${_mcc_exe} -std=c23 -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/static_constexpr_cl_valid.c")
        set_tests_properties("diag/static-constexpr-cl-valid" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "diag/static-cl-nonconst-rejected"
            COMMAND ${_mcc_exe} -std=c23 -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/static_cl_nonconst.c"
                    -o "${CMAKE_BINARY_DIR}/static-cl-nc.o")
        set_tests_properties("diag/static-cl-nonconst-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "initializer element is not constant")
        add_test(NAME "diag/constexpr-cl-nonconst-rejected"
            COMMAND ${_mcc_exe} -std=c23 -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/constexpr_cl_nonconst.c"
                    -o "${CMAKE_BINARY_DIR}/constexpr-cl-nc.o")
        set_tests_properties("diag/constexpr-cl-nonconst-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "initializer element is not constant")

        # T-lin-10395: an unsuffixed decimal constant in [2^63, 2^64) has
        # long-long rank (unsigned long long), not unsigned long — the latter is
        # not even a candidate type for a decimal constant. Target-uniform.
        add_test(NAME "diag/decimal-ull-rank"
            COMMAND ${_mcc_exe} -w -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/decimal_ull_rank.c")
        set_tests_properties("diag/decimal-ull-rank" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30074: a lone \r (classic-Mac line ending) must terminate a line
        # so a #define / // comment after it is recognized; \r\n and \n are
        # unaffected. Generated at runtime (.gitattributes normalizes stored \r).
        add_test(NAME "diag/cr-line-ending"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cr_line_ending.sh"
                    "$<TARGET_FILE:mcc>")
        set_tests_properties("diag/cr-line-ending" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "OK")

        # T-mac-30167: _Alignof of a packed struct member is its effective
        # (reduced) alignment, not the natural type alignment — _Alignof(p.x)==1
        # for a packed p. A derived value (p.x+1, a ternary/comma result) falls
        # back to the type alignment, and sizeof is unaffected.
        add_test(NAME "diag/alignof-packed-member"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/alignof_packed_member.c")
        set_tests_properties("diag/alignof-packed-member" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30158: constructor/destructor priority must order .init_array /
        # .fini_array by ascending priority (unprioritized last), matching gcc,
        # rather than source order. Declaration order is scrambled vs priority so
        # the source-order behavior (2/1/D M z/x/y) fails this regex.
        add_test(NAME "diag/ctor-dtor-priority"
            COMMAND ${_mcc_exe} -w -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/ctor_dtor_priority.c")
        set_tests_properties("diag/ctor-dtor-priority" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "12DMzyx")

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_diag_ok.c")
            add_test(NAME "diag/pragma-diagnostic-suppress"
                COMMAND ${_mcc_exe} -Wsign-compare -Werror -run
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_diag_ok.c")
            set_tests_properties("diag/pragma-diagnostic-suppress" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME "diag/pragma-diagnostic-scope"
                COMMAND ${_mcc_exe} -Wsign-compare -Werror -run
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_diag_scope.c")
            set_tests_properties("diag/pragma-diagnostic-scope" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT
                PASS_REGULAR_EXPRESSION "different signedness")
        endif()

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cleanup_nonauto.c")
            add_test(NAME "diag/cleanup-nonauto-warn"
                COMMAND ${_mcc_exe} -Wall -Werror -run
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cleanup_nonauto.c")
            set_tests_properties("diag/cleanup-nonauto-warn" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT
                PASS_REGULAR_EXPRESSION "'cleanup' attribute ignored")
            add_test(NAME "diag/cleanup-auto-ok"
                COMMAND ${_mcc_exe} -Wall -run
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cleanup_auto_ok.c")
            set_tests_properties("diag/cleanup-auto-ok" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
        endif()

        # T-mac-30104: strlen/strcmp/strncmp/memcmp of string literals must
        # const-fold in ICE contexts (global + local array dims, _Static_assert,
        # case labels, static const). The program returns 0 iff every folded
        # value is correct; it fails to compile if the fold is absent.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/strmem_ice_fold.c")
            add_test(NAME "diag/strmem-ice-fold"
                COMMAND ${_mcc_exe} -run
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/strmem_ice_fold.c")
            set_tests_properties("diag/strmem-ice-fold" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
        endif()

        # T-mac-30219: a command-line / builtin macro redefinition must warn like
        # gcc/clang (it was silently applied); a plain compile must stay quiet
        # (the built-in predefs, which self-redefine, must not leak warnings).
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cmdline_redef_probe.c")
            add_test(NAME "diag/cmdline-macro-redef"
                COMMAND ${_mcc_exe} -DX=1 -DX=2 -c
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cmdline_redef_probe.c"
                        -o "${CMAKE_BINARY_DIR}/cmdline-redef.o")
            set_tests_properties("diag/cmdline-macro-redef" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT
                PASS_REGULAR_EXPRESSION "X redefined")
            add_test(NAME "diag/cmdline-predef-redef"
                COMMAND ${_mcc_exe} -D__STDC__=0 -c
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cmdline_redef_probe.c"
                        -o "${CMAKE_BINARY_DIR}/cmdline-predef.o")
            set_tests_properties("diag/cmdline-predef-redef" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT
                PASS_REGULAR_EXPRESSION "__STDC__ redefined")
            add_test(NAME "diag/cmdline-no-spurious-redef"
                COMMAND ${_mcc_exe} -c
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cmdline_redef_probe.c"
                        -o "${CMAKE_BINARY_DIR}/cmdline-clean.o")
            set_tests_properties("diag/cmdline-no-spurious-redef" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT
                FAIL_REGULAR_EXPRESSION "redefined")
        endif()

        add_test(NAME "cli/no-args-error"
            COMMAND ${_mcc_exe})
        set_tests_properties("cli/no-args-error" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "no input files")

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_region.c")
            add_test(NAME "diag/pragma-region"
                COMMAND ${_mcc_exe} -Wall -Werror -run
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_region.c")
            set_tests_properties("diag/pragma-region" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)
        endif()

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cleanup_argcheck_err.c")
            add_test(NAME "diag/cleanup-argcheck-err"
                COMMAND ${_mcc_exe} -run
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/cleanup_argcheck_err.c")
            set_tests_properties("diag/cleanup-argcheck-err" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT
                PASS_REGULAR_EXPRESSION "'cleanup' argument has incompatible pointer type")
        endif()

        # T-mac-30184: a _Noreturn function that can fall off the end must warn
        # "'noreturn' function does return"; one ending in an unconditional
        # noreturn call stays clean (even under -Werror).
        add_test(NAME "diag/noreturn-falloff" COMMAND ${_mcc_exe} -Wall -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/noreturn_falloff.c"
                    -o "${CMAKE_BINARY_DIR}/noreturn-falloff.o")
        set_tests_properties("diag/noreturn-falloff" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "'noreturn' function does return")
        add_test(NAME "diag/noreturn-ok" COMMAND ${_mcc_exe} -Wall -Werror -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/noreturn_ok.c"
                    -o "${CMAKE_BINARY_DIR}/noreturn-ok.o")
        set_tests_properties("diag/noreturn-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30193: -Wpointer-sign — integer pointers differing only in target
        # signedness warn under -Wall; same-sign / explicit-cast stay clean.
        add_test(NAME "diag/pointer-sign" COMMAND ${_mcc_exe} -Wall -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pointer_sign.c"
                    -o "${CMAKE_BINARY_DIR}/pointer-sign.o")
        set_tests_properties("diag/pointer-sign" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "differ in signedness")
        add_test(NAME "diag/pointer-sign-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pointer_sign_ok.c")
        set_tests_properties("diag/pointer-sign-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30166: __builtin_*_overflow with a non-integer (float*) result
        # must be rejected; integer results still work.
        add_test(NAME "diag/overflow-float-rejected" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/overflow_float_result.c"
                    -o "${CMAKE_BINARY_DIR}/overflow-float.o")
        set_tests_properties("diag/overflow-float-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "diag/overflow-int-ok" COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/overflow_int_ok.c")
        set_tests_properties("diag/overflow-int-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30186: too many braces around a scalar initializer is diagnosed
        # at the default level (was -Wall-gated); single-brace stays clean.
        add_test(NAME "diag/scalar-extra-braces" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/scalar_extra_braces.c"
                    -o "${CMAKE_BINARY_DIR}/scalar-extra-braces.o")
        set_tests_properties("diag/scalar-extra-braces" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "too many braces around scalar initializer")
        add_test(NAME "diag/scalar-braces-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/scalar_braces_ok.c")
        set_tests_properties("diag/scalar-braces-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30160: #pragma GCC poison bans an identifier; using it errors.
        add_test(NAME "pp/pragma-poison-rejected" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_poison.c"
                    -o "${CMAKE_BINARY_DIR}/pragma-poison.o")
        set_tests_properties("pp/pragma-poison-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "pp/pragma-poison-ok" COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_poison_ok.c")
        set_tests_properties("pp/pragma-poison-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30161: #pragma GCC error stops the compile; #pragma GCC warning
        # emits the message.
        add_test(NAME "pp/pragma-gcc-error" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_gcc_error.c"
                    -o "${CMAKE_BINARY_DIR}/pragma-gcc-error.o")
        set_tests_properties("pp/pragma-gcc-error" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "pp/pragma-gcc-warning" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_gcc_warning.c"
                    -o "${CMAKE_BINARY_DIR}/pragma-gcc-warning.o")
        set_tests_properties("pp/pragma-gcc-warning" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "diagnostic message here")

        # T-mac-30220: #warning is under the -Wcpp category -- -Wno-cpp
        # suppresses it, -Werror=cpp promotes it to an error.
        add_test(NAME "pp/warning-cpp-suppressed" COMMAND ${_mcc_exe} -Wno-cpp -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/warning_cpp_suppressed.c")
        set_tests_properties("pp/warning-cpp-suppressed" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "pp/warning-cpp-error" COMMAND ${_mcc_exe} -Werror=cpp -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/warning_cpp_error.c"
                    -o "${CMAKE_BINARY_DIR}/warning-cpp-error.o")
        set_tests_properties("pp/warning-cpp-error" PROPERTIES FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        # T-mac-30240: __fp16 (ARM storage-only half type) is a recognized
        # keyword aliased to the _Float16 backend -- 2-byte storage, arithmetic
        # promotes to float, overflow saturates to inf.
        add_test(NAME "type/fp16" COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/fp16_type.c")
        set_tests_properties("type/fp16" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30221: -pg is warn-and-ignore (no profiling codegen), consistent
        # with -finstrument-functions -- it must not hard-error the build.
        add_test(NAME "driver/pg-warn-and-ignore" COMMAND ${_mcc_exe} -pg -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pg_flag.c")
        set_tests_properties("driver/pg-warn-and-ignore" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30239: a function declared with a variably-modified return type
        # (here pointer-to-VLA, int (*fp(void))[n]) is rejected per C11 6.7.6.3.
        add_test(NAME "type/vm-return-rejected" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/vm_return_bad.c"
                    -o "${CMAKE_BINARY_DIR}/vm-return-bad.o")
        set_tests_properties("type/vm-return-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        # T-mac-30247 (sub-items 4/5): a constexpr object may not have a
        # volatile-qualified or _Atomic type (C23 6.7.1); both are rejected.
        add_test(NAME "type/constexpr-volatile-rejected" COMMAND ${_mcc_exe} -std=c23 -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/constexpr_volatile.c"
                    -o "${CMAKE_BINARY_DIR}/constexpr-volatile.o")
        set_tests_properties("type/constexpr-volatile-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "type/constexpr-atomic-rejected" COMMAND ${_mcc_exe} -std=c23 -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/constexpr_atomic.c"
                    -o "${CMAKE_BINARY_DIR}/constexpr-atomic.o")
        set_tests_properties("type/constexpr-atomic-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        # T-mac-30235: under -E, the _Pragma operator form of push_macro/pop_macro
        # must be APPLIED (not only emitted) so a later pop restores the macro --
        # a subsequent `V` must expand to the pushed 2, not the intervening 9.
        add_test(NAME "pp/pragma-op-pushpop-E" COMMAND ${_mcc_exe} -E
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pragma_op_pushpop.c")
        set_tests_properties("pp/pragma-op-pushpop-E" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT PASS_REGULAR_EXPRESSION "int a = 2")

        # T-mac-30200: under -E, adjacent tokens produced by macro expansion must
        # be separated by a space when they would otherwise re-lex as a longer
        # pp-token -- worst case '/' + '*' forming a block-comment introducer.
        # The output must contain the spaced form "/ *", never the comment "/*".
        add_test(NAME "pp/paste-avoid-E" COMMAND ${_mcc_exe} -E
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pp_paste_avoid.c")
        set_tests_properties("pp/paste-avoid-E" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "1 / [*]b [+] c"
            FAIL_REGULAR_EXPRESSION "/[*]")

        # T-mac-30205: two '#' tokens from separate arg expansions must not merge
        # into '##' and be lost under -E -- both must survive as "# #".
        add_test(NAME "pp/hash-hash-E" COMMAND ${_mcc_exe} -E
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/pp_hash_hash.c")
        set_tests_properties("pp/hash-hash-E" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "# #")

        # T-mac-30202: -Wtype-limits (a -Wextra sub-warning, off by default like
        # gcc) flags a comparison whose result is constant because an unsigned
        # operand is compared against 0.
        add_test(NAME "diag/type-limits-warns" COMMAND ${_mcc_exe} -Wtype-limits -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/type_limits.c"
                    -o "${CMAKE_BINARY_DIR}/type-limits.o")
        set_tests_properties("diag/type-limits-warns" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "comparison of unsigned expression is always (false|true)")
        # Off by default: -Werror alone must NOT escalate (anti-vacuity below).
        add_test(NAME "diag/type-limits-default-off" COMMAND ${_mcc_exe} -Werror -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/type_limits.c"
                    -o "${CMAKE_BINARY_DIR}/type-limits-off.o")
        set_tests_properties("diag/type-limits-default-off" PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        # Anti-vacuity: -Werror=type-limits must turn the warning into an error.
        add_test(NAME "diag/type-limits-error" COMMAND ${_mcc_exe} -Werror=type-limits -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/type_limits.c"
                    -o "${CMAKE_BINARY_DIR}/type-limits-err.o")
        set_tests_properties("diag/type-limits-error" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)

        # T-mac-30130 slice-1: -Wunused-label for an ordinary defined-but-
        # unreferenced label; goto'd / address-taken labels stay clean.
        add_test(NAME "diag/unused-label" COMMAND ${_mcc_exe} -Wall -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/unused_label.c"
                    -o "${CMAKE_BINARY_DIR}/unused-label.o")
        set_tests_properties("diag/unused-label" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "label 'dead' defined but not used")
        add_test(NAME "diag/used-label-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/used_label_ok.c")
        set_tests_properties("diag/used-label-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30130 slice-2: duplicate __label__ in one block is an error;
        # distinct names and nested-block shadowing stay valid.
        add_test(NAME "diag/dup-label-rejected" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/dup_label_decl.c"
                    -o "${CMAKE_BINARY_DIR}/dup-label.o")
        set_tests_properties("diag/dup-label-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
        add_test(NAME "diag/dup-label-ok" COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/dup_label_ok.c")
        set_tests_properties("diag/dup-label-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30188: a tag first declared in a prototype parameter list warns
        # under -Wall; pre-declared/anonymous/ordinary tags stay clean.
        add_test(NAME "diag/param-tag-visibility" COMMAND ${_mcc_exe} -Wall -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/param_tag_visibility.c"
                    -o "${CMAKE_BINARY_DIR}/param-tag.o")
        set_tests_properties("diag/param-tag-visibility" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "declared inside parameter list will not be visible")
        add_test(NAME "diag/param-tag-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/param_tag_ok.c")
        set_tests_properties("diag/param-tag-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30187: `extern int x = 5;` warns (extern + initializer); plain
        # extern / ordinary / static definitions stay clean.
        add_test(NAME "diag/extern-initializer" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/extern_initializer.c"
                    -o "${CMAKE_BINARY_DIR}/extern-init.o")
        set_tests_properties("diag/extern-initializer" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "initialized and declared 'extern'")
        add_test(NAME "diag/extern-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/extern_ok.c")
        set_tests_properties("diag/extern-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30185 slice: a deprecated struct/union/enum tag warns on use;
        # non-deprecated tags stay clean.
        add_test(NAME "diag/deprecated-tag" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_tag.c"
                    -o "${CMAKE_BINARY_DIR}/deprecated-tag.o")
        set_tests_properties("diag/deprecated-tag" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "'S' is deprecated")
        add_test(NAME "diag/deprecated-tag-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_tag_ok.c")
        set_tests_properties("diag/deprecated-tag-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30185 residual: a deprecated individual enumerator warns on use
        # (both __attribute__((deprecated)) and the C23 [[deprecated]] spelling);
        # a struct/union/enum tag carrying the trailing-attribute form warns on
        # use too. Declaring the deprecated entity without using it stays clean.
        add_test(NAME "diag/deprecated-enumerator" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_enumerator.c"
                    -o "${CMAKE_BINARY_DIR}/deprecated-enumerator.o")
        set_tests_properties("diag/deprecated-enumerator" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "'A' is deprecated")
        add_test(NAME "diag/deprecated-enumerator-c23" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_enumerator_c23.c"
                    -o "${CMAKE_BINARY_DIR}/deprecated-enumerator-c23.o")
        set_tests_properties("diag/deprecated-enumerator-c23" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "'C' is deprecated")
        add_test(NAME "diag/deprecated-enumerator-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_enumerator_ok.c")
        set_tests_properties("diag/deprecated-enumerator-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME "diag/deprecated-trailing-attr" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_trailing_attr.c"
                    -o "${CMAKE_BINARY_DIR}/deprecated-trailing-attr.o")
        set_tests_properties("diag/deprecated-trailing-attr" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "'Q' is deprecated")
        add_test(NAME "diag/deprecated-trailing-attr-ok" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_trailing_attr_ok.c")
        set_tests_properties("diag/deprecated-trailing-attr-ok" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
        # T-mac-30185 residual: forming a typedef from a deprecated struct/union/enum
        # tag warns at the formation point (gcc-faithful); the deprecated-tag-ok cell
        # proves the non-deprecated path stays clean.
        add_test(NAME "diag/deprecated-typedef" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/deprecated_typedef.c"
                    -o "${CMAKE_BINARY_DIR}/deprecated-typedef.o")
        set_tests_properties("diag/deprecated-typedef" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "'S' is deprecated")

        # T-mac-30153: __FILE_NAME__ (basename) and __TIMESTAMP__ (mtime) must be
        # predefined like gcc/clang; pre-fix both were undefined identifiers.
        add_test(NAME "pp/loc-macros"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/pp/loc_macros.c")
        set_tests_properties("pp/loc-macros" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30156: <stdckdint.h> must work on every target (no __int128 dep);
        # pre-fix the #include itself failed with '__int128 not supported'.
        add_test(NAME "ckdint/ckd-basic"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/ckdint/ckd_basic.c")
        set_tests_properties("ckdint/ckd-basic" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30170/30171: on macOS __builtin_mempcpy must link and
        # __builtin_bzero(p,n) must zero n bytes (pre-fix: link error / 1 byte).
        add_test(NAME "builtins/macos-mem"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/builtins/macos_mem.c")
        set_tests_properties("builtins/macos-mem" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30196: under -run, rodata must be read-only (write faults) while
        # .data/.bss stay writable — matching AOT (Darwin-gated).
        add_test(NAME "jit/run-rodata-ro"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-rodata-ro.sh"
                    ${_mcc_exe} "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties("jit/run-rodata-ro" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
            PASS_REGULAR_EXPRESSION "const write faults under -run")

        add_test(NAME "jit/blind-retype-safe"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmarks/retype_superopt/jit_blind.sh")
        set_tests_properties("jit/blind-retype-safe" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
            ENVIRONMENT "MCC=${_mcc_exe};MB=${CMAKE_CURRENT_BINARY_DIR}"
            PASS_REGULAR_EXPRESSION "jit_blind: PASS"
            FAIL_REGULAR_EXPRESSION "MISMATCH")

        # T-mac-30192: _Alignof(void) is a GNU void-size extension (value 1),
        # not a hard error — parity with sizeof(void).
        add_test(NAME "sizeof/alignof-void"
            COMMAND ${_mcc_exe} -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/sizeof/alignof_void.c")
        set_tests_properties("sizeof/alignof-void" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30182: unused/maybe_unused suppress -Wunused for the annotated
        # entity; un-annotated ones must still warn (the bite).
        add_test(NAME "attr/unused-suppress"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/attr/unused-suppress.sh"
                    ${_mcc_exe}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/attr/unused_suppress.c"
                    "${CMAKE_BINARY_DIR}/unused-suppress.o")
        set_tests_properties("attr/unused-suppress" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "suppression is specific")
        add_test(NAME "diag/global-addr-static-init"
            COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/diag/global_addr_static_init.c"
                    -o "${CMAKE_BINARY_DIR}/gaddr.o")
        set_tests_properties("diag/global-addr-static-init" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)

        if(UNIX)
            add_test(NAME "inline/used-static-inline"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/inline/used-emit.sh"
                        "$<TARGET_FILE:mcc>"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/inline/used_static_inline.c"
                        "${CMAKE_BINARY_DIR}/usi.o")
            set_tests_properties("inline/used-static-inline" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
        endif()

        add_executable(smokerun tools/smokerun.c tools/toolsupport.c)
        target_include_directories(smokerun PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")

        # The strategy floor is target-keyed because the subject's shapes are
        # data-model-dependent, not because the strategies are: bfold and
        # narrow fire on 64-bit `long` shapes that do not exist under LLP64
        # (both fire on Windows given `long long` probes -- T-win-50012), and
        # tree-sra/tree-sroa are MCC_OPTD_OFF at every level on every host, so
        # the LP64 panel fires exactly 22 of 24 and the LLP64 panel exactly 20.
        # A data-model-neutral corpus deepening (long-long narrowing + unsigned
        # tautology rows) would let this return to one number, at the cost of a
        # three-platform bails re-bank; recorded in T-win-50012's anchor.
        if(WIN32)
            set(_smoke_min_strats 20)
        else()
            set(_smoke_min_strats 22)
        endif()

        add_test(NAME "smoke/native"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-native"
                    --min-cases 12800000 --min-passes 150
                    --min-strats ${_smoke_min_strats})
        set_tests_properties("smoke/native" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

        add_test(NAME "smoke/strats-known-positive"
            COMMAND "${CMAKE_COMMAND}"
                -DSMOKERUN=$<TARGET_FILE:smokerun>
                -DMCC=${_mcc_exe}
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DWORK=${CMAKE_BINARY_DIR}/smoke-strats
                -DMIN_STRATS=${_smoke_min_strats}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/smoke_strats_mutate.cmake")
        set_tests_properties("smoke/strats-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT TIMEOUT 900)

        add_test(NAME "smoke/native-known-positive"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-kp"
                    --min-cases 12800000 --min-passes 150 --known-positive)
        set_tests_properties("smoke/native-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

        # The engine-parity arm. Every other smoke cell compiles with
        # MCC_FORCE_REPLAY=1, so until this arm existed the AST constant
        # evaluator with no RIR replay under it -- which is what -O0 actually
        # ships -- was never run by smoke at all, the slice ladder was measured
        # only as a refusal histogram, and the --embed-jit binary was built and
        # thrown away without ever being executed. This arm builds the one
        # subject under six evaluation engines (ast, rir at -O0, rir at -O4,
        # the CPU slice ladder, the same ladder dispatching to the device, and
        # the embedded JIT re-baking at run time) and compares all 1782 dumped
        # rows plus the sweep digest against the ast baseline, so a divergence
        # names the engine and the row rather than a whole-file digest.
        #
        # Each engine must also prove it is the engine it claims to be: the ast
        # arm fails if the RIR census recorded any replay, the ladder arms fail
        # if the ladder certified nothing or disagreed with the tree on a self
        # pair, and the jit arm fails if the binary never swapped a function.
        # Without those guards six agreeing engines can be one engine run six
        # times. smoke/engines-identity mutates the environment to prove each
        # guard reads what it claims to; see cmake/smoke_engines_mutate.cmake.
        add_test(NAME "smoke/engines"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-engines"
                    --engines --min-engines 8 --min-cases 9000000)
        set_tests_properties("smoke/engines" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

        add_test(NAME "smoke/engines-known-positive"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-engines-kp"
                    --engines --min-engines 8 --known-positive)
        set_tests_properties("smoke/engines-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

        add_test(NAME "smoke/engines-identity"
            COMMAND "${CMAKE_COMMAND}"
                -DSMOKERUN=$<TARGET_FILE:smokerun>
                -DMCC=${_mcc_exe}
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DWORK=${CMAKE_BINARY_DIR}/smoke-engines-id
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/smoke_engines_mutate.cmake")
        set_tests_properties("smoke/engines-identity" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT TIMEOUT 1800)

        add_test(NAME "smoke/slice-bails"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-slice"
                    --slice-census)
        set_tests_properties("smoke/slice-bails" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

        # -O13 is the search tier and the level sweep cannot walk to it, because
        # -O5..-O12 are a hard error. It was the one level nothing watched. This banks
        # the strategies that stay DARK rather than their fire counts: a fire count must
        # not fall, which is the wrong direction for a monotone-decreasing bank, whereas
        # a strategy going dark is a new category (hard fail) and one lighting up is a
        # row that fell to zero (IMPROVED). Costs ~45 s -- the -O13 compile of the smoke
        # subject, measured, not the "~2.7 s" the board carried from before scases.h.
        add_test(NAME "smoke/strat-dark"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-dark"
                    --strat-dark)
        set_tests_properties("smoke/strat-dark" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

        if(DIFF3_GCC)
            set(MCC_SMOKE_GCC "${DIFF3_GCC}")
        else()
            find_program(MCC_SMOKE_GCC NAMES gcc-15 gcc)
        endif()
        if(NOT MCC_SMOKE_CLANG)
            if(DIFF3_CLANG)
                set(MCC_SMOKE_CLANG "${DIFF3_CLANG}")
            else()
                find_program(MCC_SMOKE_CLANG NAMES clang-22 clang)
            endif()
        endif()
        # On PE the smoke references must be mingw-target: an MSVC-target clang
        # cannot link the smoke subject (no compiler-rt __extendhfsf2/__mulsc3)
        # and would silently drop out, collapsing every divergence verdict to
        # diverge-one against a two-reference bank. The bank header names the
        # required llvm-mingw clang; pass -DMCC_SMOKE_CLANG=... to point at it.
        if(MCC_TARGETOS STREQUAL "WIN32" AND MCC_SMOKE_CLANG)
            execute_process(COMMAND "${MCC_SMOKE_CLANG}" --print-target-triple
                OUTPUT_VARIABLE _smoke_clang_triple OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET RESULT_VARIABLE _smoke_clang_rc)
            if(NOT _smoke_clang_rc EQUAL 0 OR NOT _smoke_clang_triple MATCHES "windows-gnu")
                message(STATUS "mcc smoke: dropping clang reference ${MCC_SMOKE_CLANG} "
                               "(target '${_smoke_clang_triple}' is not *-windows-gnu)")
                set(MCC_SMOKE_CLANG "")
            endif()
        endif()
        add_test(NAME "smoke/divergence"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-div"
                    --divergence)
        set_tests_properties("smoke/divergence" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900
            ENVIRONMENT "MCC_SMOKE_GCC=${MCC_SMOKE_GCC};MCC_SMOKE_CLANG=${MCC_SMOKE_CLANG}")

        set(_smoke_dev_args --device --min-cases 500000)
        if(MCC_GPU_REQUIRED)
            list(APPEND _smoke_dev_args --require-device)
        endif()
        add_test(NAME "smoke/device"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-device"
                    ${_smoke_dev_args})
        set_tests_properties("smoke/device" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

        add_test(NAME "smoke/device-known-positive"
            COMMAND smokerun --mcc "${_mcc_exe}"
                    --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    --work "${CMAKE_BINARY_DIR}/smoke-device-kp"
                    ${_smoke_dev_args} --known-positive)
        set_tests_properties("smoke/device-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)
    else()
        set(_smoke_why "smokerun drives a native mcc and runs what it builds, so it cannot run in a cross build")
        mcc_skip_test("smoke/native" "${_smoke_why}")
        mcc_skip_test("smoke/native-known-positive" "${_smoke_why}")
        mcc_skip_test("smoke/engines" "${_smoke_why}")
        mcc_skip_test("smoke/engines-known-positive" "${_smoke_why}")
        mcc_skip_test("smoke/engines-identity" "${_smoke_why}")
        mcc_skip_test("smoke/slice-bails" "${_smoke_why}")
        mcc_skip_test("smoke/strat-dark" "${_smoke_why}")
        mcc_skip_test("smoke/divergence" "${_smoke_why}")
        mcc_skip_test("smoke/device" "${_smoke_why}")
        mcc_skip_test("smoke/device-known-positive" "${_smoke_why}")
        set(_gcontract_why "runs a native mcc (-run / -c under a jump-table env) that cannot execute in a cross build")
        mcc_skip_test("bitint/scast-signext" "${_gcontract_why}")
        mcc_skip_test("bitint/scast-signext-known-positive" "${_gcontract_why}")
        mcc_skip_test("fold/int64-float" "${_gcontract_why}")
        mcc_skip_test("fold/int64-float-known-positive" "${_gcontract_why}")
        mcc_skip_test("switch/jumptable-exhaustive" "${_gcontract_why}")
        mcc_skip_test("switch/jumptable-falloff-known-positive" "${_gcontract_why}")
    endif()

    add_executable(exec_runner tests/runner.c)
    target_include_directories(exec_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set(_exec_env "MCC_TEST_EMU=${MCC_EMULATOR};MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_BCHECK=${_mcc_diag_bc};MCC_TEST_BACKTRACE=${_mcc_diag_bt}")

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

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-replay/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-replay-${_gn}" --only "${_gn}")
            set_tests_properties("exec-replay/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O1 -fno-reemit-templates -fno-promote-locals -fno-inline")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-replay-tmpl/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-replay-tmpl-${_gn}" --only "${_gn}")
            set_tests_properties("exec-replay-tmpl/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O1 -freemit-templates -fno-promote-locals -fno-inline")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-replay-promote/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-replay-promote-${_gn}" --only "${_gn}")
            set_tests_properties("exec-replay-promote/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O1 -fno-reemit-templates -fpromote-locals -fno-inline")
        endforeach()

        add_test(NAME ast/rir-position
            COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=${_mcc_exe}"
                    "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                    "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                    "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-position"
                    "-DOPT=-O1"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_position.cmake")
        set_tests_properties(ast/rir-position PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
            TIMEOUT 900)

        foreach(_ro -O1 -O2 -O3)
            string(REPLACE "-" "" _rotag "${_ro}")
            add_test(NAME ast/rir-parity-${_rotag}
                COMMAND "${CMAKE_COMMAND}"
                        "-DMCC=${_mcc_exe}"
                        "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                        "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                        "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-parity-${_rotag}"
                        "-DOPT=${_ro}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_parity.cmake")
            set_tests_properties(ast/rir-parity-${_rotag} PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
                TIMEOUT 900)

            add_test(NAME ast/rir-c2-${_rotag}
                COMMAND "${CMAKE_COMMAND}"
                        "-DMCC=${_mcc_exe}"
                        "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                        "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                        "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-c2-${_rotag}"
                        "-DOPT=${_ro}"
                        "-DBANKGAP=0"
                        "-DBANKSKIP=0"
                        "-DBANKFN=1150"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_c2.cmake")
            set_tests_properties(ast/rir-c2-${_rotag} PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
                TIMEOUT 1800)
        endforeach()

        add_test(NAME ast/rir-parity-O0
            COMMAND "${CMAKE_COMMAND}"
                    "-DMCC=${_mcc_exe}"
                    "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                    "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                    "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-parity-O0"
                    "-DOPT=-O0"
                    "-DFORCE=1"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_parity.cmake")
        set_tests_properties(ast/rir-parity-O0 PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
            TIMEOUT 900)

        set(_shadow_bash bash)
        if(MCC_TARGETOS STREQUAL "WIN32")
            # System32's bash.exe is the WSL relay and cannot run these scripts;
            # find a real one beside sh, or by asking sh itself (a scoop shim's
            # directory holds no bash, but its sh knows where /usr/bin/bash is)
            find_program(MCC_TEST_SH NAMES sh bash
                DOC "POSIX shell used to run the cli test pipelines on Windows")
            if(MCC_TEST_SH)
                get_filename_component(_shadow_sh_dir "${MCC_TEST_SH}" DIRECTORY)
                find_program(MCC_TEST_BASH NAMES bash
                    HINTS "${_shadow_sh_dir}" NO_DEFAULT_PATH
                    DOC "bash used to run the shadow-iv sweep on Windows")
                if(NOT MCC_TEST_BASH)
                    execute_process(COMMAND "${MCC_TEST_SH}" -c "cygpath -m \"$(command -v bash)\""
                        OUTPUT_VARIABLE _shadow_bash_probe OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET RESULT_VARIABLE _shadow_bash_rc)
                    if(_shadow_bash_rc EQUAL 0 AND EXISTS "${_shadow_bash_probe}")
                        set(MCC_TEST_BASH "${_shadow_bash_probe}" CACHE FILEPATH
                            "bash used to run the shadow-iv sweep on Windows" FORCE)
                    endif()
                endif()
            endif()
            set(_shadow_bash "${MCC_TEST_BASH}")
        endif()
        set(_shadow_pathmod "")
        if(_shadow_bash AND MCC_TARGETOS STREQUAL "WIN32")
            # dirname/mktemp/find live beside bash; a bare bash.exe does not put
            # its own usr/bin on PATH
            get_filename_component(_shadow_bash_dir "${_shadow_bash}" DIRECTORY)
            file(TO_NATIVE_PATH "${_shadow_bash_dir}" _shadow_bash_dir)
            set(_shadow_pathmod ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${_shadow_bash_dir}")
        endif()
        foreach(_sv x86_64 i386 arm arm64 riscv64)
            if(NOT _shadow_bash)
                mcc_skip_test(cross/shadow-iv-${_sv} "no usable bash on this Windows host"
                    LABELS "cross")
            else()
                add_test(NAME cross/shadow-iv-${_sv}
                    COMMAND "${_shadow_bash}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/shadow-iv-sweep.sh" "${_sv}")
                set_tests_properties(cross/shadow-iv-${_sv} PROPERTIES
                    FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                    LABELS "cross" TIMEOUT 3600
                    ENVIRONMENT "MCC_CROSS_DIR=${MCC_CROSS_DIR};MCC_BUILD_DIR=${CMAKE_BINARY_DIR}"
                    ${_shadow_pathmod})
            endif()
        endforeach()

        foreach(_st riscv64 arm arm64)
            add_test(NAME cross/shared-tls-refused-${_st}
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/shared-tls-refused.sh"
                        "${_st}" "${MCC_CROSS_DIR}/mcc-${_st}" "${MCC_CROSS_DIR}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gentoo-stage3-${_st}-glibc"
                        "${CMAKE_BINARY_DIR}/shared-tls-${_st}")
            set_tests_properties(cross/shared-tls-refused-${_st} PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                LABELS "cross" TIMEOUT 300)
        endforeach()

        foreach(_na riscv64 arm arm64 i386
                    x86_64-win32 i386-win32 arm64-win32 arm-win32
                    x86_64-osx arm64-osx)
            set(_na_sr "-")
            if(_na MATCHES "^(riscv64|arm|arm64|i386)$")
                set(_na_sr "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gentoo-stage3-${_na}-glibc")
            endif()
            add_test(NAME cross/no-compiler-abort-${_na}
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/no-compiler-abort.sh"
                        "${_na}" "${MCC_CROSS_DIR}/mcc-${_na}" "${MCC_CROSS_DIR}"
                        "${_na_sr}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                        -O0 -O1 -O2 -O3 -Os)
            set_tests_properties(cross/no-compiler-abort-${_na} PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                LABELS "cross" TIMEOUT 1800)
        endforeach()

        add_test(NAME cross/xferret-nonlval-riscv64
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/xferret-nonlval.sh"
                    "${MCC_CROSS_DIR}/mcc-riscv64" "${MCC_CROSS_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gentoo-stage3-riscv64-glibc"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/structs_unions/struct_byval.c"
                    "${CMAKE_BINARY_DIR}/xferret-nonlval-riscv64.o")
        set_tests_properties(cross/xferret-nonlval-riscv64 PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
            LABELS "cross" TIMEOUT 300)

        foreach(_aa riscv64 arm64 i386 arm)
            add_test(NAME cross/asan-accesstype-${_aa}
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/asan-accesstype.sh"
                        "${_aa}" "${MCC_CROSS_DIR}/mcc-${_aa}" "${MCC_CROSS_DIR}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gentoo-stage3-${_aa}-glibc"
                        "${CMAKE_BINARY_DIR}/asan-accesstype-${_aa}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/lib/mccasan.c")
            set_tests_properties(cross/asan-accesstype-${_aa} PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                LABELS "cross" TIMEOUT 600)
        endforeach()

        foreach(_bf riscv64 arm arm64 i386)
            add_test(NAME cross/bf16-qemu-${_bf}
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/bf16-qemu.sh"
                        "${_bf}" "${MCC_CROSS_DIR}/mcc-${_bf}" "${MCC_CROSS_DIR}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gentoo-stage3-${_bf}-glibc"
                        "${CMAKE_BINARY_DIR}/bf16-qemu-${_bf}")
            set_tests_properties(cross/bf16-qemu-${_bf} PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                LABELS "cross" TIMEOUT 300)
        endforeach()

        set(_c2bank_riscv64 "3;2;1157")
        set(_c2bank_arm "1;0;1131")
        set(_c2bank_arm64 "1;0;1200")
        set(_c2bank_i386 "0;0;1153")
        set(_c2bank_x86_64-win32 "0;0;1238")
        set(_c2bank_i386-win32 "1;0;1258")
        set(_c2bank_arm64-win32 "1;0;1298")
        set(_c2bank_arm-win32 "1;0;1236")
        set(_c2bank_x86_64-osx "0;0;1143")
        set(_c2bank_arm64-osx "1;0;1225")

        foreach(_rp riscv64 arm arm64 i386
                    x86_64-win32 i386-win32 arm64-win32 arm-win32
                    x86_64-osx arm64-osx)
            set(_rpsr "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gentoo-stage3-${_rp}-glibc")
            if(_rp MATCHES "(win32|wince)$")
                set(_rpf "-B ${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32 -B ${CMAKE_CURRENT_SOURCE_DIR}/runtime -I ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
            elseif(_rp MATCHES "osx$")
                set(_rpf "-B ${CMAKE_CURRENT_SOURCE_DIR}/runtime -I ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
            elseif(_rp STREQUAL "arm")
                set(_rpf "-B ${MCC_CROSS_DIR} -mfloat-abi hard --sysroot=${_rpsr} -I${_rpsr}/usr/include")
            else()
                set(_rpf "-B ${MCC_CROSS_DIR} --sysroot=${_rpsr} -I${_rpsr}/usr/include")
            endif()
            add_test(NAME ast/rir-parity-${_rp}
                COMMAND "${CMAKE_COMMAND}"
                        "-DMCC=${MCC_CROSS_DIR}/mcc-${_rp}"
                        "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                        "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                        "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-parity-${_rp}"
                        "-DOPT=-O1"
                        "-DMCCFLAGS=${_rpf}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_parity.cmake")
            set_tests_properties(ast/rir-parity-${_rp} PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                LABELS "cross" TIMEOUT 1800)

            set(_rpc2f "${_rpf}")
            if(_rp STREQUAL "arm")
                set(_rpc2f "-B ${MCC_CROSS_DIR} -mfloat-abi hard -I ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include --sysroot=${_rpsr} -I${_rpsr}/usr/include")
            elseif(NOT _rp MATCHES "(win32|wince|osx)$")
                set(_rpc2f "-B ${MCC_CROSS_DIR} -I ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include --sysroot=${_rpsr} -I${_rpsr}/usr/include")
            endif()
            list(GET _c2bank_${_rp} 0 _c2gap)
            list(GET _c2bank_${_rp} 1 _c2skip)
            list(GET _c2bank_${_rp} 2 _c2fn)
            add_test(NAME ast/rir-c2-${_rp}
                COMMAND "${CMAKE_COMMAND}"
                        "-DMCC=${MCC_CROSS_DIR}/mcc-${_rp}"
                        "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                        "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                        "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-c2-${_rp}"
                        "-DOPT=-O1"
                        "-DMCCFLAGS=${_rpc2f}"
                        "-DBANKGAP=${_c2gap}"
                        "-DBANKSKIP=${_c2skip}"
                        "-DBANKFN=${_c2fn}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_c2.cmake")
            set_tests_properties(ast/rir-c2-${_rp} PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                LABELS "cross" TIMEOUT 1800)

            add_test(NAME ast/rir-parity-${_rp}-O0
                COMMAND "${CMAKE_COMMAND}"
                        "-DMCC=${MCC_CROSS_DIR}/mcc-${_rp}"
                        "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                        "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                        "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-parity-${_rp}-O0"
                        "-DOPT=-O0"
                        "-DFORCE=1"
                        "-DMCCFLAGS=${_rpf}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_parity.cmake")
            set_tests_properties(ast/rir-parity-${_rp}-O0 PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                LABELS "cross" TIMEOUT 1800)

            foreach(_rpo -O2 -O3)
                string(REPLACE "-" "" _rpot "${_rpo}")
                add_test(NAME ast/rir-parity-${_rp}-${_rpot}
                    COMMAND "${CMAKE_COMMAND}"
                            "-DMCC=${MCC_CROSS_DIR}/mcc-${_rp}"
                            "-DCORPUS=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec"
                            "-DEXTRA=${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c"
                            "-DTMPDIR=${CMAKE_BINARY_DIR}/ast-rir-parity-${_rp}-${_rpot}"
                            "-DOPT=${_rpo}"
                            "-DMCCFLAGS=${_rpf}"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ast/rir_parity.cmake")
                set_tests_properties(ast/rir-parity-${_rp}-${_rpot} PROPERTIES
                    FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" SKIP_RETURN_CODE 77
                    LABELS "cross" TIMEOUT 1800)
            endforeach()
        endforeach()

        set(MCC_WINE "" CACHE STRING
            "wine binary the PE test tiers should prefer (empty: auto-detect wine64/wine/...)")
        set(MCC_WINE_REQUIRED OFF CACHE BOOL
            "fail (rather than skip) the PE wine tiers when no wine is found on this host")
        foreach(_rt x86_64 i386 arm arm64 riscv64
                    x86_64-win32 i386-win32 x86_64-osx arm64-osx)
            set(_rtfixreq "MCC_BUILT;MCC_CROSS_BUILT")
            set(_rtlock "")
            if(_rt MATCHES "win32$")
                set(_rtlabels LABELS "wine")
                set(_rtlock RESOURCE_LOCK "wine")
            elseif(_rt MATCHES "osx$")
                set(_rtlabels LABELS "macho")
            elseif(_rt STREQUAL "x86_64")
                set(_rtlabels "")
            else()
                set(_rtlabels LABELS "qemu")
                set(_rtfixreq "${_rtfixreq};FX_qemu-${_rt}-glibc")
            endif()
            add_test(NAME run-tier/${_rt}
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/run-tier.sh"
                        "${_rt}" "${CMAKE_BINARY_DIR}" "${MCC_CROSS_DIR}")
            set_tests_properties(run-tier/${_rt} PROPERTIES
                FIXTURES_REQUIRED "${_rtfixreq}" SKIP_RETURN_CODE 77
                ${_rtlock} ${_rtlabels} TIMEOUT 1800)
            if(_rt MATCHES "win32$")
                set_tests_properties(run-tier/${_rt} PROPERTIES
                    ENVIRONMENT "MCC_WINE=${MCC_WINE};MCC_WINE_REQUIRED=${MCC_WINE_REQUIRED}")
            endif()
        endforeach()
        foreach(_rt arm64-win32 arm-win32)
            mcc_skip_test(run-tier/${_rt}
                "${_rt} -run needs a ${_rt}-hosted mcc, and no host here can execute one: wine emulates x86 PE only and qemu-user cannot load PE"
                LABELS "wine" TIMEOUT 1800)
        endforeach()

        # run-tier drives tests/run with no -O flag, so the -run corpus only ever
        # ran at -O0 -- where the AST replay path is off entirely. Every arena
        # consumer reached only by -run was therefore untested at -O1 and above,
        # and a segfault compiling an operand-carrying inline asm with -O2 -run
        # survived two full 8600-cell ctest runs because of it. This sweeps the
        # same corpus over every -O level and both JIT settings on the host mcc.
        add_test(NAME run-opt/native
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/run-opt.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}")
        set_tests_properties(run-opt/native PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT" SKIP_RETURN_CODE 77 TIMEOUT 900)

        find_program(MCC_OBJCOPY objcopy)
        if(UNIX AND MCC_OBJCOPY)
            add_test(NAME superopt/promote-floor
                COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/promote-floor.sh"
                        "${_mcc_exe}" "${CMAKE_BINARY_DIR}/superopt-floor"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/src/spillheavy.c")
            set_tests_properties(superopt/promote-floor PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
                TIMEOUT 600 RUN_SERIAL TRUE)
        else()
            mcc_skip_test(superopt/promote-floor
                "the promote-floor ratchet counts spill slots with objcopy on a POSIX host; no objcopy on PATH, or not a POSIX host"
                LABELS native)
        endif()

        find_program(MCC_OBJDUMP NAMES objdump)
        if(UNIX AND MCC_OBJDUMP)
            add_test(NAME superopt/global-reload
                COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/global-reload.sh"
                        "${_mcc_exe}" "${_bdir}"
                        "${CMAKE_BINARY_DIR}/global-reload"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/src/globreload.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/globreload.awk")
            set_tests_properties(superopt/global-reload PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
                TIMEOUT 600)
            add_test(NAME superopt/global-reload-known-positive
                COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/global-reload.sh"
                        "${_mcc_exe}" "${_bdir}"
                        "${CMAKE_BINARY_DIR}/global-reload-kp"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/src/globreload-broken.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/superopt/globreload.awk"
                        known-positive)
            set_tests_properties(superopt/global-reload-known-positive PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
                TIMEOUT 600)
        else()
            mcc_skip_test(superopt/global-reload
                "the global-reload cell reads relocations out of objdump -dr on a POSIX host; no objdump on PATH, or not a POSIX host"
                LABELS native)
            mcc_skip_test(superopt/global-reload-known-positive
                "the global-reload cell reads relocations out of objdump -dr on a POSIX host; no objdump on PATH, or not a POSIX host"
                LABELS native)
        endif()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-narrowfix/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-narrowfix-${_gn}" --only "${_gn}")
            set_tests_properties("exec-narrowfix/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -fnarrow-fix -fsethi-ullman-leaf -ftree-ccp-iterate -ftree-loop-im -fivopts -ftree-pre -ftree-dse -foptimize-sibling-calls -fgcse -ftree-vrp -fdivmagic -fif-conversion-abs -ftree-reassoc;MCC_DEV=1;MCC_AST_CSE_WINDOW=256;MCC_AST_CPROP_WINDOW=512")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-chainstore/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-chainstore-${_gn}" --only "${_gn}")
            set_tests_properties("exec-chainstore/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -fchain-store -fpromote-incdec")
        endforeach()

        # exec-chainstore above pins only -fchain-store, which leaves
        # chain-store-live off and never reaches the pairing pass, and no -O
        # level below 4 turns the family on by itself. The pairing marks are
        # what the strategies invalidate, so nothing below this rung reaches the
        # arena state at all. The storeval-rot defect that used to block a
        # chain_member_mix fixture here is fixed -- docs/TODO.md debt #6a -- so
        # that fixture is now free to be added.
        foreach(_gn IN ITEMS chained_assign assign_value_effects dead_store_elim
                             cse local_const_prop region_store)
            add_test(NAME "exec-chainlive/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-chainlive-${_gn}" --only "${_gn}")
            set_tests_properties("exec-chainlive/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 900
                ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O4 -fchain-store -fchain-store-live -fchain-store-member")
        endforeach()

        # storeval-rot is default-on from -O1, so the shipped ladder is where its
        # replay has to balance the vstack. The rot mark makes a store's value a
        # single-use live entry; an outer AST_StoreVal that recomputed the middle
        # store's value subtree consumed that entry twice and the function ended
        # on "vstack leak (-1)" -- docs/TODO.md debt #6a. These rungs pin the flag
        # explicitly at three levels so the shape cannot be silently lost to a
        # default change, and the fixtures carry chains whose outer target is a
        # member/element rather than a leaf, which is what defeats the mark.
        foreach(_lv IN ITEMS 1 2 3)
            foreach(_gn IN ITEMS chained_assign assign_value_effects
                                 local_const_prop region_store)
                add_test(NAME "exec-storevalrot${_lv}/${_gn}"
                    COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                            "${CMAKE_BINARY_DIR}/exec-storevalrot${_lv}-${_gn}" --only "${_gn}")
                set_tests_properties("exec-storevalrot${_lv}/${_gn}" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900
                    ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O${_lv} -fstoreval-rot -fstoreval-constl -fstoreval-calllast")
            endforeach()
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-ivsrptr/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-ivsrptr-${_gn}" --only "${_gn}")
            set_tests_properties("exec-ivsrptr/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -fivopts-ptr;MCC_DEV=1")
        endforeach()

        if(MCC_TARGETOS STREQUAL "WIN32")
            foreach(_gl IN LISTS _exec_glines)
                string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
                set(_gn "${CMAKE_MATCH_1}")
                add_test(NAME "exec-gatecombo/${_gn}"
                    COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                            "${CMAKE_BINARY_DIR}/exec-gatecombo-${_gn}" --only "${_gn}")
                set_tests_properties("exec-gatecombo/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -fchain-store -fpromote-incdec -fivopts-ptr;MCC_DEV=1")
            endforeach()
        else()
            foreach(_gl IN LISTS _exec_glines)
                string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
                set(_gn "${CMAKE_MATCH_1}")
                mcc_skip_test("exec-gatecombo/${_gn}"
                    "the -fchain-store+-fpromote-incdec+-fivopts-ptr combined gate is registered on a WIN32 target build only")
            endforeach()
        endif()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-vlat/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-vlat-${_gn}" --only "${_gn}")
            set_tests_properties("exec-vlat/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -floop-vlat")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-select/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-select-${_gn}" --only "${_gn}")
            set_tests_properties("exec-select/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -fif-conversion")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-zerobss/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-zerobss-${_gn}" --only "${_gn}")
            set_tests_properties("exec-zerobss/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -fzero-initialized-in-bss")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-interchange/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-interchange-${_gn}" --only "${_gn}")
            set_tests_properties("exec-interchange/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -floop-interchange;MCC_DEV=1")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-fusion/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-fusion-${_gn}" --only "${_gn}")
            set_tests_properties("exec-fusion/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -floop-fusion;MCC_DEV=1")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-tile/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-tile-${_gn}" --only "${_gn}")
            set_tests_properties("exec-tile/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -floop-block;MCC_DEV=1;MCC_AST_TILE_SIZE=7")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-mergestrings/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-mergestrings-${_gn}" --only "${_gn}")
            set_tests_properties("exec-mergestrings/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O2 -fmerge-constants")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-search/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-search-${_gn}" --only "${_gn}")
            set_tests_properties("exec-search/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O13 -fopt-search;MCC_SEARCH_WORKER=1")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-search-emitsize/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-search-emitsize-${_gn}" --only "${_gn}")
            set_tests_properties("exec-search-emitsize/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O13 -fopt-search -fopt-search-emit-size;MCC_SEARCH_WORKER=1")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-search-emitiso/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-search-emitiso-${_gn}" --only "${_gn}")
            set_tests_properties("exec-search-emitiso/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O13 -fopt-search -fopt-search-emit-size -fopt-search-emit-iso;MCC_SEARCH_WORKER=1")
        endforeach()

        # T-lin-10417: the exec-search-threads/* family (every exec group under
        # -O13 -fopt-search -fopt-search-threads) is RETIRED. That flag now runs
        # the search serially (the racy pool is gated behind
        # MCC_OPT_SEARCH_THREADS_UNSAFE), so the family is byte-for-byte identical
        # to exec-search/* and only re-ran the same slow -O13 search (it was the
        # source of the atomic_counter deadlock that blocked a goal eval 83 min).
        # The focused opt/search-threads-no-deadlock cell above covers "the flag
        # is accepted and does not deadlock". Re-add this family only alongside a
        # real fix to the worker ast_cur/ast_*_env sharing.

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-gatesoff/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-gatesoff-${_gn}" --only "${_gn}")
            set_tests_properties("exec-gatesoff/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O3 -fno-chain-store")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-O3/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-O3-${_gn}" --only "${_gn}")
            set_tests_properties("exec-O3/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O3")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-O1/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-O1-${_gn}" --only "${_gn}")
            set_tests_properties("exec-O1/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-O1")
        endforeach()

        foreach(_gl IN LISTS _exec_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_gl}")
            set(_gn "${CMAKE_MATCH_1}")
            add_test(NAME "exec-Os/${_gn}"
                COMMAND exec_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/exec-Os-${_gn}" --only "${_gn}")
            set_tests_properties("exec-Os/${_gn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_exec_env};MCC_TEST_OPT=-Os")
        endforeach()

        # T-lin-10478/win: native x86_64-PE optfire -run leg. The optfire matrix +
        # native block below are if(UNIX)-gated (optfire.sh needs a POSIX shell and
        # the UNIX cross/qemu machinery). On win-PE the mcc is already built and runs
        # PE natively, so register the optimizer differ cells to run-verify via
        # `mcc -O{n} [gflag] -run` through the same POSIX shell the cli tests use --
        # the OPTFIRE_EXECVIA=run leg of DETAILS#t-lin-10476-impl-interface (5), with
        # FIXTURES_REQUIRED MCC_BUILT only (no cross build, no hosted-boot).
        if(WIN32)
            set(_ofw_sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/optfire.sh")
            file(GLOB _ofw_dfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/differs*.txt")
            set(_ofw_seen "")
            foreach(_ofwfile IN LISTS _ofw_dfiles)
                file(STRINGS "${_ofwfile}" _ofwlines)
                foreach(_ofwl IN LISTS _ofwlines)
                    if(NOT _ofwl MATCHES "^#" AND NOT _ofwl STREQUAL "")
                        string(REPLACE "|" ";" _ofw "${_ofwl}")
                        list(GET _ofw 0 _ofwname)
                        list(GET _ofw 1 _ofwopt)
                        list(GET _ofw 2 _ofwarg)
                        list(LENGTH _ofw _ofwlen)
                        set(_ofwextra "-")
                        if(_ofwlen GREATER 3)
                            list(GET _ofw 3 _ofwextra)
                        endif()
                        set(_ofwldf "-")
                        if(_ofwlen GREATER 4)
                            list(GET _ofw 4 _ofwldf)
                        endif()
                        set(_ofwsrc "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/src/${_ofwname}.c")
                        if(NOT "${_ofwname}" IN_LIST _ofw_seen)
                            list(APPEND _ofw_seen "${_ofwname}")
                            if(MCC_TEST_SH AND EXISTS "${_ofwsrc}")
                                add_test(NAME "optfire-x86_64/${_ofwname}"
                                    COMMAND "${MCC_TEST_SH}" "${_ofw_sh}" differ "${_mcc_exe}"
                                            "${_ofwsrc}" "${CMAKE_BINARY_DIR}/optfire-x86_64"
                                            "${_ofwname}" "${_ofwopt}" "${_ofwarg}"
                                            "${_ofwextra}" "${_ofwldf}")
                                set_tests_properties("optfire-x86_64/${_ofwname}" PROPERTIES
                                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native
                                    ENVIRONMENT "OPTFIRE_EXECVIA=run;OPTFIRE_MCCFLAGS=-B${CMAKE_BINARY_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
                            elseif(NOT MCC_TEST_SH)
                                mcc_skip_test("optfire-x86_64/${_ofwname}"
                                    "no POSIX shell (MCC_TEST_SH) on this Windows host to run optfire.sh; set MCC_TEST_SH to a git-bash sh to run-verify the optimizer differ cells via mcc -run on native PE"
                                    LABELS native)
                            else()
                                mcc_skip_test("optfire-x86_64/${_ofwname}"
                                    "no tests/optfire/src/${_ofwname}.c on this host for this differs row"
                                    LABELS native)
                            endif()
                        endif()
                    endif()
                endforeach()
            endforeach()
        endif()
        if(UNIX)
            set(_of_seen "")
            set(_of_sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/optfire.sh")
            set(_of_boot "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/hosted-boot.sh")
            set(_of_matrix
                "arm64|linux|qemu-aarch64|run"
                "riscv64|linux|qemu-riscv64|run"
                "i386|linux|qemu-i386|fired")
            set(_of_archnames "")
            set(_of_archcpus "")
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/arch.txt")
                file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/arch.txt" _of_alines)
                foreach(_al IN LISTS _of_alines)
                    if(NOT _al MATCHES "^#" AND NOT _al STREQUAL "")
                        string(REPLACE "|" ";" _av "${_al}")
                        list(GET _av 0 _an)
                        list(GET _av 1 _ac)
                        list(APPEND _of_archnames "${_an}")
                        list(APPEND _of_archcpus "${_ac}")
                    endif()
                endforeach()
            endif()
            foreach(_ofrow IN LISTS _of_matrix)
            string(REPLACE "|" ";" _ofrl "${_ofrow}")
            list(GET _ofrl 0 _ofx_cpu)
            list(GET _ofrl 2 _ofx_runner)
            list(GET _ofrl 3 _ofx_canrun)
            mcc_cross_cc(${_ofx_cpu} _ofx_cc _ofx_fix)
            if(NOT _ofx_cc STREQUAL "")
                set(_ofx_have 1)
            else()
                set(_ofx_have 0)
            endif()
            set(_ofx_run 0)
            set(_ofx_hosted "")
            set(_ofx_hfix "")
            if(_ofx_canrun STREQUAL "run")
                set(_ofx_sr "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gentoo-stage3-${_ofx_cpu}-glibc")
                find_program(_OF_QEMU_${_ofx_cpu} ${_ofx_runner})
                if(_OF_QEMU_${_ofx_cpu}
                   AND EXISTS "${_ofx_sr}"
                   AND EXISTS "${MCC_CROSS_DIR}/mcc-${_ofx_cpu}${MCC_EXESUF}"
                   AND EXISTS "${MCC_CROSS_DIR}/${_ofx_cpu}-libmccrt.a"
                   AND EXISTS "${MCC_CROSS_DIR}/${_ofx_cpu}-runmain.o")
                    set(_ofx_run 1)
                    set(_ofx_hdir "${CMAKE_BINARY_DIR}/optfire-hosted/${_ofx_cpu}")
                    set(_ofx_hosted "${_ofx_hdir}/mcc-hosted")
                    set(_ofx_hfix "FX_optfire-${_ofx_cpu}")
                    add_test(NAME "optfire-boot/${_ofx_cpu}"
                        COMMAND sh "${_of_boot}" "${_ofx_cpu}" "${MCC_CROSS_DIR}"
                                "${_ofx_sr}" "${_ofx_hdir}" "${CMAKE_CURRENT_SOURCE_DIR}")
                    set_tests_properties("optfire-boot/${_ofx_cpu}" PROPERTIES
                        FIXTURES_SETUP "${_ofx_hfix}"
                        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT"
                        SKIP_RETURN_CODE 77 LABELS qemu TIMEOUT 600)
                else()
                    mcc_skip_test("optfire-boot/${_ofx_cpu}"
                        "no ${_ofx_runner} + vendored gentoo-stage3-${_ofx_cpu}-glibc sysroot + ${MCC_CROSS_DIR}/mcc-${_ofx_cpu} cross toolchain on this host, so no ${_ofx_cpu}-hosted mcc can be bootstrapped to -run optfire cells; the differ cells fall back to fired-only. Install qemu-user + `cmake --preset cross`, or point -DMCC_CROSS_DIR at a tree with mcc-${_ofx_cpu} and its runtime"
                        LABELS qemu)
                endif()
            endif()
            file(GLOB _ofx_dfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/differs*.txt")
            file(GLOB _ofx_xfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/cdelta*.txt")
            set(_ofx_seen "")
            set(_ofx_rows 0)
            foreach(_offile IN LISTS _ofx_dfiles _ofx_xfiles)
                set(_ofxmode "differ")
                if(_offile MATCHES "/cdelta[^/]*\\.txt$")
                    set(_ofxmode "cdelta")
                endif()
                file(STRINGS "${_offile}" _oflines)
                foreach(_ol IN LISTS _oflines)
                    if(NOT _ol MATCHES "^#" AND NOT _ol STREQUAL "")
                        math(EXPR _ofx_rows "${_ofx_rows} + 1")
                        string(REPLACE "|" ";" _of "${_ol}")
                        list(GET _of 0 _ofname)
                        list(GET _of 1 _ofopt)
                        list(GET _of 2 _ofarg)
                        list(LENGTH _of _oflen)
                        set(_ofextra "-")
                        if(_oflen GREATER 3)
                            list(GET _of 3 _ofextra)
                        endif()
                        set(_ofcell "${_ofname}")
                        set(_ofx_tail "-")
                        if(_ofxmode STREQUAL "cdelta")
                            set(_ofcell "cdelta-${_ofname}")
                            if(_oflen GREATER 4)
                                list(GET _of 4 _ofx_tail)
                            endif()
                        endif()
                        set(_ofsrc "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/src/${_ofname}.c")
                        set(_ofskip 0)
                        list(FIND _of_archnames "${_ofname}" _ofai)
                        if(NOT _ofai EQUAL -1)
                            list(GET _of_archcpus ${_ofai} _ofreq)
                            string(REPLACE "," ";" _ofreq "${_ofreq}")
                            if(NOT "${_ofx_cpu}" IN_LIST _ofreq)
                                set(_ofskip 1)
                            endif()
                        endif()
                        if("${_ofcell}" IN_LIST _ofx_seen)
                            set(_ofskip 1)
                        endif()
                        if(NOT _ofskip AND EXISTS "${_ofsrc}")
                            list(APPEND _ofx_seen "${_ofcell}")
                            if(_ofx_run AND _ofxmode STREQUAL "differ")
                                add_test(NAME "optfire-${_ofx_cpu}/${_ofcell}"
                                    COMMAND "${_of_sh}"
                                            "${_ofxmode}" "${_ofx_hosted}" "${_ofsrc}"
                                            "${CMAKE_BINARY_DIR}/optfire-${_ofx_cpu}" "${_ofcell}"
                                            "${_ofopt}" "${_ofarg}" "${_ofextra}" "${_ofx_tail}")
                                set_tests_properties("optfire-${_ofx_cpu}/${_ofcell}" PROPERTIES
                                    FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT;${_ofx_hfix}"
                                    SKIP_RETURN_CODE 77 LABELS qemu
                                    ENVIRONMENT "OPTFIRE_EXECVIA=run")
                            elseif(_ofx_have)
                                add_test(NAME "optfire-${_ofx_cpu}/${_ofcell}"
                                    COMMAND "${_of_sh}"
                                            "${_ofxmode}" "${_ofx_cc}" "${_ofsrc}"
                                            "${CMAKE_BINARY_DIR}/optfire-${_ofx_cpu}" "${_ofcell}"
                                            "${_ofopt}" "${_ofarg}" "${_ofextra}" "${_ofx_tail}")
                                set_tests_properties("optfire-${_ofx_cpu}/${_ofcell}" PROPERTIES
                                    FIXTURES_REQUIRED "${_ofx_fix}" SKIP_RETURN_CODE 77 LABELS native
                                    ENVIRONMENT "OPTFIRE_NORUN=1;OPTFIRE_MCCFLAGS=-B${CMAKE_BINARY_DIR}")
                            else()
                                mcc_skip_test("optfire-${_ofx_cpu}/${_ofcell}"
                                    "no ${_ofx_cpu} cross compiler on this host, and a POSIX host is required: build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-${_ofx_cpu}"
                                    LABELS native)
                            endif()
                        endif()
                    endif()
                endforeach()
            endforeach()
            endforeach()

            if(APPLE)
                file(GLOB _ofm_dfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/differs*.txt")
                foreach(_ofmrow IN ITEMS "arm64-osx|arm64|native" "x86_64-osx|x86_64|rosetta")
                    string(REPLACE "|" ";" _ofml "${_ofmrow}")
                    list(GET _ofml 0 _ofm_leg)
                    list(GET _ofml 1 _ofm_cpu)
                    list(GET _ofml 2 _ofm_kind)
                    set(_ofm_fix "MCC_BUILT")
                    if(_ofm_kind STREQUAL "native")
                        set(_ofm_mcc "${_mcc_exe}")
                        set(_ofm_runpfx "")
                        set(_ofm_bdir "${CMAKE_BINARY_DIR}")
                        set(_ofm_ok 1)
                    else()
                        set(_ofm_x64dir "${CMAKE_CURRENT_SOURCE_DIR}/cmake-macos-x64")
                        set(_ofm_mcc "${_ofm_x64dir}/mcc")
                        set(_ofm_runpfx "arch -x86_64")
                        set(_ofm_bdir "${_ofm_x64dir}")
                        if(EXISTS "${_ofm_mcc}")
                            set(_ofm_ok 1)
                            set(_ofm_fix "MCC_BUILT;FX_optfire_x64")
                            add_test(NAME "optfire-x64boot/rosetta"
                                COMMAND ${CMAKE_COMMAND} --build "${_ofm_x64dir}" --target mcc)
                            set_tests_properties("optfire-x64boot/rosetta" PROPERTIES
                                FIXTURES_SETUP "FX_optfire_x64" FIXTURES_REQUIRED "MCC_BUILT"
                                SKIP_RETURN_CODE 77 LABELS "macho;native" TIMEOUT 600)
                        else()
                            set(_ofm_ok 0)
                            mcc_skip_test("optfire-x64boot/rosetta"
                                "no cmake-macos-x64/mcc (x86_64 Rosetta backend build) on this host, so the x86_64-osx JIT -run boot fixture can't run; build it with: CC=/usr/bin/cc cmake -S . -B cmake-macos-x64 -G Ninja -DCMAKE_OSX_ARCHITECTURES=x86_64 -DMCC_TARGET_ARCH=x86_64 && ninja -C cmake-macos-x64 mcc runmain.o libmccrt.a"
                                LABELS "macho;native")
                        endif()
                    endif()
                    foreach(_offile IN LISTS _ofm_dfiles)
                        file(STRINGS "${_offile}" _ofmlines)
                        foreach(_ol IN LISTS _ofmlines)
                            if(NOT _ol MATCHES "^#" AND NOT _ol STREQUAL "")
                                string(REPLACE "|" ";" _of "${_ol}")
                                list(GET _of 0 _ofname)
                                list(GET _of 1 _ofopt)
                                list(GET _of 2 _ofarg)
                                list(LENGTH _of _oflen)
                                set(_ofextra "-")
                                if(_oflen GREATER 3)
                                    list(GET _of 3 _ofextra)
                                endif()
                                set(_ofmskip 0)
                                list(FIND _of_archnames "${_ofname}" _ofmai)
                                if(NOT _ofmai EQUAL -1)
                                    list(GET _of_archcpus ${_ofmai} _ofmreq)
                                    string(REPLACE "," ";" _ofmreq "${_ofmreq}")
                                    if(NOT "${_ofm_cpu}" IN_LIST _ofmreq)
                                        set(_ofmskip 1)
                                    endif()
                                endif()
                                set(_ofmsrc "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/src/${_ofname}.c")
                                if(NOT _ofmskip AND EXISTS "${_ofmsrc}")
                                    if(_ofm_ok)
                                        add_test(NAME "optfire-${_ofm_leg}/${_ofname}"
                                            COMMAND "${_of_sh}" "differ" "${_ofm_mcc}" "${_ofmsrc}"
                                                    "${CMAKE_BINARY_DIR}/optfire-${_ofm_leg}" "${_ofname}"
                                                    "${_ofopt}" "${_ofarg}" "${_ofextra}" "-")
                                        set_tests_properties("optfire-${_ofm_leg}/${_ofname}" PROPERTIES
                                            FIXTURES_REQUIRED "${_ofm_fix}" SKIP_RETURN_CODE 77 LABELS "macho;native"
                                            ENVIRONMENT "OPTFIRE_EXECVIA=run;OPTFIRE_AOT=1;OPTFIRE_RUN=${_ofm_runpfx};OPTFIRE_MCCFLAGS=-B${_ofm_bdir}")
                                    else()
                                        mcc_skip_test("optfire-${_ofm_leg}/${_ofname}"
                                            "no cmake-macos-x64/mcc (x86_64 Rosetta backend build) on this host, so the x86_64-osx JIT -run leg can't register; build it with: CC=/usr/bin/cc cmake -S . -B cmake-macos-x64 -G Ninja -DCMAKE_OSX_ARCHITECTURES=x86_64 -DMCC_TARGET_ARCH=x86_64 && ninja -C cmake-macos-x64 mcc runmain.o libmccrt.a"
                                            LABELS "macho;native")
                                    endif()
                                endif()
                            endif()
                        endforeach()
                    endforeach()
                endforeach()
            endif()

            # Row-count ratchet. These cells are registered one-per-row from globbed
            # tables, so deleting a row deletes a cell and ctest reports the same N/N
            # with a smaller N. A drop is a hard error; adding rows is fine, and the
            # floor is meant to be raised when you add them.
            if(_ofx_rows LESS 52)
                message(FATAL_ERROR
                    "optfire differs/cdelta tables registered ${_ofx_rows} rows, below the "
                    "floor of 52. A row that disappears takes its ctest cell with it and "
                    "nothing else reports the loss. If the removal was deliberate, lower "
                    "the floor in the same commit and say why")
            endif()

            file(GLOB _of_cfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/counters*.txt")
            file(GLOB _of_dfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/differs*.txt")
            file(GLOB _of_lfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/levels*.txt")
            file(GLOB _of_sfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/defstate*.txt")
            file(GLOB _of_xfiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/cdelta*.txt")
            set(_of_rows 0)
            foreach(_offile IN LISTS _of_cfiles _of_dfiles _of_lfiles _of_sfiles _of_xfiles)
                set(_ofmode "differ")
                if(_offile MATCHES "/counters[^/]*\\.txt$")
                    set(_ofmode "counter")
                elseif(_offile MATCHES "/levels[^/]*\\.txt$")
                    set(_ofmode "level")
                elseif(_offile MATCHES "/defstate[^/]*\\.txt$")
                    set(_ofmode "defstate")
                elseif(_offile MATCHES "/cdelta[^/]*\\.txt$")
                    set(_ofmode "cdelta")
                endif()
                file(STRINGS "${_offile}" _oflines)
                foreach(_ol IN LISTS _oflines)
                    if(NOT _ol MATCHES "^#" AND NOT _ol STREQUAL "")
                        math(EXPR _of_rows "${_of_rows} + 1")
                        string(REPLACE "|" ";" _of "${_ol}")
                        list(GET _of 0 _ofname)
                        list(GET _of 1 _ofopt)
                        list(GET _of 2 _ofarg)
                        list(LENGTH _of _oflen)
                        set(_ofextra "-")
                        if(_oflen GREATER 3)
                            list(GET _of 3 _ofextra)
                        endif()
                        set(_ofextra2 "-")
                        if(_oflen GREATER 4)
                            list(GET _of 4 _ofextra2)
                        endif()
                        set(_ofsrc "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/src/${_ofname}.c")
                        set(_ofcell "${_ofname}")
                        if(_ofmode STREQUAL "level")
                            set(_ofcell "level-${_ofname}")
                        elseif(_ofmode STREQUAL "defstate")
                            set(_ofcell "default-${_ofname}")
                        elseif(_ofmode STREQUAL "cdelta")
                            set(_ofcell "cdelta-${_ofname}")
                        endif()
                        set(_ofskip 0)
                        list(FIND _of_archnames "${_ofname}" _ofai)
                        if(NOT _ofai EQUAL -1)
                            list(GET _of_archcpus ${_ofai} _ofreq)
                            string(REPLACE "," ";" _ofreq "${_ofreq}")
                            if(NOT MCC_CPU IN_LIST _ofreq)
                                set(_ofskip 1)
                            endif()
                        endif()
                        if(_ofcell IN_LIST _of_seen)
                            message(STATUS "optfire: skipping duplicate case ${_ofcell}")
                        elseif(NOT EXISTS "${_ofsrc}")
                            message(STATUS "optfire: skipping ${_ofname} (no source)")
                        elseif(_ofskip)
                            list(APPEND _of_seen "${_ofcell}")
                            mcc_skip_test("optfire/${_ofcell}"
                                "pinned to CPU ${_ofreq}; this build targets ${MCC_CPU}"
                                LABELS native)
                        else()
                            list(APPEND _of_seen "${_ofcell}")
                            if(_ofmode STREQUAL "counter")
                                add_test(NAME "optfire/${_ofcell}"
                                    COMMAND "${_of_sh}" counter "${_mcc_exe}" "${_ofsrc}"
                                            "${CMAKE_BINARY_DIR}/optfire" "${_ofcell}"
                                            "${_ofopt}" "${_ofarg}" "${_ofextra}")
                            elseif(_ofmode STREQUAL "cdelta")
                                add_test(NAME "optfire/${_ofcell}"
                                    COMMAND "${_of_sh}" cdelta "${_mcc_exe}" "${_ofsrc}"
                                            "${CMAKE_BINARY_DIR}/optfire" "${_ofcell}"
                                            "${_ofopt}" "${_ofarg}" "${_ofextra}"
                                            "${_ofextra2}")
                            elseif(_ofmode STREQUAL "defstate")
                                add_test(NAME "optfire/${_ofcell}"
                                    COMMAND "${_of_sh}" defstate "${_mcc_exe}" "${_ofsrc}"
                                            "${CMAKE_BINARY_DIR}/optfire" "${_ofcell}"
                                            "${_ofopt}" "${_ofarg}" "${_ofextra}"
                                            "${_ofextra2}")
                            elseif(_ofmode STREQUAL "level")
                                add_test(NAME "optfire/${_ofcell}"
                                    COMMAND "${_of_sh}" level "${_mcc_exe}" "${_ofsrc}"
                                            "${CMAKE_BINARY_DIR}/optfire" "${_ofcell}"
                                            "-" "${_ofopt}" "${_ofarg}")
                            else()
                                set(_ofldf "-")
                                if(_oflen GREATER 4)
                                    list(GET _of 4 _ofldf)
                                endif()
                                add_test(NAME "optfire/${_ofcell}"
                                    COMMAND "${_of_sh}" differ "${_mcc_exe}" "${_ofsrc}"
                                            "${CMAKE_BINARY_DIR}/optfire" "${_ofcell}"
                                            "${_ofopt}" "${_ofarg}" "${_ofextra}" "${_ofldf}")
                            endif()
                            set_tests_properties("optfire/${_ofcell}" PROPERTIES
                                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native)
                        endif()
                    endif()
                endforeach()
            endforeach()

            if(_of_rows LESS 125)
                message(FATAL_ERROR
                    "optfire counters/differs/levels/defstate/cdelta tables registered "
                    "${_of_rows} rows, below the floor of 125. Same reason as the differs "
                    "ratchet above: row count drives cell count, so a silent deletion is "
                    "indistinguishable from a smaller suite")
            endif()

            add_test(NAME "optfire/coverage-ledger"
                COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/coverage-check.sh")
            set_tests_properties("optfire/coverage-ledger" PROPERTIES
                SKIP_RETURN_CODE 77 LABELS native)

            set(_fs_sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/flagsweep.sh")
            set(_fs_py "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/cover3.py")
            set(_fs_tbl "${CMAKE_CURRENT_SOURCE_DIR}/src/mccopt.h")
            set(_fs_cov "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/cover3.txt")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${_fs_tbl}" "${_fs_cov}")

            add_test(NAME "flagsweep/accept"
                COMMAND "${_fs_sh}" accept "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_BINARY_DIR}/flagsweep/accept")
            set_tests_properties("flagsweep/accept" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS flagsweep
                ENVIRONMENT "MCC_DEV=1")

            add_test(NAME "flagsweep/dev-gate"
                COMMAND "${_fs_sh}" devgate "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_BINARY_DIR}/flagsweep/devgate")
            set_tests_properties("flagsweep/dev-gate" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS flagsweep
                ENVIRONMENT "MCC_DEV=")

            add_test(NAME "flagsweep/dev-gate-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                    -DFS=${_fs_sh} -DMCC=${_mcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
                    -DWORK=${CMAKE_BINARY_DIR}/flagsweep/devgate-kp
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/flagsweep_devgate_mutate.cmake")
            set_tests_properties("flagsweep/dev-gate-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT LABELS flagsweep TIMEOUT 300)

            set(_la_sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/labeladdr.sh")
            set(_la_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/src/labeladdr.c")
            add_test(NAME "optfire/labeladdr"
                COMMAND "${_la_sh}" "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_BINARY_DIR}/labeladdr" "${_la_src}")
            set_tests_properties("optfire/labeladdr" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "flagsweep;native" TIMEOUT 300)
            add_test(NAME "optfire/labeladdr-known-positive"
                COMMAND "${_la_sh}" "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_BINARY_DIR}/labeladdr-kp" "${_la_src}"
                        known-positive)
            set_tests_properties("optfire/labeladdr-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "flagsweep;native" TIMEOUT 300)

            set(_ar_sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/asmreplay.sh")
            set(_ar_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/src/asmreplay.c")
            add_test(NAME "optfire/asm-replay-recover"
                COMMAND "${_ar_sh}" "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_BINARY_DIR}/asmreplay" "${_ar_src}")
            set_tests_properties("optfire/asm-replay-recover" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "replay;native" TIMEOUT 300)
            add_test(NAME "optfire/asm-replay-recover-known-positive"
                COMMAND "${_ar_sh}" "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_BINARY_DIR}/asmreplay-kp" "${_ar_src}"
                        known-positive)
            set_tests_properties("optfire/asm-replay-recover-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "replay;native" TIMEOUT 300)


            file(READ "${_fs_tbl}" _fs_txt)
            string(REGEX MATCHALL "MCC_OPT_ROW\\([A-Z0-9_]+, *\"[a-z0-9-]+\"" _fs_rows "${_fs_txt}")
            string(REGEX MATCHALL "MCC_OPT_ROW\\([A-Z0-9_]+, *\"[a-z0-9-]+\", *MCC_OPTD_DEV"
                   _fs_devrows "${_fs_txt}")
            unset(_fs_txt)
            set(_fs_flags "")
            foreach(_fr IN LISTS _fs_rows)
                string(REGEX MATCH "\"([a-z0-9-]+)\"" _ignore "${_fr}")
                list(APPEND _fs_flags "${CMAKE_MATCH_1}")
            endforeach()
            set(_fs_devflags "")
            foreach(_fr IN LISTS _fs_devrows)
                string(REGEX MATCH "\"([a-z0-9-]+)\"" _ignore "${_fr}")
                list(APPEND _fs_devflags "${CMAKE_MATCH_1}")
            endforeach()
            list(LENGTH _fs_flags _fs_nflag)
            list(LENGTH _fs_devflags _fs_ndev)
            foreach(_ff IN LISTS _fs_flags)
                add_test(NAME "flagsweep-exec/${_ff}"
                    COMMAND "${_fs_sh}" exec "${_mcc_exe}" "${_bdir}" "${_idir}"
                            "${CMAKE_BINARY_DIR}/flagsweep/exec-${_ff}" "${_ff}")
                set(_fs_env "MCC_DEV=")
                if(_ff IN_LIST _fs_devflags)
                    set(_fs_env "MCC_DEV=1")
                endif()
                set_tests_properties("flagsweep-exec/${_ff}" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    LABELS flagsweep TIMEOUT 300 ENVIRONMENT "${_fs_env}")
            endforeach()
            message(STATUS "flagsweep: ${_fs_nflag} -f flags from src/mccopt.h, "
                           "${_fs_ndev} of them MCC_OPTD_DEV")

            option(MCC_FLAGSWEEP_FULL
                "Run the whole 3-way covering array over the -f surface as ctest cells (label flagsweep-full)." OFF)
            find_program(MCC_PYTHON3 NAMES python3 python)
            if(NOT MCC_PYTHON3)
                mcc_skip_test(flagsweep/cover3-verify "python3 not found"
                    LABELS flagsweep)
            elseif(NOT EXISTS "${_fs_cov}")
                mcc_skip_test(flagsweep/cover3-verify "no cover3.txt; run tests/optfire/cover3.py gen"
                    LABELS flagsweep)
            else()
                add_test(NAME "flagsweep/cover3-verify"
                    COMMAND "${MCC_PYTHON3}" "${_fs_py}" verify "${_fs_cov}")
                set_tests_properties("flagsweep/cover3-verify" PROPERTIES
                    LABELS flagsweep TIMEOUT 300)
            endif()

            set(_ld_py "${CMAKE_CURRENT_SOURCE_DIR}/tools/leveldiff.py")
            set(_ld_known "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/leveldiff-known.txt")
            set(_ld_corpus "${MCC_VENDOR_DIR}/gcc-c-torture-execute")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ld_known}")
            if(NOT MCC_PYTHON3)
                mcc_skip_test("optlevel/torture-differential" "python3 not found")
                mcc_skip_test("optlevel/torture-differential-known-positive" "python3 not found")
            elseif(NOT EXISTS "${_ld_corpus}")
                mcc_skip_test("optlevel/torture-differential"
                    "no corpus at ${_ld_corpus} (symlink an upstream gcc checkout's gcc/testsuite/gcc.c-torture/execute there; it is deliberately not vendored)")
                mcc_skip_test("optlevel/torture-differential-known-positive"
                    "no corpus at ${_ld_corpus}; the real cell is skipped too")
            else()
                add_test(NAME "optlevel/torture-differential"
                    COMMAND "${MCC_PYTHON3}" "${_ld_py}"
                            --mcc "${_mcc_exe}" --corpus "${_ld_corpus}"
                            --known "${_ld_known}"
                            --work "${CMAKE_BINARY_DIR}/leveldiff"
                            --levels 0,1,2,3,4
                            --jobs 32 --minprog 1200)
                set_tests_properties("optlevel/torture-differential" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    LABELS native TIMEOUT 900)
                add_test(NAME "optlevel/torture-differential-known-positive"
                    COMMAND "${CMAKE_COMMAND}"
                            -DPY=${MCC_PYTHON3}
                            -DTOOL=${_ld_py}
                            -DMCC=${_mcc_exe}
                            -DCORPUS=${_ld_corpus}
                            -DKNOWN=${_ld_known}
                            -DWORK=${CMAKE_BINARY_DIR}/leveldiff-kp
                            -DJOBS=32
                            -DSUBJECT=20000112-1.c
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/leveldiff_mutate.cmake")
                set_tests_properties("optlevel/torture-differential-known-positive" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT LABELS native TIMEOUT 900)
            endif()

            set(_fs_nrow 0)
            if(EXISTS "${_fs_cov}")
                file(STRINGS "${_fs_cov}" _fs_rowline REGEX "^rows ")
                if(_fs_rowline)
                    list(GET _fs_rowline 0 _fs_rowline)
                    string(REGEX MATCH "^rows ([0-9]+)" _ignore "${_fs_rowline}")
                    set(_fs_nrow "${CMAKE_MATCH_1}")
                endif()
            endif()
            if(_fs_nrow GREATER 0)
                math(EXPR _fs_last "${_fs_nrow} - 1")
                foreach(_fn RANGE 0 ${_fs_last})
                    if(MCC_FLAGSWEEP_FULL)
                        add_test(NAME "flagsweep-cover/${_fn}"
                            COMMAND "${_fs_sh}" cover "${_mcc_exe}" "${_bdir}" "${_idir}"
                                    "${CMAKE_BINARY_DIR}/flagsweep/cover-${_fn}" "${_fn}")
                        set_tests_properties("flagsweep-cover/${_fn}" PROPERTIES
                            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                            LABELS "flagsweep;flagsweep-full" TIMEOUT 600
                            ENVIRONMENT "MCC_DEV=1")
                    else()
                        mcc_skip_test("flagsweep-cover/${_fn}"
                            "the ${_fs_nrow} covering-array rows are opt-in: cmake -DMCC_FLAGSWEEP_FULL=ON, then ctest -L flagsweep-full"
                            LABELS "flagsweep\;flagsweep-full")
                    endif()
                endforeach()
                message(STATUS "flagsweep: ${_fs_nrow} covering-array rows as flagsweep-cover/* cells")
            endif()

            # Strategy-registry sweeps. The registry (src/mccast.c,
            # ast_strategies[]) is driven as an ordered sequence, so the two
            # things nothing else checks are: does a row hold up run ALONE, and
            # do two rows commute. MCC_AST_STRAT_ORDER makes both askable.
            #
            # Every one of the 22 rows is covered on both axes. The split
            # between the two tiers is by corpus and by how much of the
            # 22*21*20 = 9240 triple space is walked, not by which rows are
            # looked at:
            #
            #   default    all 22 rows in isolation and six permutation shards,
            #              over the 31-program subject list. 29 cells, 55
            #              sec*proc on an idle machine and 145 under a loaded
            #              one, no cell over ~20s either way.
            #   opt-in     the same 22 rows in isolation over the whole exec
            #              corpus (282 admitted of 300), and all 9240 triples
            #              bare plus all 9240 with rows 0..3 in front. 86
            #              cells, 6440 sec*proc, 7min at -j16. All green
            #              2026-08-06. -DMCC_STRATSWEEP_FULL=ON, label
            #              stratsweep-full.
            #
            # The full-corpus isolation cells cannot be in the default gate:
            # admission alone builds 282 programs twice, so no such cell
            # finishes under a minute and eighteen of them cost more wall than
            # the rest of the suite put together.
            #
            # CTest's CONFIGURATIONS property is not usable for the opt-in: a
            # ctest run with no -C matches every configuration, so the cells
            # would land in the default gate anyway.
            set(_ss_sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/stratsweep.sh")
            execute_process(COMMAND "${_ss_sh}" names
                OUTPUT_VARIABLE _ss_names OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _ss_names_rc)
            string(REPLACE "\n" ";" _ss_names "${_ss_names}")
            set(_ss_rows "")
            foreach(_ss_line IN LISTS _ss_names)
                string(REGEX REPLACE "^[0-9]+:" "" _ss_line "${_ss_line}")
                if(_ss_line)
                    list(APPEND _ss_rows "${_ss_line}")
                endif()
            endforeach()
            list(LENGTH _ss_rows _ss_nrow)
            if(NOT _ss_names_rc EQUAL 0 OR _ss_nrow LESS 24)
                message(FATAL_ERROR
                    "stratsweep.sh names returned '${_ss_names_rc}' and ${_ss_nrow} strategy "
                    "row(s), below the floor of 24. This list used to be a hand-written mirror "
                    "of STRAT_NAMES and had drifted two rows -- sra and sroa had no "
                    "stratsweep/iso-* cell at all -- so it is derived now and floored here")
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${_ss_sh}")
            add_test(NAME "stratsweep/check" COMMAND "${_ss_sh}" check)
            set_tests_properties("stratsweep/check" PROPERTIES
                SKIP_RETURN_CODE 77 LABELS "native;stratsweep")
            foreach(_ss_row IN LISTS _ss_rows)
                add_test(NAME "stratsweep/iso-${_ss_row}"
                    COMMAND "${_ss_sh}" iso "${_mcc_exe}" "${_bdir}"
                            "${_idir}" "${CMAKE_BINARY_DIR}/stratsweep-iso-${_ss_row}"
                            "${_ss_row}" subjects)
                set_tests_properties("stratsweep/iso-${_ss_row}" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    LABELS "native;stratsweep" TIMEOUT 600)
            endforeach()
            # Four shards of 1024 = 40 of the 9240 ordered triples, bare; two
            # more run their shard with rows 0..3 in front, which is the
            # composition the real pipeline performs. A sample, not a proof --
            # the proof is the opt-in tier below. The shard divisor is what
            # holds these to 4s idle and 19s loaded: at 1/512 they were 31s
            # apiece, and when ctest schedules them last they are the tail of
            # the whole run and set its wall time.
            foreach(_ss_shard RANGE 0 3)
                add_test(NAME "stratsweep/perm3-${_ss_shard}"
                    COMMAND "${_ss_sh}" perm3 "${_mcc_exe}" "${_bdir}"
                            "${_idir}" "${CMAKE_BINARY_DIR}/stratsweep-perm3-${_ss_shard}"
                            "${_ss_shard}" 1024 subjects -)
                set_tests_properties("stratsweep/perm3-${_ss_shard}" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    LABELS "native;stratsweep" TIMEOUT 600)
            endforeach()
            foreach(_ss_shard RANGE 0 1)
                add_test(NAME "stratsweep/perm3-based-${_ss_shard}"
                    COMMAND "${_ss_sh}" perm3 "${_mcc_exe}" "${_bdir}"
                            "${_idir}" "${CMAKE_BINARY_DIR}/stratsweep-perm3b-${_ss_shard}"
                            "${_ss_shard}" 1024 subjects 0,1,2,3)
                set_tests_properties("stratsweep/perm3-based-${_ss_shard}" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    LABELS "native;stratsweep" TIMEOUT 600)
            endforeach()
            set(_ss_optin "the exhaustive strategy tier is opt-in (6440 sec*proc): cmake -DMCC_STRATSWEEP_FULL=ON, then ctest -L stratsweep-full")
            foreach(_ss_row IN LISTS _ss_rows)
                if(MCC_STRATSWEEP_FULL)
                    add_test(NAME "stratsweep/isofull-${_ss_row}"
                        COMMAND "${_ss_sh}" iso "${_mcc_exe}" "${_bdir}"
                                "${_idir}" "${CMAKE_BINARY_DIR}/stratsweep-isofull-${_ss_row}"
                                "${_ss_row}" full)
                    set_tests_properties("stratsweep/isofull-${_ss_row}" PROPERTIES
                        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                        LABELS "native;stratsweep;stratsweep-full" TIMEOUT 3600)
                else()
                    mcc_skip_test("stratsweep/isofull-${_ss_row}" "${_ss_optin}"
                        LABELS "native\;stratsweep\;stratsweep-full")
                endif()
            endforeach()
            foreach(_ss_shard RANGE 0 31)
                if(MCC_STRATSWEEP_FULL)
                    add_test(NAME "stratsweep/perm3-exhaustive-${_ss_shard}"
                        COMMAND "${_ss_sh}" perm3 "${_mcc_exe}" "${_bdir}"
                                "${_idir}" "${CMAKE_BINARY_DIR}/stratsweep-full-${_ss_shard}"
                                "${_ss_shard}" 32 subjects -)
                    set_tests_properties("stratsweep/perm3-exhaustive-${_ss_shard}" PROPERTIES
                        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                        LABELS "native;stratsweep;stratsweep-full" TIMEOUT 3600)
                    add_test(NAME "stratsweep/perm3-exhaustive-based-${_ss_shard}"
                        COMMAND "${_ss_sh}" perm3 "${_mcc_exe}" "${_bdir}"
                                "${_idir}" "${CMAKE_BINARY_DIR}/stratsweep-fullb-${_ss_shard}"
                                "${_ss_shard}" 32 subjects 0,1,2,3)
                    set_tests_properties("stratsweep/perm3-exhaustive-based-${_ss_shard}" PROPERTIES
                        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                        LABELS "native;stratsweep;stratsweep-full" TIMEOUT 3600)
                else()
                    mcc_skip_test("stratsweep/perm3-exhaustive-${_ss_shard}" "${_ss_optin}"
                        LABELS "native\;stratsweep\;stratsweep-full")
                    mcc_skip_test("stratsweep/perm3-exhaustive-based-${_ss_shard}" "${_ss_optin}"
                        LABELS "native\;stratsweep\;stratsweep-full")
                endif()
            endforeach()
        else()
            # gate-contract manifest gates registered only in this UNIX branch need
            # a host-skip twin so they read as host-skipped (not "declared but not
            # registered") on non-POSIX targets such as PE/Win32. See T-win-50001.
            mcc_skip_test("flagsweep/dev-gate" "the flagsweep dev-gate needs a POSIX host build")
            mcc_skip_test("flagsweep/dev-gate-known-positive" "the flagsweep dev-gate needs a POSIX host build")
            mcc_skip_test("optfire/labeladdr" "the optfire labeladdr gate needs a POSIX host build")
            mcc_skip_test("optfire/labeladdr-known-positive" "the optfire labeladdr gate needs a POSIX host build")
            mcc_skip_test("optfire/asm-replay-recover" "the optfire asm-replay-recover gate needs a POSIX host build")
            mcc_skip_test("optfire/asm-replay-recover-known-positive" "the optfire asm-replay-recover gate needs a POSIX host build")
            mcc_skip_test("optlevel/torture-differential" "the optlevel torture-differential gate needs a POSIX host build")
            mcc_skip_test("optlevel/torture-differential-known-positive" "the optlevel torture-differential gate needs a POSIX host build")
        endif()

        # optfire/asm-replay-recover above compares what the program PRINTS,
        # and none of the ways replay corrupts an asm body change that. This
        # one compares the object, which is the artefact the A/B defends.
        set(_ao_sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/asmobject.sh")
        set(_ao_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/src/asmobject.c")
        add_test(NAME "optfire/asm-replay-object"
            COMMAND sh "${_ao_sh}" "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_BINARY_DIR}/asmobject" "${_ao_src}")
        set_tests_properties("optfire/asm-replay-object" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
            LABELS "replay;native" TIMEOUT 300)
        add_test(NAME "optfire/asm-replay-object-known-positive"
            COMMAND sh "${_ao_sh}" "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_BINARY_DIR}/asmobject-kp" "${_ao_src}"
                    known-positive)
        set_tests_properties("optfire/asm-replay-object-known-positive" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
            LABELS "replay;native" TIMEOUT 300)

    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/cases.h" _cli_clines
         REGEX "^[ \t]*{[ \t]*\"[A-Za-z0-9_]+\"[ \t]*,")
    if(MCC_EMULATOR)
        foreach(_cl IN LISTS _cli_clines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_cl}")
            set(_cn "${CMAKE_MATCH_1}")
            mcc_skip_test("cli/${_cn}" "structural readelf/nm suite runs on native host only")
        endforeach()
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
            foreach(_cl IN LISTS _cli_clines)
                string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_cl}")
                set(_cn "${CMAKE_MATCH_1}")
                mcc_skip_test("cli/${_cn}"
                    "needs a POSIX shell (no sh/bash found; set MCC_TEST_SH)")
            endforeach()
        else()
            add_executable(cli_runner tests/cli/runner.c)
            target_include_directories(cli_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli")
            set(_cli_env "MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_DWARF=${MCC_CONFIG_DWARF};MCC_TEST_BACKTRACE=${_mcc_diag_bt};MCC_TEST_BCHECK=${_mcc_diag_bc}")
            if(_cli_sh)
                set(_cli_env "${_cli_env};MCC_TEST_SH=${_cli_sh}")
            endif()
            foreach(_cl IN LISTS _cli_clines)
                string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_cl}")
                set(_cn "${CMAKE_MATCH_1}")
                add_test(NAME "cli/${_cn}"
                    COMMAND cli_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                            "${CMAKE_BINARY_DIR}/cli-gran-${_cn}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli" --only "${_cn}")
                set_tests_properties("cli/${_cn}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                    SKIP_RETURN_CODE 77 TIMEOUT 90 ENVIRONMENT "${_cli_env}")
                if(_win_sh_pathmod)
                    set_tests_properties("cli/${_cn}" PROPERTIES
                        ENVIRONMENT_MODIFICATION "${_win_sh_pathmod}")
                endif()
            endforeach()
        endif()
    endif()

    # DIFF3_GCC/DIFF3_CLANG are resolved at the top of MCC_BUILD_TESTS -- the
    # cref-oracle corpora consume them ~1500 lines above here.
    set(MCC_DIFF3_EXTRA_REFS "" CACHE STRING
        "Extra diff3 reference compilers as a ;-list of label=path; each must accept a gcc-style '-o' build line and run its output natively on the host. Widens the N-way majority consensus (use independent compiler families, not a second build of one already listed -- the runner de-dups identical --version but cannot tell two gcc builds apart, and double-weighting one family skews the vote).")

    set(_diff3_refs "")
    if(DIFF3_GCC)
        list(APPEND _diff3_refs "gcc=${DIFF3_GCC}")
    endif()
    if(DIFF3_CLANG)
        list(APPEND _diff3_refs "clang=${DIFF3_CLANG}")
    endif()
    foreach(_xr IN LISTS MCC_DIFF3_EXTRA_REFS)
        if(_xr)
            list(APPEND _diff3_refs "${_xr}")
        endif()
    endforeach()
    set(_diff3_ref_args "")
    foreach(_r IN LISTS _diff3_refs)
        string(FIND "${_r}" "=" _d3eq)
        if(_d3eq GREATER 0)
            string(SUBSTRING "${_r}" 0 ${_d3eq} _d3lbl)
            math(EXPR _d3vs "${_d3eq} + 1")
            string(SUBSTRING "${_r}" ${_d3vs} -1 _d3path)
            list(APPEND _diff3_ref_args --ref "${_d3lbl}" "${_d3path}")
        endif()
    endforeach()
    list(LENGTH _diff3_refs _diff3_nref)

    set(_diff3_sh_ok TRUE)
    if(MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TEST_SH)
        set(_diff3_sh_ok FALSE)
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/goldens.h" _d3_glines
         REGEX "^[ \t]*{[ \t]*\"[A-Za-z0-9_]+\"[ \t]*,[ \t]*\"run2?\"")
    if(NOT MCC_EMULATOR AND _diff3_nref GREATER_EQUAL 2 AND _diff3_sh_ok)
        add_executable(diff3_runner tests/diff3/runner.c)
        target_include_directories(diff3_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec")
        set(_diff3_env "MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS}")
        if(MCC_TEST_SH)
            set(_diff3_env "${_diff3_env};MCC_TEST_SH=${MCC_TEST_SH}")
        endif()
        foreach(_d3l IN LISTS _d3_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_d3l}")
            set(_d3n "${CMAKE_MATCH_1}")
            add_test(NAME "diff3/${_d3n}"
                COMMAND diff3_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                        "${CMAKE_BINARY_DIR}/diff3-gran-${_d3n}"
                        ${_diff3_ref_args} --only "${_d3n}")
            set_tests_properties("diff3/${_d3n}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_diff3_env}")
            if(_win_sh_pathmod)
                set_tests_properties("diff3/${_d3n}" PROPERTIES
                    ENVIRONMENT_MODIFICATION "${_win_sh_pathmod}")
            endif()
        endforeach()
    else()
        if(NOT _diff3_sh_ok)
            set(_d3why "needs a POSIX shell on Windows (no sh/bash found; set MCC_TEST_SH)")
        else()
            set(_d3why "needs native host + >=2 distinct reference compilers (gcc/clang, or add -DMCC_DIFF3_EXTRA_REFS=label=path; build the clang-toolchain target for a self-contained clang)")
        endif()
        foreach(_d3l IN LISTS _d3_glines)
            string(REGEX MATCH "\"([A-Za-z0-9_]+)\"" _ignore "${_d3l}")
            set(_d3n "${CMAKE_MATCH_1}")
            mcc_skip_test("diff3/${_d3n}" "${_d3why}")
        endforeach()
    endif()

    if(NOT MCC_EMULATOR AND _diff3_nref GREATER_EQUAL 2 AND _diff3_sh_ok
       AND (MCC_CPU STREQUAL "x86_64" OR MCC_CPU STREQUAL "arm64")
       AND NOT MCC_TARGETOS STREQUAL "WIN32")
        add_executable(fuzz_runner tests/fuzz/runner.c)
        add_test(NAME "fuzz/smoke"
            COMMAND fuzz_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_BINARY_DIR}/fuzz-smoke" ${_diff3_ref_args}
                    --seed 1 --count 40)
        set_tests_properties("fuzz/smoke" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77 ENVIRONMENT "${_diff3_env}")
        add_test(NAME "fuzz/o4"
            COMMAND fuzz_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_BINARY_DIR}/fuzz-o4" ${_diff3_ref_args}
                    --seed 41000 --count 8 --o4)
        set_tests_properties("fuzz/o4" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77 ENVIRONMENT "${_diff3_env}")
        foreach(_fm_shard RANGE 0 3)
            math(EXPR _fm_seed "1 + ${_fm_shard} * 6")
            add_test(NAME "fuzz/matrix-${_fm_shard}"
                COMMAND fuzz_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                        "${CMAKE_BINARY_DIR}/fuzz-matrix-${_fm_shard}" ${_diff3_ref_args}
                        --seed ${_fm_seed} --count 6 --gates)
            set_tests_properties("fuzz/matrix-${_fm_shard}" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_diff3_env}")
        endforeach()
        add_test(NAME "fuzz/corpus"
            COMMAND fuzz_runner "${_mcc_exe}" "${_bdir}" "${_idir}"
                    "${CMAKE_BINARY_DIR}/fuzz-corpus" ${_diff3_ref_args}
                    --corpus "${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/corpus" --replay)
        set_tests_properties("fuzz/corpus" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77 ENVIRONMENT "${_diff3_env}")
    else()
        set(_fzwhy "needs native x86_64/arm64 host + >=2 distinct reference compilers (differential oracle executes mcc's own target ISA; add -DMCC_DIFF3_EXTRA_REFS=label=path to widen)")
        mcc_skip_test("fuzz/smoke" "${_fzwhy}")
        mcc_skip_test("fuzz/o4" "${_fzwhy}")
        foreach(_fm_shard RANGE 0 3)
            mcc_skip_test("fuzz/matrix-${_fm_shard}" "${_fzwhy}")
        endforeach()
        mcc_skip_test("fuzz/corpus" "${_fzwhy}")
    endif()

    file(GLOB _parts_wraps RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/parts"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/parts/run_*.c")
    if(MCC_TARGETOS STREQUAL "WIN32")
        foreach(_pw IN LISTS _parts_wraps)
            string(REGEX REPLACE "\\.c$" "" _pn "${_pw}")
            mcc_skip_test("parts/${_pn}" "native gcc/clang/mcc 3-way needs a shared C99 libc; the PE/msvcrt target has no msvcrt equivalent for the parts' complex/tgmath/libm surface (covered in aggregate by mcctest)")
        endforeach()
    elseif(NOT MCC_EMULATOR AND DIFF3_GCC AND DIFF3_CLANG
       AND TARGET mccharness)
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
        foreach(_pw IN LISTS _parts_wraps)
            string(REGEX REPLACE "\\.c$" "" _pn "${_pw}")
            mcc_skip_test("parts/${_pn}" "needs native host + gcc + clang + integrated assembler")
        endforeach()
    endif()

    file(GLOB_RECURSE _pp_cases RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess/*.c"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocess/*.S")
    if(NOT MCC_EMULATOR AND DIFF3_GCC AND DIFF3_CLANG)
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
        foreach(_ppc IN LISTS _pp_cases)
            string(REGEX REPLACE "\\.[cS]$" "" _ppn "${_ppc}")
            mcc_skip_test("preprocess/${_ppn}"
                "needs native host + both gcc and clang (build the clang-toolchain target for a self-contained clang)")
        endforeach()
    endif()

    if(MCC_TARGET_IS_HOST)
        add_executable(libmcc_test tests/embed/api_basic.c)
        target_link_libraries(libmcc_test PRIVATE libmcc)
        target_include_directories(libmcc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME libtest
            COMMAND libmcc_test "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(libtest PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        if(MCC_EMBED_JIT)
            add_executable(jit_selftest tests/embed/jit_selftest.c)
            target_link_libraries(jit_selftest PRIVATE libmcc)
            target_include_directories(jit_selftest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-leaf-int COMMAND jit_selftest)
            set_tests_properties(jit/selftest-leaf-int PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-reemit-gates COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SELFTEST_REEMIT_GATES=1 $<TARGET_FILE:jit_selftest>)
            set_tests_properties(jit/selftest-reemit-gates PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_stage2 tests/embed/jit_selftest_stage2.c)
            target_link_libraries(jit_selftest_stage2 PRIVATE libmcc)
            target_include_directories(jit_selftest_stage2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
                mcc_skip_test(jit/selftest-stage2
                    "call-bearing JIT recompile callee-binding unsupported on the MS x64 ABI")
            else()
                add_test(NAME jit/selftest-stage2 COMMAND jit_selftest_stage2)
                set_tests_properties(jit/selftest-stage2 PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            endif()

            add_executable(jit_selftest_kgc tests/embed/jit_selftest_kgc.c)
            target_link_libraries(jit_selftest_kgc PRIVATE libmcc)
            target_include_directories(jit_selftest_kgc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-kgc COMMAND jit_selftest_kgc)
            set_tests_properties(jit/selftest-kgc PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_nearmatch tests/embed/jit_selftest_nearmatch.c)
            target_link_libraries(jit_selftest_nearmatch PRIVATE libmcc)
            target_include_directories(jit_selftest_nearmatch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-nearmatch COMMAND jit_selftest_nearmatch)
            set_tests_properties(jit/selftest-nearmatch PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)

            add_executable(jit_selftest_purity tests/embed/jit_selftest_purity.c)
            target_link_libraries(jit_selftest_purity PRIVATE libmcc)
            target_include_directories(jit_selftest_purity PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-purity COMMAND jit_selftest_purity)
            set_tests_properties(jit/selftest-purity PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-purity-known-positive
                COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SELFTEST_PURITY_LIE=1
                        $<TARGET_FILE:jit_selftest_purity>)
            set_tests_properties(jit/selftest-purity-known-positive PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_strlit tests/embed/jit_selftest_strlit.c)
            target_link_libraries(jit_selftest_strlit PRIVATE libmcc)
            target_include_directories(jit_selftest_strlit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-strlit COMMAND jit_selftest_strlit)
            set_tests_properties(jit/selftest-strlit PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_struct tests/embed/jit_selftest_struct.c)
            target_link_libraries(jit_selftest_struct PRIVATE libmcc)
            target_include_directories(jit_selftest_struct PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-struct COMMAND jit_selftest_struct)
            set_tests_properties(jit/selftest-struct PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_ptrret tests/embed/jit_selftest_ptrret.c)
            target_link_libraries(jit_selftest_ptrret PRIVATE libmcc)
            target_include_directories(jit_selftest_ptrret PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-ptrret COMMAND jit_selftest_ptrret)
            set_tests_properties(jit/selftest-ptrret PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_lazy tests/embed/jit_selftest_lazy.c)
            target_link_libraries(jit_selftest_lazy PRIVATE libmcc)
            target_include_directories(jit_selftest_lazy PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-lazy COMMAND jit_selftest_lazy)
            set_tests_properties(jit/selftest-lazy PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_pool tests/embed/jit_selftest_pool.c)
            target_link_libraries(jit_selftest_pool PRIVATE libmcc)
            target_include_directories(jit_selftest_pool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-pool COMMAND jit_selftest_pool)
            set_tests_properties(jit/selftest-pool PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_eligibility tests/embed/jit_selftest_eligibility.c)
            target_link_libraries(jit_selftest_eligibility PRIVATE libmcc)
            target_include_directories(jit_selftest_eligibility PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-eligibility COMMAND jit_selftest_eligibility)
            set_tests_properties(jit/selftest-eligibility PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_submit tests/embed/jit_selftest_submit.c)
            target_link_libraries(jit_selftest_submit PRIVATE libmcc)
            target_include_directories(jit_selftest_submit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-submit COMMAND jit_selftest_submit)
            set_tests_properties(jit/selftest-submit PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_fork tests/embed/jit_selftest_fork.c)
            target_link_libraries(jit_selftest_fork PRIVATE libmcc)
            target_include_directories(jit_selftest_fork PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-fork COMMAND jit_selftest_fork)
            set_tests_properties(jit/selftest-fork PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_observability tests/embed/jit_selftest_observability.c)
            target_link_libraries(jit_selftest_observability PRIVATE libmcc)
            target_include_directories(jit_selftest_observability PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            # Drives the JIT fault injectors (MCC_JIT_FORCE_INFEASIBLE /
            # MCC_JIT_BENCH*), which compile out of libmcc unless MCC_DEV. Without
            # it the injector is inert and the cell would pass vacuously.
            if(MCC_DEV)
                add_test(NAME jit/selftest-observability COMMAND jit_selftest_observability)
                set_tests_properties(jit/selftest-observability PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test(jit/selftest-observability "requires -DMCC_DEV=ON (JIT fault injection)")
            endif()

            add_executable(jit_selftest_liverun tests/embed/jit_selftest_liverun.c)
            target_link_libraries(jit_selftest_liverun PRIVATE libmcc)
            target_include_directories(jit_selftest_liverun PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-liverun
                COMMAND jit_selftest_liverun "-B${CMAKE_BINARY_DIR}"
                        "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
            set_tests_properties(jit/selftest-liverun PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_poison tests/embed/jit_selftest_poison.c)
            target_link_libraries(jit_selftest_poison PRIVATE libmcc)
            target_include_directories(jit_selftest_poison PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-poison COMMAND jit_selftest_poison)
            set_tests_properties(jit/selftest-poison PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_bench tests/embed/jit_selftest_bench.c)
            target_link_libraries(jit_selftest_bench PRIVATE libmcc)
            target_include_directories(jit_selftest_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            # Drives MCC_JIT_BENCH*, which compiles out of libmcc unless MCC_DEV.
            if(MCC_DEV)
                add_test(NAME jit/selftest-bench COMMAND jit_selftest_bench)
                set_tests_properties(jit/selftest-bench PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test(jit/selftest-bench "requires -DMCC_DEV=ON (JIT bench gate)")
            endif()

            add_executable(jit_selftest_profile tests/embed/jit_selftest_profile.c)
            target_link_libraries(jit_selftest_profile PRIVATE libmcc)
            target_include_directories(jit_selftest_profile PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-profile COMMAND jit_selftest_profile)
            set_tests_properties(jit/selftest-profile PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_vrange tests/embed/jit_selftest_vrange.c)
            target_link_libraries(jit_selftest_vrange PRIVATE libmcc)
            target_include_directories(jit_selftest_vrange PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-vrange COMMAND jit_selftest_vrange)
            set_tests_properties(jit/selftest-vrange PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_fparg tests/embed/jit_selftest_fparg.c)
            target_link_libraries(jit_selftest_fparg PRIVATE libmcc)
            target_include_directories(jit_selftest_fparg PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-fparg
                COMMAND jit_selftest_fparg "-B${CMAKE_BINARY_DIR}"
                        "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
            set_tests_properties(jit/selftest-fparg PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_mixed tests/embed/jit_selftest_mixed.c)
            target_link_libraries(jit_selftest_mixed PRIVATE libmcc)
            target_include_directories(jit_selftest_mixed PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-mixed
                COMMAND jit_selftest_mixed "-B${CMAKE_BINARY_DIR}"
                        "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
            set_tests_properties(jit/selftest-mixed PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_qsbr tests/embed/jit_selftest_qsbr.c)
            target_link_libraries(jit_selftest_qsbr PRIVATE libmcc)
            target_include_directories(jit_selftest_qsbr PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-qsbr COMMAND jit_selftest_qsbr)
            set_tests_properties(jit/selftest-qsbr PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_shutdown tests/embed/jit_selftest_shutdown.c)
            target_link_libraries(jit_selftest_shutdown PRIVATE libmcc)
            target_include_directories(jit_selftest_shutdown PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-shutdown COMMAND jit_selftest_shutdown)
            set_tests_properties(jit/selftest-shutdown PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)
            add_test(NAME jit/selftest-shutdown-known-positive
                COMMAND jit_selftest_shutdown known-positive)
            set_tests_properties(jit/selftest-shutdown-known-positive PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)
            add_test(NAME jit/selftest-shutdown-swap-wide
                COMMAND jit_selftest_shutdown swap-wide)
            set_tests_properties(jit/selftest-shutdown-swap-wide PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)

            add_executable(jit_selftest_patch tests/embed/jit_selftest_patch.c)
            target_link_libraries(jit_selftest_patch PRIVATE libmcc)
            target_include_directories(jit_selftest_patch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-patch COMMAND jit_selftest_patch)
            set_tests_properties(jit/selftest-patch PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                ENVIRONMENT "MCC_JIT_PATCH_ITERS=300000")

            add_executable(jit_selftest_slice tests/embed/jit_selftest_slice.c)
            target_link_libraries(jit_selftest_slice PRIVATE libmcc)
            target_include_directories(jit_selftest_slice PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-slice COMMAND jit_selftest_slice)
            set_tests_properties(jit/selftest-slice PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_sliceextract tests/embed/jit_selftest_sliceextract.c)
            target_link_libraries(jit_selftest_sliceextract PRIVATE libmcc)
            target_include_directories(jit_selftest_sliceextract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-sliceextract COMMAND jit_selftest_sliceextract)
            set_tests_properties(jit/selftest-sliceextract PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_sliceoracle tests/embed/jit_selftest_sliceoracle.c)
            target_link_libraries(jit_selftest_sliceoracle PRIVATE libmcc)
            target_include_directories(jit_selftest_sliceoracle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-sliceoracle COMMAND jit_selftest_sliceoracle)
            set_tests_properties(jit/selftest-sliceoracle PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_sliceladder tests/embed/jit_selftest_sliceladder.c)
            target_link_libraries(jit_selftest_sliceladder PRIVATE libmcc)
            target_include_directories(jit_selftest_sliceladder PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-sliceladder COMMAND jit_selftest_sliceladder)
            set_tests_properties(jit/selftest-sliceladder PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_consteval tests/embed/jit_selftest_consteval.c)
            target_link_libraries(jit_selftest_consteval PRIVATE libmcc)
            target_include_directories(jit_selftest_consteval PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-consteval COMMAND jit_selftest_consteval)
            set_tests_properties(jit/selftest-consteval PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-fold-consts COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SELFTEST_FOLD_CONSTS=1 $<TARGET_FILE:jit_selftest>)
            set_tests_properties(jit/selftest-fold-consts PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_executable(jit_selftest_gated tests/embed/jit_selftest_gated.c)
            target_link_libraries(jit_selftest_gated PRIVATE libmcc)
            target_include_directories(jit_selftest_gated PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-gated COMMAND jit_selftest_gated)
            set_tests_properties(jit/selftest-gated PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_executable(jit_selftest_search tests/embed/jit_selftest_search.c)
            target_link_libraries(jit_selftest_search PRIVATE libmcc)
            target_include_directories(jit_selftest_search PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-search COMMAND jit_selftest_search)
            set_tests_properties(jit/selftest-search PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-search-live COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SEARCH=1 MCC_JIT_HOT_CALLS=2 $<TARGET_FILE:jit_selftest>)
            set_tests_properties(jit/selftest-search-live PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            if((MCC_CPU STREQUAL "x86_64" AND NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC"
                AND NOT WIN32) OR
               (MCC_CPU STREQUAL "arm64" AND NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC"
                AND NOT WIN32))
                add_test(NAME regression/o4-aot-jit COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ci/regression_o4_aot_jit.sh" "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
                set_tests_properties(regression/o4-aot-jit PROPERTIES FIXTURES_REQUIRED MCC_BUILT TIMEOUT 90)
                add_test(NAME regression/jit-submit-aot-diff COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ci/regression_jit_submit_aot_diff.sh" "$<TARGET_FILE:mcc>")
                set_tests_properties(regression/jit-submit-aot-diff PROPERTIES FIXTURES_REQUIRED MCC_BUILT TIMEOUT 90)
            else()
                mcc_skip_test(regression/o4-aot-jit "runtime-JIT override path fires+correct on x86_64/arm64 non-MSVC non-Windows; MSVC/arm64-PE/i686 gated or tracked-broken")
                mcc_skip_test(regression/jit-submit-aot-diff "runtime-JIT override path fires+correct on x86_64/arm64 non-MSVC non-Windows; MSVC/arm64-PE/i686 gated or tracked-broken")
            endif()
            add_test(NAME jit/selftest-search-live-kgc COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SEARCH=1 MCC_JIT_HOT_CALLS=2 MCC_JIT_SEARCH_MS=25 $<TARGET_FILE:jit_selftest_kgc>)
            set_tests_properties(jit/selftest-search-live-kgc PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-search-live-struct COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SEARCH=1 MCC_JIT_HOT_CALLS=2 MCC_JIT_SEARCH_MS=25 $<TARGET_FILE:jit_selftest_struct>)
            set_tests_properties(jit/selftest-search-live-struct PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-search-live-ptrret COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SEARCH=1 MCC_JIT_HOT_CALLS=2 MCC_JIT_SEARCH_MS=25 $<TARGET_FILE:jit_selftest_ptrret>)
            set_tests_properties(jit/selftest-search-live-ptrret PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-search-live-purity COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SEARCH=1 MCC_JIT_HOT_CALLS=2 MCC_JIT_SEARCH_MS=25 $<TARGET_FILE:jit_selftest_purity>)
            set_tests_properties(jit/selftest-search-live-purity PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-search-live-pool COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SEARCH=1 MCC_JIT_HOT_CALLS=2 MCC_JIT_SEARCH_MS=25 $<TARGET_FILE:jit_selftest_pool>)
            set_tests_properties(jit/selftest-search-live-pool PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            add_test(NAME jit/selftest-search-live-lazy COMMAND ${CMAKE_COMMAND} -E env MCC_JIT_SEARCH=1 MCC_JIT_HOT_CALLS=2 MCC_JIT_SEARCH_MS=25 $<TARGET_FILE:jit_selftest_lazy>)
            set_tests_properties(jit/selftest-search-live-lazy PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_slicekernel tests/embed/jit_selftest_slicekernel.c)
            target_link_libraries(jit_selftest_slicekernel PRIVATE libmcc)
            target_include_directories(jit_selftest_slicekernel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-slicekernel COMMAND jit_selftest_slicekernel)
            set_tests_properties(jit/selftest-slicekernel PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_slicereemit tests/embed/jit_selftest_slicereemit.c)
            target_link_libraries(jit_selftest_slicereemit PRIVATE libmcc)
            target_include_directories(jit_selftest_slicereemit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-slicereemit COMMAND jit_selftest_slicereemit)
            set_tests_properties(jit/selftest-slicereemit PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_sliceinstall tests/embed/jit_selftest_sliceinstall.c)
            target_link_libraries(jit_selftest_sliceinstall PRIVATE libmcc)
            target_include_directories(jit_selftest_sliceinstall PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-sliceinstall COMMAND jit_selftest_sliceinstall)
            set_tests_properties(jit/selftest-sliceinstall PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_slicesearch tests/embed/jit_selftest_slicesearch.c)
            target_link_libraries(jit_selftest_slicesearch PRIVATE libmcc)
            target_include_directories(jit_selftest_slicesearch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-slicesearch COMMAND jit_selftest_slicesearch)
            set_tests_properties(jit/selftest-slicesearch PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_slicelive tests/embed/jit_selftest_slicelive.c)
            target_link_libraries(jit_selftest_slicelive PRIVATE libmcc)
            target_include_directories(jit_selftest_slicelive PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-slice-live COMMAND jit_selftest_slicelive)
            set_tests_properties(jit/selftest-slice-live PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_noescape tests/embed/jit_selftest_noescape.c)
            target_link_libraries(jit_selftest_noescape PRIVATE libmcc)
            target_include_directories(jit_selftest_noescape PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-noescape COMMAND jit_selftest_noescape)
            set_tests_properties(jit/selftest-noescape PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_evalgate tests/embed/jit_selftest_evalgate.c)
            target_link_libraries(jit_selftest_evalgate PRIVATE libmcc)
            target_include_directories(jit_selftest_evalgate PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-evalgate COMMAND jit_selftest_evalgate)
            set_tests_properties(jit/selftest-evalgate PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_l4a tests/embed/jit_selftest_l4a.c)
            target_link_libraries(jit_selftest_l4a PRIVATE libmcc)
            target_include_directories(jit_selftest_l4a PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME jit/selftest-l4a COMMAND jit_selftest_l4a)
            set_tests_properties(jit/selftest-l4a PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

            add_executable(jit_selftest_benchwire tests/embed/jit_selftest_benchwire.c)
            target_link_libraries(jit_selftest_benchwire PRIVATE libmcc)
            target_include_directories(jit_selftest_benchwire PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
            # Drives the JIT fault injectors (MCC_JIT_FORCE_INFEASIBLE /
            # MCC_JIT_BENCH*), which compile out of libmcc unless MCC_DEV. Without
            # it the injector is inert and the cell would pass vacuously.
            if(MCC_DEV)
                add_test(NAME jit/selftest-benchwire COMMAND jit_selftest_benchwire)
                set_tests_properties(jit/selftest-benchwire PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test(jit/selftest-benchwire "requires -DMCC_DEV=ON (JIT fault injection)")
            endif()

            if(MCC_BUILD_STATIC_LIB AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
                add_test(NAME jit/standalone-static
                    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/jit_standalone.sh"
                            "$<TARGET_FILE:mcc>" "${CMAKE_BINARY_DIR}"
                            "$<TARGET_FILE:libmcc>"
                            "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
                            "${CMAKE_BINARY_DIR}")
                set_tests_properties(jit/standalone-static PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
            else()
                mcc_skip_test(jit/standalone-static
                    "links libmcc.a into a standalone JIT host: needs -DMCC_BUILD_STATIC_LIB=ON on a Linux target")
            endif()

            # The JIT must not change what a program computes. Each case runs the
            # same --embed-jit binary twice, MCC_JIT=1 against MCC_JIT=0, so the
            # AOT body is the control. Catches the class where the runtime
            # re-emit and the parser disagree -- at compile time the faithfulness
            # gate hides that by falling back, at runtime nothing does.
            add_test(NAME jit/replay-parity
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/jit_replay_parity.sh"
                        "$<TARGET_FILE:mcc>" "${CMAKE_BINARY_DIR}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
                        "${CMAKE_BINARY_DIR}")
            set_tests_properties(jit/replay-parity PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "smoke" TIMEOUT 600)

            if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND MCC_CPU STREQUAL "arm64")
                foreach(_jt leaf-int kgc purity struct liverun poison fparg
                            reemit-gates fold-consts search-live search-live-kgc
                            search-live-struct search-live-purity)
                    set_tests_properties(jit/selftest-${_jt} PROPERTIES DISABLED TRUE)
                endforeach()
            endif()
        else()
            set(_jitoff_why "the Tier-B runtime-JIT engine is not linked into this build; configure with -DMCC_EMBED_JIT=ON")
            mcc_skip_test(jit/replay-parity "${_jitoff_why}")
            mcc_skip_test(jit/selftest-leaf-int "${_jitoff_why}")
            mcc_skip_test(jit/selftest-reemit-gates "${_jitoff_why}")
            mcc_skip_test(jit/selftest-stage2 "${_jitoff_why}")
            mcc_skip_test(jit/selftest-kgc "${_jitoff_why}")
            mcc_skip_test(jit/selftest-nearmatch "${_jitoff_why}")
            mcc_skip_test(jit/selftest-purity "${_jitoff_why}")
            mcc_skip_test(jit/selftest-purity-known-positive "${_jitoff_why}")
            mcc_skip_test(jit/selftest-strlit "${_jitoff_why}")
            mcc_skip_test(jit/selftest-struct "${_jitoff_why}")
            mcc_skip_test(jit/selftest-ptrret "${_jitoff_why}")
            mcc_skip_test(jit/selftest-lazy "${_jitoff_why}")
            mcc_skip_test(jit/selftest-pool "${_jitoff_why}")
            mcc_skip_test(jit/selftest-eligibility "${_jitoff_why}")
            mcc_skip_test(jit/selftest-submit "${_jitoff_why}")
            mcc_skip_test(jit/selftest-fork "${_jitoff_why}")
            mcc_skip_test(jit/selftest-observability "${_jitoff_why}")
            mcc_skip_test(jit/selftest-liverun "${_jitoff_why}")
            mcc_skip_test(jit/selftest-poison "${_jitoff_why}")
            mcc_skip_test(jit/selftest-bench "${_jitoff_why}")
            mcc_skip_test(jit/selftest-profile "${_jitoff_why}")
            mcc_skip_test(jit/selftest-vrange "${_jitoff_why}")
            mcc_skip_test(jit/selftest-fparg "${_jitoff_why}")
            mcc_skip_test(jit/selftest-mixed "${_jitoff_why}")
            mcc_skip_test(jit/selftest-qsbr "${_jitoff_why}")
            mcc_skip_test(jit/selftest-shutdown "${_jitoff_why}")
            mcc_skip_test(jit/selftest-shutdown-known-positive "${_jitoff_why}")
            mcc_skip_test(jit/selftest-shutdown-swap-wide "${_jitoff_why}")
            mcc_skip_test(jit/selftest-patch "${_jitoff_why}")
            mcc_skip_test(jit/selftest-slice "${_jitoff_why}")
            mcc_skip_test(jit/selftest-sliceextract "${_jitoff_why}")
            mcc_skip_test(jit/selftest-sliceoracle "${_jitoff_why}")
            mcc_skip_test(jit/selftest-sliceladder "${_jitoff_why}")
            mcc_skip_test(jit/selftest-consteval "${_jitoff_why}")
            mcc_skip_test(jit/selftest-fold-consts "${_jitoff_why}")
            mcc_skip_test(jit/selftest-gated "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search-live "${_jitoff_why}")
            mcc_skip_test(regression/o4-aot-jit "${_jitoff_why}")
            mcc_skip_test(regression/jit-submit-aot-diff "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search-live-kgc "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search-live-struct "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search-live-ptrret "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search-live-purity "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search-live-pool "${_jitoff_why}")
            mcc_skip_test(jit/selftest-search-live-lazy "${_jitoff_why}")
            mcc_skip_test(jit/selftest-slicekernel "${_jitoff_why}")
            mcc_skip_test(jit/selftest-slicereemit "${_jitoff_why}")
            mcc_skip_test(jit/selftest-sliceinstall "${_jitoff_why}")
            mcc_skip_test(jit/selftest-slicesearch "${_jitoff_why}")
            mcc_skip_test(jit/selftest-slice-live "${_jitoff_why}")
            mcc_skip_test(jit/selftest-noescape "${_jitoff_why}")
            mcc_skip_test(jit/selftest-evalgate "${_jitoff_why}")
            mcc_skip_test(jit/selftest-l4a "${_jitoff_why}")
            mcc_skip_test(jit/selftest-benchwire "${_jitoff_why}")
            mcc_skip_test(jit/standalone-static "${_jitoff_why}")
        endif()

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            foreach(_ja dispatch counter kgc kgcfp)
                add_test(NAME jit/arm64-${_ja}
                    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/jit_arm64_${_ja}.sh"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/qemu/jit_arm64_${_ja}.c"
                            "${CMAKE_BINARY_DIR}")
                set_tests_properties(jit/arm64-${_ja} PROPERTIES SKIP_RETURN_CODE 77)
            endforeach()
        endif()

        add_executable(libmcc_test_extra tests/embed/api_extra.c)
        target_link_libraries(libmcc_test_extra PRIVATE libmcc)
        target_include_directories(libmcc_test_extra PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME libtest-extra
            COMMAND libmcc_test_extra "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(libtest-extra PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        add_executable(mcc_selftest_surrogate tests/embed/mcc_selftest_surrogate.c)
        target_include_directories(mcc_selftest_surrogate PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME opt/selftest-surrogate COMMAND mcc_selftest_surrogate)

        # api_threaded.c drives mcc_setjmp/mcc_set_backtrace_func, which
        # mccrun.c only compiles when the C-level MCC_TARGET_IS_HOST holds.
        # The pointer-size check above cannot see an x86_64 compiler
        # configured for an arm64 target (the Rosetta dist stage2, where
        # mcc ignores clang's -arch), so ask the compiler itself whether it
        # targets the configured MCC_CPU/MCC_TARGETOS.
        include(CheckCSourceCompiles)
        if(MCC_CPU STREQUAL "i386")
            set(_mcc_run_api_arch "defined __i386__")
        elseif(MCC_CPU STREQUAL "x86_64")
            set(_mcc_run_api_arch "defined __x86_64__")
        elseif(MCC_CPU STREQUAL "arm")
            set(_mcc_run_api_arch "defined __arm__")
        elseif(MCC_CPU STREQUAL "arm64")
            set(_mcc_run_api_arch "defined __aarch64__")
        else()
            set(_mcc_run_api_arch "defined __riscv && defined __LP64__")
        endif()
        if(MCC_TARGETOS STREQUAL "WIN32")
            set(_mcc_run_api_os "defined _WIN32")
        elseif(MCC_TARGETOS STREQUAL "Darwin")
            set(_mcc_run_api_os "defined __APPLE__")
        else()
            set(_mcc_run_api_os "!defined _WIN32 && !defined __APPLE__")
        endif()
        check_c_source_compiles("
#if !(${_mcc_run_api_arch}) || !(${_mcc_run_api_os})
#error the compiler does not target the configured MCC_CPU/MCC_TARGETOS
#endif
int main(void) { return 0; }" MCC_RUN_API_IS_HOST)
        if(MCC_RUN_API_IS_HOST AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/api_threaded.c")
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
        elseif(NOT MCC_RUN_API_IS_HOST)
            mcc_skip_test(libtest-mt "the compiler does not target ${MCC_CPU}/${MCC_TARGETOS}: libmcc will not export the -run API (mccrun.c is MCC_TARGET_IS_HOST-gated)")
        endif()
    else()
        mcc_skip_test(libtest "mcc targets ${MCC_CPU} but runs on a ${CMAKE_SIZEOF_VOID_P}-byte host: the JIT run API (mcc_run/mcc_relocate) is compiled out")
        mcc_skip_test(libtest-extra "mcc targets ${MCC_CPU} but runs on a ${CMAKE_SIZEOF_VOID_P}-byte host: the JIT run API (mcc_run/mcc_relocate) is compiled out")
        mcc_skip_test(libtest-mt "mcc targets ${MCC_CPU} but runs on a ${CMAKE_SIZEOF_VOID_P}-byte host: the JIT run API (mcc_run/mcc_relocate) is compiled out")
    endif()

    if(MCC_TARGET_IS_HOST AND NOT MCC_TARGETOS STREQUAL "WIN32")
        add_executable(mcchv tools/mcchv.c)
        target_link_libraries(mcchv PRIVATE libmcc m)
        target_include_directories(mcchv PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(mcchv PRIVATE Threads::Threads)
        endif()
        add_test(NAME hypervisor-smoke
            COMMAND mcchv "-B${CMAKE_BINARY_DIR}" --seed 42 --passes 4 --workers 4)
        set_tests_properties(hypervisor-smoke PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77)
        add_test(NAME hypervisor-task
            COMMAND mcchv "-B${CMAKE_BINARY_DIR}" --seed 42 --passes 2 --workers 2)
        set_tests_properties(hypervisor-task PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77
            PASS_REGULAR_EXPRESSION "hv: optimizer task: parked-on=hv_npat ")
        add_test(NAME hypervisor-search
            COMMAND mcchv "-B${CMAKE_BINARY_DIR}" --seed 42 --passes 3 --workers 2 --seconds 1.2)
        set_tests_properties(hypervisor-search PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77
            ENVIRONMENT "MCCHV_CACHE_DIR=${CMAKE_BINARY_DIR}/hvcache")
        add_test(NAME hypervisor-tpe
            COMMAND mcchv "-B${CMAKE_BINARY_DIR}" --seed 42 --passes 3 --workers 2 --seconds 1.2 --search tpe)
        set_tests_properties(hypervisor-tpe PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77
            ENVIRONMENT "MCCHV_CACHE_DIR=${CMAKE_BINARY_DIR}/hvcache-tpe")
        if(TARGET mccharness)
            add_test(NAME hypervisor-cache
                COMMAND mccharness hvcache --mcchv "$<TARGET_FILE:mcchv>" --bdir "${CMAKE_BINARY_DIR}")
            set_tests_properties(hypervisor-cache PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77)
                add_test(NAME superopt-perfn-cache
                    COMMAND mccharness perfncache --mcc "${_mcc_exe}" --bdir "${CMAKE_BINARY_DIR}")
                set_tests_properties(superopt-perfn-cache PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
                add_test(NAME ast-cprop-join
                    COMMAND mccharness cpropjoin --mcc "${_mcc_exe}" --bdir "${CMAKE_BINARY_DIR}")
                set_tests_properties(ast-cprop-join PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)
        else()
            mcc_skip_test(hypervisor-cache "requires the host mccharness driver (native build)")
                mcc_skip_test(superopt-perfn-cache "requires the host mccharness driver (native build)")
                mcc_skip_test(ast-cprop-join "requires the host mccharness driver (native build)")
        endif()
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
        if(MCC_CC_NAME STREQUAL "mcc" AND MCC_CPU STREQUAL "x86_64")
            mcc_skip_test(static-glibc-run "x86_64 self-host: mcc-built mccrt lacks __unordtf2/__letf2 (no __float128); a static TF-helper link is unresolved")
        elseif(NOT MCC_EMULATOR AND NOT MCC_BUILD_MUSL
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
            if(NOT MCC_BUILD_MUSL AND NOT MCC_CC_NAME STREQUAL "mcc")
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

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/tls/shared.c")
        if(MCC_CPU STREQUAL "arm64" AND NOT MCC_TARGETOS STREQUAL "WIN32"
           AND NOT MCC_TARGETOS STREQUAL "Darwin")
            add_test(NAME tls-shared COMMAND "${CMAKE_COMMAND}"
                "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/tls/shared.c"
                "-DOUT=${CMAKE_BINARY_DIR}/tls-shared.so"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/tls/run_shared.cmake")
            set_tests_properties(tls-shared PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(tls-shared "tls-shared gate: arm64 ELF only; dynamic TLS is unimplemented on arm and riscv64, and Mach-O/PE reach the thread block through their own loaders")
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/vla.c")
        add_test(NAME vla_test-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/vla.c
            --out ${CMAKE_BINARY_DIR}/vla_test${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(vla_test-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bool_weak_addr.c")
        add_test(NAME bool-weak-addr-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bool_weak_addr.c
            --out ${CMAKE_BINARY_DIR}/bool_weak_addr${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(bool-weak-addr-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_bool_store.c")
        add_test(NAME bitint-bool-store-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_bool_store.c
            --out ${CMAKE_BINARY_DIR}/bitint_bool_store${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(bitint-bool-store-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/switch_bool_promote.c")
        add_test(NAME switch-bool-promote-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/switch_bool_promote.c
            --out ${CMAKE_BINARY_DIR}/switch_bool_promote${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(switch-bool-promote-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/switch_bitint_wide.c")
        add_test(NAME switch-bitint-wide-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/switch_bitint_wide.c
            --out ${CMAKE_BINARY_DIR}/switch_bitint_wide${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(switch-bitint-wide-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/typeof_bitint.c")
        add_test(NAME typeof-bitint-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/typeof_bitint.c
            --out ${CMAKE_BINARY_DIR}/typeof_bitint${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(typeof-bitint-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_bit_builtins.c")
        add_test(NAME bitint-bit-builtins-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_bit_builtins.c
            --out ${CMAKE_BINARY_DIR}/bitint_bit_builtins${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(bitint-bit-builtins-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_cond_const.c")
        add_test(NAME bitint-cond-const-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_cond_const.c
            --out ${CMAKE_BINARY_DIR}/bitint_cond_const${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(bitint-cond-const-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_arith_result_type.c")
        add_test(NAME bitint-arith-result-type-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_arith_result_type.c
            --out ${CMAKE_BINARY_DIR}/bitint_arith_result_type${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(bitint-arith-result-type-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/concat_narrow_wide_utf8.c")
        add_test(NAME concat-narrow-wide-utf8-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/concat_narrow_wide_utf8.c
            --out ${CMAKE_BINARY_DIR}/concat_narrow_wide_utf8${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(concat-narrow-wide-utf8-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_int128_signext.c")
        add_test(NAME bitint-int128-signext-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/bitint_int128_signext.c
            --out ${CMAKE_BINARY_DIR}/bitint_int128_signext${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(bitint-int128-signext-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/ftoi_fold_saturate.c")
        add_test(NAME ftoi-fold-saturate-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/ftoi_fold_saturate.c
            --out ${CMAKE_BINARY_DIR}/ftoi_fold_saturate${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(ftoi-fold-saturate-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/sizeof_long_double.c")
        add_test(NAME sizeof-long-double-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/sizeof_long_double.c
            --out ${CMAKE_BINARY_DIR}/sizeof_long_double${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(sizeof-long-double-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/sizeof_wint_t.c")
        add_test(NAME sizeof-wint-t-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/sizeof_wint_t.c
            --out ${CMAKE_BINARY_DIR}/sizeof_wint_t${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(sizeof-wint-t-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/alignof_vla_noeval.c")
        add_test(NAME alignof-vla-noeval-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/alignof_vla_noeval.c
            --out ${CMAKE_BINARY_DIR}/alignof_vla_noeval${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(alignof-vla-noeval-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/ld_arith_type.c")
        add_test(NAME ld-arith-type-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/ld_arith_type.c
            --out ${CMAKE_BINARY_DIR}/ld_arith_type${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(ld-arith-type-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/enum_u64_underlying.c")
        add_test(NAME enum-u64-underlying-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/enum_u64_underlying.c
            --out ${CMAKE_BINARY_DIR}/enum_u64_underlying${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(enum-u64-underlying-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/labeladdr_filescope.c")
        add_test(NAME "diag/labeladdr-filescope-rejected" COMMAND ${_mcc_exe} -c
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/labeladdr_filescope.c"
            -o "${CMAKE_BINARY_DIR}/labeladdr_filescope.o")
        set_tests_properties("diag/labeladdr-filescope-rejected" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT WILL_FAIL TRUE)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/labeladdr_infunc.c")
        add_test(NAME labeladdr-infunc-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/labeladdr_infunc.c
            --out ${CMAKE_BINARY_DIR}/labeladdr_infunc${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(labeladdr-infunc-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(MCC_CPU STREQUAL "arm64"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_align_p2_arm64.S")
        add_test(NAME asm-align-p2-arm64-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_align_p2_arm64.S ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_align_p2_check.c"
            --out ${CMAKE_BINARY_DIR}/asm_align_p2_arm64${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(asm-align-p2-arm64-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(MCC_CPU STREQUAL "arm64"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_word_arm64.S")
        add_test(NAME asm-word-arm64-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_word_arm64.S ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_word_arm64_check.c"
            --out ${CMAKE_BINARY_DIR}/asm_word_arm64${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(asm-word-arm64-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(MCC_CPU STREQUAL "arm64"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_word_arm64_sym.s")
        add_test(NAME asm-word-arm64-sym COMMAND ${_mcc_exe} -c
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/asm_word_arm64_sym.s"
            -o "${CMAKE_BINARY_DIR}/asm_word_arm64_sym.o")
        set_tests_properties(asm-word-arm64-sym PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/atomic_aggregate_align.c")
        add_test(NAME atomic-aggregate-align-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/atomic_aggregate_align.c
            --out ${CMAKE_BINARY_DIR}/atomic_aggregate_align${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(atomic-aggregate-align-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/u16_hex_escape.c")
        add_test(NAME u16-hex-escape-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/u16_hex_escape.c
            --out ${CMAKE_BINARY_DIR}/u16_hex_escape${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(u16-hex-escape-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/u32_no_surrogate_combine.c")
        add_test(NAME u32-no-surrogate-combine-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/u32_no_surrogate_combine.c
            --out ${CMAKE_BINARY_DIR}/u32_no_surrogate_combine${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(u32-no-surrogate-combine-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/has_cpp_attribute_c.c")
        add_test(NAME has-cpp-attribute-c COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/has_cpp_attribute_c.c"
                    -o "${CMAKE_BINARY_DIR}/has_cpp_attribute_c.o")
        set_tests_properties(has-cpp-attribute-c PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/has_builtin_name_recognized.c")
        add_test(NAME has-builtin-name-recognized COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/has_builtin_name_recognized.c"
                    -o "${CMAKE_BINARY_DIR}/has_builtin_name_recognized.o")
        set_tests_properties(has-builtin-name-recognized PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/semantic_attrs_accepted.c")
        add_test(NAME semantic-attrs-accepted COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/semantic_attrs_accepted.c")
        set_tests_properties(semantic-attrs-accepted PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/codegen_hint_attrs.c")
        add_test(NAME codegen-hint-attrs COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/codegen_hint_attrs.c")
        set_tests_properties(codegen-hint-attrs PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    # T-mac-30181: discarding the result of a warn_unused_result function warns
    # (-Wunused-result); T-mac-30183: a null constant passed where a nonnull
    # parameter is required warns (-Wnonnull), and a variadic sentinel call whose
    # last argument is not a null pointer constant warns (-Wsentinel). The clean
    # cell proves non-vacuity: correct usage stays silent under -Wall -Werror.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/unused_result_warn.c")
        add_test(NAME "diag/unused-result" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/unused_result_warn.c"
                    -o "${CMAKE_BINARY_DIR}/unused-result-warn.o")
        set_tests_properties("diag/unused-result" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "ignoring return value of 'f' declared with attribute 'warn_unused_result'")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/nonnull_warn.c")
        add_test(NAME "diag/nonnull" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/nonnull_warn.c"
                    -o "${CMAKE_BINARY_DIR}/nonnull-warn.o")
        set_tests_properties("diag/nonnull" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "argument 1 null where non-null expected")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/sentinel_warn.c")
        add_test(NAME "diag/sentinel" COMMAND ${_mcc_exe} -c
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/sentinel_warn.c"
                    -o "${CMAKE_BINARY_DIR}/sentinel-warn.o")
        set_tests_properties("diag/sentinel" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "missing sentinel in function call")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/nonnull_sentinel_clean.c")
        add_test(NAME "diag/nonnull-sentinel-clean" COMMAND ${_mcc_exe} -Wall -Werror -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/nonnull_sentinel_clean.c")
        set_tests_properties("diag/nonnull-sentinel-clean" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/xpreprocessor_forward.c")
        add_test(NAME driver/xpreprocessor-forward COMMAND ${_mcc_exe}
                    -Xpreprocessor -DFOO=41 -run
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/xpreprocessor_forward.c")
        set_tests_properties(driver/xpreprocessor-forward PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        add_test(NAME driver/save-temps-accepted COMMAND ${_mcc_exe} -save-temps -Xassembler --noexecstack
                    -c "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/xpreprocessor_forward.c"
                    -DFOO=1 -o "${CMAKE_BINARY_DIR}/xpp-fwd.o")
        set_tests_properties(driver/save-temps-accepted PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        # T-mac-30227 slice-1: stdin input is named <stdin> (not -) in __FILE__
        # and -E line markers, matching gcc/clang.
        add_test(NAME driver/stdin-filename
            COMMAND sh -c "printf 'int x;\n' | '$<TARGET_FILE:mcc>' -E -")
        set_tests_properties(driver/stdin-filename PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT PASS_REGULAR_EXPRESSION "\"<stdin>\"")

        # T-mac-30227 residual: the initial synthetic -E marker uses line 0
        # for stdin, matching gcc (# 0 "<stdin>").
        add_test(NAME driver/stdin-initial-marker
            COMMAND sh -c "printf 'int x;\n' | '$<TARGET_FILE:mcc>' -E -")
        set_tests_properties(driver/stdin-initial-marker PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT PASS_REGULAR_EXPRESSION "# 0 \"<stdin>\"")

        # T-mac-30227 residual: gcc-matching synthetic preamble markers
        # # 0 "<built-in>" and # 0 "<command-line>" (hyphen spelling, line 0,
        # no include-level flag).
        add_test(NAME driver/stdin-builtin-marker
            COMMAND sh -c "printf 'int x;\n' | '$<TARGET_FILE:mcc>' -E -")
        set_tests_properties(driver/stdin-builtin-marker PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT PASS_REGULAR_EXPRESSION "# 0 \"<built-in>\"")

        add_test(NAME driver/stdin-cmdline-marker
            COMMAND sh -c "printf 'int x;\n' | '$<TARGET_FILE:mcc>' -E -")
        set_tests_properties(driver/stdin-cmdline-marker PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "# 0 \"<command-line>\""
            FAIL_REGULAR_EXPRESSION "<command line>\"")

        # T-mac-30227 residual regression guard: a named file's real-content
        # marker stays line 1 (# 1 "<name>"), never regressed to # 0.
        add_test(NAME driver/named-content-marker
            COMMAND "$<TARGET_FILE:mcc>" -E
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/emarker_named.c")
        set_tests_properties(driver/named-content-marker PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "# 1 \"[^\"]*emarker_named\\.c\"")

        # T-mac-30227 residual: a named input gets the same gcc-matching
        # # 0 "<command-line>" preamble as stdin (hyphen spelling, line 0).
        add_test(NAME driver/named-cmdline-marker
            COMMAND "$<TARGET_FILE:mcc>" -E
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/emarker_named.c")
        set_tests_properties(driver/named-cmdline-marker PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "# 0 \"<command-line>\""
            FAIL_REGULAR_EXPRESSION "<command line>\"")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/alignof_vla.c")
        add_test(NAME alignof-vla-run COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcs ${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/alignof_vla.c
            --out ${CMAKE_BINARY_DIR}/alignof_vla${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(alignof-vla-run PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/c99inline_a.c")
        add_test(NAME c99-inline-body-2tu COMMAND "$<TARGET_FILE:mccharness>" mccexe
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --cflags "-fc99-inline-body"
            --srcs "${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/c99inline_a.c ${CMAKE_CURRENT_SOURCE_DIR}/tests/behavior/c99inline_b.c"
            --out ${CMAKE_BINARY_DIR}/c99inline_2tu${MCC_EXESUF} --emu "${MCC_EMULATOR}")
        set_tests_properties(c99-inline-body-2tu PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(MCC_TARGET_IS_HOST AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/embed/abi.c")
        add_executable(abitest tests/embed/abi.c)
        target_link_libraries(abitest PRIVATE libmcc)
        target_include_directories(abitest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME abitest-cc
            COMMAND abitest "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(abitest-cc PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endif()

    if(MCC_TARGETOS STREQUAL "WIN32" AND MCC_TARGET_IS_HOST AND NOT MCC_EMULATOR
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/pe/shortimport_main.c")
        if(MCC_CPU STREQUAL "x86_64")
            set(_shortimp_mach 8664)
        elseif(MCC_CPU STREQUAL "arm64")
            set(_shortimp_mach AA64)
        elseif(MCC_CPU STREQUAL "i386")
            set(_shortimp_mach 014c)
        else()
            set(_shortimp_mach "")
        endif()
        if(_shortimp_mach)
            add_executable(mkshortimp tools/mkshortimp.c)
            add_test(NAME pe/short-import COMMAND "${CMAKE_COMMAND}"
                "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}"
                "-DGEN=$<TARGET_FILE:mkshortimp>" "-DMACHINE=${_shortimp_mach}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/pe/shortimport_main.c"
                "-DOUT=${CMAKE_BINARY_DIR}/pe_short_import${MCC_EXESUF}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/pe/run_shortimport.cmake")
            set_tests_properties(pe/short-import PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(pe/short-import "short-import gate: x86_64/arm64 PE host only")
        endif()
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
            set(_ref_want "")
            if(MCC_CPU STREQUAL "arm64")
                set(_ref_want "${_MCC_MINGW_ARM64_GCC}")
                set(_ref_want_triplet "aarch64")
            elseif(MCC_CPU STREQUAL "i386")
                set(_ref_want "${_MCC_MINGW_I686_GCC}")
                set(_ref_want_triplet "i686|i386")
            else()
                set(_ref_want "${_MCC_MINGW_X86_64_GCC}")
                set(_ref_want_triplet "x86_64|amd64")
            endif()
            if(EXISTS "${_ref_want}")
                set(_ref_cc "${_ref_want}")
            else()
                find_program(_mcc_ref_gcc NAMES gcc)
                set(_ref_cc "")
                if(_mcc_ref_gcc)
                    execute_process(COMMAND "${_mcc_ref_gcc}" -dumpmachine
                        OUTPUT_VARIABLE _ref_triplet ERROR_QUIET
                        OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _ref_trc)
                    if(_ref_trc EQUAL 0 AND _ref_triplet MATCHES "${_ref_want_triplet}")
                        set(_ref_cc "${_mcc_ref_gcc}")
                    else()
                        message(STATUS
                            "mcc: ignoring reference cc ${_mcc_ref_gcc} (-dumpmachine "
                            "'${_ref_triplet}' does not target ${MCC_CPU}); a "
                            "wrong-arch reference is a wrong answer, not a missing one")
                    endif()
                endif()
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux"
               AND CMAKE_C_COMPILER_ID MATCHES "Clang")

            find_program(_mcc_ref_gcc NAMES gcc)
            if(_mcc_ref_gcc)
                set(_ref_cc "${_mcc_ref_gcc}")
            else()
                set(_ref_cc "${CMAKE_C_COMPILER}")
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
    # the refflags below force the reference gcc to GNU bit-field mode on PE;
    # since 8d4f0a80 mcc defaults ms_bitfields there, so the mcc side of the
    # differential must opt back too or the pairing compares two ABIs
    set(_mcctest_mccargs "")
    if(MCC_TARGETOS STREQUAL "WIN32")
        set(_mcctest_mccargs "-mno-ms-bitfields")

        set(_mcctest_refflags "-mno-ms-bitfields -D__USE_MINGW_ANSI_STDIO=0")
        if(MCC_CPU STREQUAL "i386")
            string(APPEND _mcctest_refflags " -fexcess-precision=standard")
        endif()
    elseif(MCC_CPU STREQUAL "arm")
        set(_mcctest_refflags "-marm")
    elseif(MCC_CPU STREQUAL "i386")
        set(_mcctest_refflags "-fno-PIC -fno-PIE -Wl,-z,notext")
    endif()
    if(NOT _ref_cc)
        mcc_skip_test(mcctest "needs a GCC-compatible reference cc (host cc is ${CMAKE_C_COMPILER_ID}; set MCC_REF_CC or put gcc on PATH)")
        mcc_skip_test(mcctest-bcheck "needs a GCC-compatible reference cc (host cc is ${CMAKE_C_COMPILER_ID}; set MCC_REF_CC or put gcc on PATH)")
        mcc_skip_test(mcctest-embedjit "needs a GCC-compatible reference cc (host cc is ${CMAKE_C_COMPILER_ID}; set MCC_REF_CC or put gcc on PATH)")
    elseif(_ref_cc_name STREQUAL "gcc" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")

        mcc_skip_test(mcctest
            "GNU gcc reference on Darwin/arm64 diverges from clang on impl-defined/UB ABI corners (covered by the Apple-clang mcctest + diff3-suite)")
        mcc_skip_test(mcctest-bcheck
            "GNU gcc reference on Darwin/arm64 diverges from clang on impl-defined/UB ABI corners (covered by the Apple-clang mcctest + diff3-suite)")
        mcc_skip_test(mcctest-embedjit
            "GNU gcc reference on Darwin/arm64 diverges from clang on impl-defined/UB ABI corners (covered by the Apple-clang mcctest + diff3-suite)")
    elseif(MCC_TARGETOS STREQUAL "WIN32" AND MCC_CPU STREQUAL "arm64")
        mcc_skip_test(mcctest "no arm64-Windows gcc reference matches the PE differential: the only reference is an emulated x86_64 mingw (UCRT printf + x86-only asm_test), which cannot be byte-identical to native arm64 mcc on legacy msvcrt (mcc arm64 codegen is covered by the exec/* goldens + pe-native-conformance; full_language differential runs on x86/x64 Windows)")
        mcc_skip_test(mcctest-bcheck "no arm64-Windows gcc reference matches the PE differential (emulated x86_64 mingw vs native arm64 mcc; see mcctest)")
        mcc_skip_test(mcctest-embedjit "no arm64-Windows gcc reference matches the PE differential (emulated x86_64 mingw vs native arm64 mcc; see mcctest)")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c")
        add_test(NAME mcctest COMMAND "$<TARGET_FILE:mccharness>" mcctest
            --cc ${_ref_cc} --mcc ${_mcc_exe}
            --src ${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c
            --srcdir ${CMAKE_CURRENT_SOURCE_DIR} --bdir ${_bdir} --idir ${_idir}
            --work ${CMAKE_BINARY_DIR}
            --refflags "${_mcctest_refflags}"
            --testdefs "-DCC_NAME=CC_${_ref_cc_name} -DGCC_MAJOR=${_ref_cc_major}"
            --mccargs "${_mcctest_mccargs}"
            --emu "${MCC_EMULATOR}")
        set_tests_properties(mcctest PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77)
        # The same differential, built with --embed-jit, so the runtime hot-swap
        # is in the loop. mcctest above builds without it and therefore cannot
        # see a defect that only the JIT's re-emit introduces -- isid() in
        # full_language.c is exactly that shape.
        if(MCC_EMBED_JIT)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/embedjit")
            add_test(NAME mcctest-embedjit COMMAND "$<TARGET_FILE:mccharness>" mcctest
                --cc ${_ref_cc} --mcc ${_mcc_exe}
                --src ${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c
                --srcdir ${CMAKE_CURRENT_SOURCE_DIR} --bdir ${_bdir} --idir ${_idir}
                --work ${CMAKE_BINARY_DIR}/embedjit
                --refflags "${_mcctest_refflags}"
                --testdefs "-DCC_NAME=CC_${_ref_cc_name} -DGCC_MAJOR=${_ref_cc_major}"
                --mccargs "--embed-jit ${_mcctest_mccargs}" --emu "${MCC_EMULATOR}")
            set_tests_properties(mcctest-embedjit PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77)
            if(NOT MCC_EMULATOR AND NOT CMAKE_CROSSCOMPILING)
                add_test(NAME jit/embed-nobake-warn COMMAND "${CMAKE_COMMAND}"
                    -DMCC=${_mcc_exe}
                    -DWORK=${CMAKE_BINARY_DIR}/embedjit
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embedjit_nobake_warn.cmake")
                set_tests_properties(jit/embed-nobake-warn PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
                if(MCC_TARGETOS STREQUAL "WIN32")
                    add_test(NAME jit/embed-libc-override COMMAND "${CMAKE_COMMAND}"
                        -DMCC=${_mcc_exe}
                        -DWORK=${CMAKE_BINARY_DIR}/embedjit
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embedjit_libc_override_link.cmake")
                    set_tests_properties(jit/embed-libc-override PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
                    add_test(NAME jit/embed-recursion-nofastfail COMMAND "${CMAKE_COMMAND}"
                        -DMCC=${_mcc_exe}
                        -DWORK=${CMAKE_BINARY_DIR}/embedjit
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embedjit_recursion_nofastfail.cmake")
                    set_tests_properties(jit/embed-recursion-nofastfail PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
                endif()
            endif()
        else()
            mcc_skip_test(mcctest-embedjit "the embedded JIT is not built into this configuration (needs -DMCC_EMBED_JIT=ON)")
        endif()
        if(MCC_TARGETOS STREQUAL "WIN32")
            mcc_skip_test(mcctest-bcheck "mcc bounds-checking is unsupported on the PE/msvcrt target (faults in msvcrt callbacks/library calls)")
        else()
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bcheck")
            add_test(NAME mcctest-bcheck COMMAND "$<TARGET_FILE:mccharness>" mcctest
                --cc ${_ref_cc} --mcc ${_mcc_exe}
                --src ${CMAKE_CURRENT_SOURCE_DIR}/tests/diff/full_language.c
                --srcdir ${CMAKE_CURRENT_SOURCE_DIR} --bdir ${_bdir} --idir ${_idir}
                --work ${CMAKE_BINARY_DIR}/bcheck
                --refflags "${_mcctest_refflags}"
                --testdefs "-DCC_NAME=CC_${_ref_cc_name} -DGCC_MAJOR=${_ref_cc_major}"
                --mccargs -b --emu "${MCC_EMULATOR}")
            set_tests_properties(mcctest-bcheck PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77)
        endif()
    else()
        mcc_skip_test(mcctest "tests/diff/full_language.c is missing from this tree")
        mcc_skip_test(mcctest-bcheck "tests/diff/full_language.c is missing from this tree")
        mcc_skip_test(mcctest-embedjit "tests/diff/full_language.c is missing from this tree")
    endif()

    # abi/vector-layout -- the only cell in the tree that compiles a 32- or
    # 64-byte vector at all. Both probes must RUN on this host (each prints the
    # offsetof/sizeof its own compiler baked in), so it needs a native,
    # unemulated build with a reference cc.
    if(_ref_cc AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR)
        add_test(NAME abi/vector-layout
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/vector-abi-layout.sh"
                    "${_mcc_exe}" "${_ref_cc}"
                    "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/vector-abi-layout")
        set_tests_properties(abi/vector-layout PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 120)
        add_test(NAME abi/vector-layout-known-positive
            COMMAND "${CMAKE_COMMAND}" -E env "VECABI_MUTATE=1"
                    sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/vector-abi-layout.sh"
                    "${_mcc_exe}" "${_ref_cc}"
                    "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/vector-abi-layout-kp")
        set_tests_properties(abi/vector-layout-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 120)
    else()
        mcc_skip_test(abi/vector-layout
            "needs a reference cc and a native unemulated host: both probes must run here")
        mcc_skip_test(abi/vector-layout-known-positive
            "needs a reference cc and a native unemulated host: both probes must run here")
    endif()

    if(_ref_cc AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR)
        add_test(NAME abi/arm64-macho-stackargs
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/arm64-macho-stackargs.sh"
                    "${_mcc_exe}" "${_ref_cc}"
                    "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/arm64-macho-stackargs")
        set_tests_properties(abi/arm64-macho-stackargs PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 120)
        add_test(NAME abi/arm64-macho-stackargs-known-positive
            COMMAND "${CMAKE_COMMAND}" -E env "STACKABI_MUTATE=1"
                    sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/arm64-macho-stackargs.sh"
                    "${_mcc_exe}" "${_ref_cc}"
                    "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/arm64-macho-stackargs-kp")
        set_tests_properties(abi/arm64-macho-stackargs-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 120)
    else()
        mcc_skip_test(abi/arm64-macho-stackargs
            "needs a reference cc and a native unemulated arm64 Darwin host")
        mcc_skip_test(abi/arm64-macho-stackargs-known-positive
            "needs a reference cc and a native unemulated arm64 Darwin host")
    endif()

    # abi/zero-array-fp -- a struct with a trailing zero-length array member
    # (struct{double a,b; char pad[0];}) must keep its FP-aggregate register
    # class; the phantom [0] member must be skipped in argument classification.
    # Both directions must RUN here against a native reference cc.
    if(_ref_cc AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR)
        add_test(NAME abi/zero-array-fp
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/zero-array-fp-abi.sh"
                    "${_mcc_exe}" "${_ref_cc}"
                    "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/zero-array-fp-abi")
        set_tests_properties(abi/zero-array-fp PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 120)
        add_test(NAME abi/zero-array-fp-known-positive
            COMMAND "${CMAKE_COMMAND}" -E env "ZEROARR_MUTATE=1"
                    sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/zero-array-fp-abi.sh"
                    "${_mcc_exe}" "${_ref_cc}"
                    "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/zero-array-fp-abi-kp")
        set_tests_properties(abi/zero-array-fp-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 120)
    else()
        mcc_skip_test(abi/zero-array-fp
            "needs a reference cc and a native unemulated host: both directions must run here")
        mcc_skip_test(abi/zero-array-fp-known-positive
            "needs a reference cc and a native unemulated host: both directions must run here")
    endif()

    if(MCC_CPU MATCHES "^(i386|x86_64)$"
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

    if((MCC_CPU STREQUAL "x86_64" OR MCC_CPU STREQUAL "i386")
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm/dash_s_roundtrip/prog.c")
        add_test(NAME dash-s-roundtrip COMMAND "$<TARGET_FILE:mccharness>" dashs
            --mcc ${_mcc_exe} --bdir ${_bdir} --idir ${_idir}
            --srcdir ${CMAKE_CURRENT_SOURCE_DIR}/tests --work ${CMAKE_BINARY_DIR}
            --emu "${MCC_EMULATOR}")
        set_tests_properties(dash-s-roundtrip PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    else()
        mcc_skip_test(dash-s-roundtrip
            "requires x86_64/i386 target with integrated assembler (host target: ${MCC_CPU})")
    endif()

    add_executable(seccmp tests/support/seccmp.c)
    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(dashsbytes tools/dashsbytes.c tools/toolsupport.c)
        target_link_libraries(dashsbytes ${CMAKE_DL_LIBS})
    endif()
    add_executable(objcheck tools/objcheck.c)

    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(defcheck tools/defcheck.c tools/toolsupport.c)
        target_link_libraries(defcheck ${CMAKE_DL_LIBS})
        file(GLOB _win32_defs "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib/*.def")
        add_test(NAME def-verify COMMAND "$<TARGET_FILE:defcheck>" verify ${_win32_defs})
        set_tests_properties(def-verify PROPERTIES FIXTURES_REQUIRED MCC_BUILT)

        if(MCC_PYTHON3)
            add_test(NAME bitint-diff
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/bitint-diff.py"
                        --mcc "$<TARGET_FILE:mcc>" --bdir "${CMAKE_BINARY_DIR}"
                        --min-lines 40
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/types/bitint.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/types/bitint128.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/types/bitint256.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/types/bitint_lit.c")
            set_tests_properties(bitint-diff PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 300 LABELS "bitint")
        else()
            mcc_skip_test(bitint-diff
                "the 3-way _BitInt value differential needs python3 (and self-skips without gcc+clang)")
        endif()

        add_test(NAME ci-matrix
            COMMAND "$<TARGET_FILE:ci>" matrix "${CMAKE_CURRENT_SOURCE_DIR}/CMakePresets.json")
        set_tests_properties(ci-matrix PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            PASS_REGULAR_EXPRESSION "linux-gcc")

        # T-lin-10374: two `mcc -c` of a fixed source must be byte-identical.
        # An object carries no .note.gnu.build-id, so this isolates codegen from
        # the benign whole-binary differences (build-id + DWARF build paths).
        add_test(NAME "ci/reproducible-object"
            COMMAND "${CMAKE_COMMAND}"
                    -DMCC=$<TARGET_FILE:mcc>
                    -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/misc/repro-subject.c
                    -DWORK=${CMAKE_BINARY_DIR}/repro-object
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/repro_object.cmake")
        set_tests_properties("ci/reproducible-object" PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT TIMEOUT 120)

        if(TARGET mccharness)
            add_test(NAME ci-pkg-smoke COMMAND "$<TARGET_FILE:mccharness>" pkgsmoke
                --ci "$<TARGET_FILE:ci>" --work "${CMAKE_BINARY_DIR}/pkgsmoke-work")
            set_tests_properties(ci-pkg-smoke PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77
                ENVIRONMENT "CMAKE_COMMAND=${CMAKE_COMMAND}")

            add_test(NAME qemu-fetch-parse COMMAND "$<TARGET_FILE:mccharness>" qemufetch --selftest)
            set_tests_properties(qemu-fetch-parse PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(ci-pkg-smoke "the mccharness driver is not built in this configuration")
            mcc_skip_test(qemu-fetch-parse "the mccharness driver is not built in this configuration")
        endif()

        # T-lin-10388: detect host-local provisioning LOSS. Auto-stamps each resource
        # in tests/provisioning.txt the first time it is seen present (stamps kept
        # host-local + untracked under vendor/.provisioned/), then FAILs by name if a
        # previously-stamped resource vanishes -- instead of the dependent cell silently
        # degrading to an mcc_skip_test stub. Pure shell + file checks: no python, no
        # mcc build, so it registers unconditionally and still fires if either is absent.
        add_test(NAME "ci/provisioning-detector"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/provisioning-detector.sh"
                    "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/.provisioned")
        set_tests_properties("ci/provisioning-detector" PROPERTIES LABELS native TIMEOUT 60)
        add_test(NAME "ci/provisioning-detector-known-positive"
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/provisioning-detector.sh" --selftest)
        set_tests_properties("ci/provisioning-detector-known-positive" PROPERTIES LABELS native TIMEOUT 60)

        find_program(MCC_PYTHON3 NAMES python3 python)
        if(MCC_PYTHON3 AND (MCC_EMBED_MCCRT OR MCC_TARGETOS STREQUAL "Darwin"
                            OR (MCC_TARGETOS STREQUAL "WIN32"
                                AND NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")))
            add_test(NAME selfhost-smoke
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-smoke.py"
                        "${CMAKE_BINARY_DIR}")
            set_tests_properties(selfhost-smoke PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            add_test(NAME selfhost-smoke-gates
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-smoke.py"
                        "${CMAKE_BINARY_DIR}"
                        -fpromote-locals -freg-color -ftree-loop-im
                        -fivopts -ftree-pre -ftree-reassoc
                        -fsethi-ullman-leaf -fnarrow-elim -farg-forward
                        -fspill-share -floop-vlat -fdivmagic)
            set_tests_properties(selfhost-smoke-gates PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            # T-lin-10047: register tools/selfhost-o3.py as a live cell. It builds
            # a -O3 self-hosted mcc (mcc compiling src/mcc.c at -O3, linked with
            # its own runtime) and verifies that compiler with fib(10)==55 and a
            # quicksort golden -- the built-in floor: a -O3 self-host miscompile
            # fails the cell.  Was registered nowhere.
            add_test(NAME selfhost-o3
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-o3.py"
                        "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/selfhost-o3-mcc" -O3)
            set_tests_properties(selfhost-o3 PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS native TIMEOUT 900)

            # T-lin-10047: register tools/c2_equiv.sh (arena-vs-parser semantic
            # differential -- builds a key twice with MCC_REPLAY_IR unset then set
            # and compares program behaviour).  Floor = its refuse-zero-pass guard
            # (a differential over a population that never ran is nothing, not
            # "NONE").  Skips (77) when no key is measurable on this host/build
            # (needs the per-target cross compilers / a non-empty exec population).
            add_test(NAME "c2/equiv"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/c2_equiv.sh"
                        "${CMAKE_BINARY_DIR}" all -O1 "${CMAKE_BINARY_DIR}/c2equiv")
            set_tests_properties("c2/equiv" PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS native SKIP_RETURN_CODE 77 TIMEOUT 900)

            foreach(_op -O2 -O3)
                string(REPLACE "-" "" _optag "${_op}")
                add_test(NAME selfhost-output-parity-${_optag}
                    COMMAND "${MCC_PYTHON3}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-output-parity.py"
                            "${CMAKE_BINARY_DIR}" "--opt=${_op}")
                set_tests_properties(selfhost-output-parity-${_optag} PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800)
            endforeach()

            # N13: the must-run manifest. Asserts that cells the suite is not
            # allowed to lose silently are actually registered. The skip half
            # needs a results file and runs from CI (tools/must-run.py --results).
            add_test(NAME "ci/must-run-registered"
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/must-run.py"
                        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/must-run.txt"
                        --build "${CMAKE_BINARY_DIR}")
            set_tests_properties("ci/must-run-registered" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT)

            # T-lin-10003: the gate contract. must-run.txt asserts a cell is
            # registered; this asserts the cell can fail. The two ratchets are
            # the whole point -- they are the count of gates that still have no
            # floor and no known-positive, and they may only ever fall.
            add_test(NAME "ci/gate-contract"
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/gate-contract.py"
                        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/gate-contract.txt"
                        --must-run "${CMAKE_CURRENT_SOURCE_DIR}/tests/must-run.txt"
                        --build "${CMAKE_BINARY_DIR}"
                        --min-rows 104 --min-proved 54
                        --max-unfloored 86 --max-unproved 50)
            set_tests_properties("ci/gate-contract" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)

            add_test(NAME "ci/gate-contract-known-positive"
                COMMAND "${CMAKE_COMMAND}"
                        -DPY=${MCC_PYTHON3}
                        -DTOOL=${CMAKE_CURRENT_SOURCE_DIR}/tools/gate-contract.py
                        -DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/tests/gate-contract.txt
                        -DMUSTRUN=${CMAKE_CURRENT_SOURCE_DIR}/tests/must-run.txt
                        -DBINDIR=${CMAKE_BINARY_DIR}
                        -DMINROWS=90 -DMINPROVED=40 -DMINPROVEDLIVE=51
                        -DMAXUNFLOORED=86 -DMAXUNPROVED=51
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gate_contract_mutate.cmake")
            set_tests_properties("ci/gate-contract-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)
            # Phase -1: the drop histogram exists; this gives it teeth.
            add_test(NAME "rir/rec-miss"
                COMMAND "${CMAKE_COMMAND}"
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rir_recmiss.cmake")
            set_tests_properties("rir/rec-miss" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)

            add_test(NAME "rir/drop-ratchet"
                COMMAND "${CMAKE_COMMAND}"
                        -DMCC=$<TARGET_FILE:mcc>
                        -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                        -DBINDIR=${CMAKE_CURRENT_BINARY_DIR}
                        -DCPU=${MCC_CPU}
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rir_drop_ratchet.cmake")
            set_tests_properties("rir/drop-ratchet" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)
            add_test(NAME rir-coverage
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                        "${CMAKE_BINARY_DIR}" --corpus self --levels O0,O1,O2,O3)
            set_tests_properties(rir-coverage PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 900)

            add_test(NAME node-census
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/node-census.py"
                        "${CMAKE_BINARY_DIR}"
                        --srcdir "${CMAKE_CURRENT_SOURCE_DIR}"
                        --bank "${CMAKE_CURRENT_SOURCE_DIR}/tests/rir/node-bank.json")
            set_tests_properties(node-census PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 600)

            add_test(NAME rir-gap-classes
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                        "${CMAKE_BINARY_DIR}" --check-gap-dir --levels O0,O1,O2,O3)
            set_tests_properties(rir-gap-classes PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            add_test(NAME rir-lowerable-classes
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                        "${CMAKE_BINARY_DIR}" --check-low-dir --levels O0,O1,O2,O3)
            set_tests_properties(rir-lowerable-classes PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)

            add_test(NAME rir-coverage-census
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                        "${CMAKE_BINARY_DIR}" --corpus wide --levels O0,O1,O2,O3
                        --top 3 --opt-in)
            set_tests_properties(rir-coverage-census PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "census" SKIP_RETURN_CODE 77 TIMEOUT 3600
                ENVIRONMENT "MCC_RIR_CENSUS=1")

            add_test(NAME slice-enum
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/slice-census.py"
                        "${CMAKE_BINARY_DIR}" --levels O0,O1,O2,O3 --verify --quiet
                        --min-slices 100 --jobs 8 --sources
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/optimizer/licm.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/optimizer/loop_interchange.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/optimizer/loop_fusion.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/optimizer/cse.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/functions_abi/indirect_call_shapes.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/functions_abi/inline.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/functions_abi/recursion.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/statements/for.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/statements/switch.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/programs/quicksort.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/programs/math_library.c"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/programs/grep.c")
            set_tests_properties(slice-enum PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            add_test(NAME slice-census
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/slice-census.py"
                        "${CMAKE_BINARY_DIR}" --corpus wide --levels O0,O1,O2,O3
                        --verify --top 20 --jobs 8 --opt-in)
            set_tests_properties(slice-census PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "census" SKIP_RETURN_CODE 77 TIMEOUT 3600
                ENVIRONMENT "MCC_SLICE_CENSUS_RUN=1;MCC_TEST_CPU=${MCC_CPU};MCC_TEST_OS=${MCC_TARGETOS};MCC_X11_INCLUDE_DIR=${MCC_X11_INCLUDE_DIR}")

            # S5': the dynamic per-loop trip census. The control cell is the one
            # that matters -- it compiles a program whose trip counts are known
            # by construction at every -O level and compares the histogram to
            # ground truth, plus a negative control (no -floop-census, no data)
            # and a perturbation (change a bound, the numbers move).
            add_test(NAME loop-census-control
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/loop-census.py"
                        "${CMAKE_BINARY_DIR}" --selftest)
            set_tests_properties(loop-census-control PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)

            # The par= verdict has its own ground truth: loops whose dependence
            # structure is known by construction.  The cell that matters is the
            # unsoundness check -- a loop that carries a dependence must never
            # come back par=1 -- plus a negative control (-O0 builds no AST, so
            # every loop is par=?) and a perturbation (drop the carry, the
            # verdict flips), so the predicate cannot pass by being inert.
            add_test(NAME loop-census-parallel
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/loop-census.py"
                        "${CMAKE_BINARY_DIR}" --partest)
            set_tests_properties(loop-census-parallel PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)

            # The lock-independence verdict has ground truth the same way: the
            # sections in tests/thread/locks.c are disjoint or conflicting by
            # construction. The cell that matters is the unsoundness one -- a
            # pair that shares an object must never come back independent,
            # because that verdict is what decides whether the mutex becomes a
            # serializing edge or nothing at all -- plus a negative control
            # (-O0 builds no AST, no verdicts) and a perturbation (make one
            # disjoint section write the other's object, the verdict flips).
            add_test(NAME thread-census-control
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/thread-census.py"
                        "${CMAKE_BINARY_DIR}" --selftest)
            set_tests_properties(thread-census-control PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)

            add_test(NAME loop-census
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/loop-census.py"
                        "${CMAKE_BINARY_DIR}" --levels O2 --top 20 --opt-in)
            set_tests_properties(loop-census PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "census" SKIP_RETURN_CODE 77 TIMEOUT 3600
                ENVIRONMENT "MCC_LOOP_CENSUS_RUN=1")

            # The same census over a NUMERIC workload. The self-compile is
            # pointer-chasing, allocation and switch dispatch -- close to the
            # worst case for data parallelism -- so on its own it cannot decide
            # whether a lane source exists anywhere, only whether one exists in
            # the compiler. The corpus is tools/runtime-bench.py's KERNELS,
            # imported rather than restated so it cannot be curated to produce
            # lanes, and its per-kernel argv is used unchanged.
            add_test(NAME loop-census-numeric
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/loop-census.py"
                        "${CMAKE_BINARY_DIR}" --corpus runtime --levels O2
                        --top 20 --opt-in)
            set_tests_properties(loop-census-numeric PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT LABELS "census" SKIP_RETURN_CODE 77
                TIMEOUT 3600 ENVIRONMENT "MCC_LOOP_CENSUS_RUN=1")

            add_test(NAME rir-nofb-probe
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                        "${CMAKE_BINARY_DIR}" --corpus exec --levels O0,O1,O2,O3
                        --nofb-probe --opt-in)
            set_tests_properties(rir-nofb-probe PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "census" SKIP_RETURN_CODE 77 TIMEOUT 3600
                ENVIRONMENT "MCC_RIR_CENSUS=1")

            add_test(NAME rir-nofb-probe-self
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                        "${CMAKE_BINARY_DIR}" --corpus self --levels O0,O1,O2,O3
                        --nofb-probe --opt-in)
            set_tests_properties(rir-nofb-probe-self PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "census" SKIP_RETURN_CODE 77 TIMEOUT 7200
                ENVIRONMENT "MCC_RIR_CENSUS=1")

            # The emit map: what each of the four layers (AOT/RIR/AST/JRN) does
            # for every body, and the machine-code emit calls reconciled against
            # the bytes that survive. Needs MCC_CONFIG_TRACE=ON to say anything;
            # on a build without it the tool reports 77 rather than a green zero.
            add_test(NAME emit-map-full-language
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/emit-map.py"
                        "${CMAKE_BINARY_DIR}" --target full_language.c --opt=-O1
                        --bank "${CMAKE_CURRENT_SOURCE_DIR}/tests/emitmap/bank.json"
                        --opt-in)
            set_tests_properties(emit-map-full-language PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "census" SKIP_RETURN_CODE 77 TIMEOUT 3600
                ENVIRONMENT "MCC_EMIT_MAP=1")

            add_test(NAME emit-map-selfhost
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/emit-map.py"
                        "${CMAKE_BINARY_DIR}" --target selfhost --opt=-O1
                        --bank "${CMAKE_CURRENT_SOURCE_DIR}/tests/emitmap/bank.json"
                        --opt-in)
            set_tests_properties(emit-map-selfhost PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                LABELS "census" SKIP_RETURN_CODE 77 TIMEOUT 7200
                ENVIRONMENT "MCC_EMIT_MAP=1")

            # N18: the -O0 --embed-jit arm is the least faithful one and
            # nothing watched it. Two compiles and a subtraction off MCC_INV=1,
            # so unlike emit-map above it needs no trace build, no opt-in and
            # about a second per target -- which is why it is an ordinary cell.
            add_test(NAME "ast/inv-faithful"
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/inv-faithful.py"
                        "${CMAKE_BINARY_DIR}"
                        --bank "${CMAKE_CURRENT_SOURCE_DIR}/tests/emitmap/faithful.json")
            set_tests_properties("ast/inv-faithful" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

            add_test(NAME "ast/inv-faithful-known-positive"
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/inv-faithful.py"
                        "${CMAKE_BINARY_DIR}" --known-positive
                        --bank "${CMAKE_CURRENT_SOURCE_DIR}/tests/emitmap/faithful.json")
            set_tests_properties("ast/inv-faithful-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

            add_test(NAME "census/gates-armed"
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/census-armed.py"
                        "${CMAKE_BINARY_DIR}" --min-cells 5)
            set_tests_properties("census/gates-armed" PROPERTIES
                LABELS "census" TIMEOUT 300)

            add_test(NAME "ci/registration-stubs"
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/regstub-lint.py"
                        "${CMAKE_CURRENT_SOURCE_DIR}" --min-chains 30)
            set_tests_properties("ci/registration-stubs" PROPERTIES TIMEOUT 300)

            add_test(NAME selfhost-fixpoint
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-fixpoint.py"
                        "${CMAKE_BINARY_DIR}")
            set_tests_properties(selfhost-fixpoint PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            add_test(NAME selfhost-fixpoint-O3
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-fixpoint.py"
                        "${CMAKE_BINARY_DIR}" --opt=-O3)
            set_tests_properties(selfhost-fixpoint-O3 PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            foreach(_qa riscv64 arm arm64 i386)
                foreach(_qo -O1 -O2)
                    string(REPLACE "-" "" _qon "${_qo}")
                    add_test(NAME selfhost-qemu-${_qa}-${_qon}
                        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/qemu-selfhost.sh"
                                "${_qa}" "${CMAKE_BINARY_DIR}/selfhost-qemu-${_qa}-${_qon}"
                                "${_qo}")
                    set_tests_properties(selfhost-qemu-${_qa}-${_qon} PROPERTIES
                        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT"
                        LABELS "qemu" SKIP_RETURN_CODE 77 TIMEOUT 1800 ENVIRONMENT "MCC_CROSS_DIR=${MCC_CROSS_DIR}")
                endforeach()
            endforeach()

            add_test(NAME selfhost-fixpoint-O1
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-fixpoint.py"
                        "${CMAKE_BINARY_DIR}" --opt=-O1)
            set_tests_properties(selfhost-fixpoint-O1 PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            add_test(NAME selfhost-fixpoint-Os
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-fixpoint.py"
                        "${CMAKE_BINARY_DIR}" --opt=-Os)
            set_tests_properties(selfhost-fixpoint-Os PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            add_test(NAME selfhost-fixpoint-gates
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-fixpoint.py"
                        "${CMAKE_BINARY_DIR}"
                        -fpromote-locals -freg-color -ftree-loop-im
                        -fivopts -ftree-pre -ftree-reassoc
                        -fsethi-ullman-leaf -fnarrow-elim -farg-forward
                        -fspill-share -floop-vlat -fdivmagic)
            set_tests_properties(selfhost-fixpoint-gates PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)

            if(MCC_CPU STREQUAL "arm64" AND NOT CMAKE_CROSSCOMPILING)
                foreach(_shopt IN ITEMS -O2 -O3 -Os)
                    add_test(NAME "selfhost-fixpoint-memmodel${_shopt}"
                        COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-fixpoint.py"
                                "${CMAKE_BINARY_DIR}" "--opt=${_shopt}"
                                -fchain-store -fpromote-incdec
                                -fivopts-ptr)
                    set_tests_properties("selfhost-fixpoint-memmodel${_shopt}"
                        PROPERTIES FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300
                        ENVIRONMENT "MCC_DEV=1")
                endforeach()

                add_test(NAME selfhost-smoke-memmodel
                    COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-smoke.py"
                            "${CMAKE_BINARY_DIR}"
                            -fchain-store -fpromote-incdec
                            -fivopts-ptr)
                set_tests_properties(selfhost-smoke-memmodel PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                    TIMEOUT 300 ENVIRONMENT "MCC_DEV=1")
            endif()
        else()
            set(_nopy_why "the selfhost/census drivers are python3 scripts and need both a python3 on PATH and an mcc that can rebuild itself (MCC_EMBED_MCCRT, or a Darwin/mingw target)")
            mcc_skip_test("selfhost-smoke" "${_nopy_why}")
            mcc_skip_test("selfhost-smoke-gates" "${_nopy_why}")
            mcc_skip_test("ci/must-run-registered" "${_nopy_why}")
            mcc_skip_test("ci/gate-contract" "${_nopy_why}")
            mcc_skip_test("ci/gate-contract-known-positive" "${_nopy_why}")
            mcc_skip_test("ci/registration-stubs" "${_nopy_why}")
            mcc_skip_test("rir/drop-ratchet" "${_nopy_why}")
            mcc_skip_test("rir/rec-miss" "${_nopy_why}")
            mcc_skip_test("rir-coverage" "${_nopy_why}")
            mcc_skip_test("node-census" "${_nopy_why}")
            mcc_skip_test("rir-gap-classes" "${_nopy_why}")
            mcc_skip_test("rir-lowerable-classes" "${_nopy_why}")
            mcc_skip_test("slice-enum" "${_nopy_why}")
            mcc_skip_test("loop-census-control" "${_nopy_why}")
            mcc_skip_test("loop-census-parallel" "${_nopy_why}")
            mcc_skip_test("thread-census-control" "${_nopy_why}")
            mcc_skip_test("selfhost-fixpoint" "${_nopy_why}")
            mcc_skip_test("selfhost-fixpoint-O3" "${_nopy_why}")
            mcc_skip_test("selfhost-fixpoint-O1" "${_nopy_why}")
            mcc_skip_test("selfhost-fixpoint-Os" "${_nopy_why}")
            mcc_skip_test("selfhost-fixpoint-gates" "${_nopy_why}")
            mcc_skip_test("selfhost-o3" "${_nopy_why}")
            mcc_skip_test("c2/equiv" "${_nopy_why}")
            mcc_skip_test("rir-coverage-census" "${_nopy_why}" LABELS census)
            mcc_skip_test("slice-census" "${_nopy_why}" LABELS census)
            mcc_skip_test("loop-census" "${_nopy_why}" LABELS census)
            mcc_skip_test("loop-census-numeric" "${_nopy_why}" LABELS census)
            mcc_skip_test("rir-nofb-probe" "${_nopy_why}" LABELS census)
            mcc_skip_test("rir-nofb-probe-self" "${_nopy_why}" LABELS census)
            mcc_skip_test("emit-map-full-language" "${_nopy_why}" LABELS census)
            mcc_skip_test("emit-map-selfhost" "${_nopy_why}" LABELS census)
            mcc_skip_test("ast/inv-faithful" "${_nopy_why}")
            mcc_skip_test("ast/inv-faithful-known-positive" "${_nopy_why}")
            mcc_skip_test("census/gates-armed" "${_nopy_why}" LABELS census)
            foreach(_op -O2 -O3)
                string(REPLACE "-" "" _optag "${_op}")
                mcc_skip_test("selfhost-output-parity-${_optag}" "${_nopy_why}")
            endforeach()
            foreach(_qa riscv64 arm arm64 i386)
                foreach(_qo -O1 -O2)
                    string(REPLACE "-" "" _qon "${_qo}")
                    mcc_skip_test("selfhost-qemu-${_qa}-${_qon}" "${_nopy_why}" LABELS qemu)
                endforeach()
            endforeach()
        endif()

        if(MCC_PYTHON3 AND MCC_EMBED_JIT)
            add_test(NAME selfhost-jit
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-jit.py"
                        "${CMAKE_BINARY_DIR}" "${MCC_CPU}")
            set_tests_properties(selfhost-jit PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 720)

            # What a zero-AOT `mcc -O0 --embed-jit` self-host runs at RUNTIME. It
            # hot-swaps ~190 functions, so zero AOT optimization is not zero runtime
            # optimization -- but the swapped code is optimize=1, not -O3+, and neither
            # the -O13 search nor any resumable cache is involved. Three of those four
            # are negative results, which is exactly why they need an assertion.
            # -g must not suppress the bake, and the resulting image must stay
            # debuggable. Also pins the limit: hot-patched code is unsymbolized,
            # because mcc implements no GDB JIT interface.
            add_test(NAME jit/gdb-debuggable
                COMMAND "${MCC_PYTHON3}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tools/jit-gdb-debuggable.py"
                        "${CMAKE_BINARY_DIR}")
            set_tests_properties(jit/gdb-debuggable PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 600)

            add_test(NAME jit/selfhost-opt
                COMMAND "${MCC_PYTHON3}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tools/jit-selfhost-opt.py"
                        "${CMAKE_BINARY_DIR}")
            set_tests_properties(jit/selfhost-opt PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 600)
        else()
            mcc_skip_test(selfhost-jit
                "tools/selfhost-jit.py needs a python3 on PATH and -DMCC_EMBED_JIT=ON")
            mcc_skip_test(jit/gdb-debuggable
                "tools/jit-gdb-debuggable.py needs a python3 on PATH and -DMCC_EMBED_JIT=ON; with no bake there is no hot-swapped code to ask gdb about")
            mcc_skip_test(jit/selfhost-opt
                "tools/jit-selfhost-opt.py needs a python3 on PATH and -DMCC_EMBED_JIT=ON; it measures what a zero-AOT --embed-jit self-host optimizes at runtime")
        endif()

        if(MCC_PYTHON3 AND MCC_EMBED_JIT AND MCC_TARGET_IS_HOST AND UNIX
           AND (MCC_CPU STREQUAL "x86_64" OR MCC_CPU STREQUAL "arm64"))
            add_test(NAME "jit/xoracle-known-positive"
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/jitconform.py"
                        --phase selfcheck
                        "--out=${CMAKE_BINARY_DIR}/jitconform-selfcheck"
                        "--mcc=$<TARGET_FILE:mcc>" --surface embed)
            set_tests_properties("jit/xoracle-known-positive" PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)

            set(MCC_XSUITE_GCC "$ENV{HOME}/Projects/gcc" CACHE PATH
                "gcc source checkout supplying gcc.c-torture/execute for jit/xoracle-conformance")
            set(MCC_XSUITE_LLVMTS "$ENV{HOME}/Projects/llvm-test-suite" CACHE PATH
                "llvm-test-suite checkout supplying SingleSource/UnitTests for jit/xoracle-conformance")
            mark_as_advanced(MCC_XSUITE_GCC MCC_XSUITE_LLVMTS)
            set(_jitconf_ts "")
            if(EXISTS "${MCC_XSUITE_LLVMTS}/SingleSource/UnitTests")
                set(_jitconf_ts "--testsuite=${MCC_XSUITE_LLVMTS}" --suite ts-unittests)
            else()
                # A corpus that vanishes must say so. --limit is per-suite, so losing this
                # arm halves jit/xoracle-conformance's subject and takes jit/xoracle-coverage
                # below its --min-cross 400 floor against a single-suite denominator that
                # tops out at 379 -- i.e. the cell then fails for what reads as a coverage
                # reason when the cause is a path that does not exist.
                message(STATUS
                    "jit/xoracle-conformance: llvm-test-suite absent at "
                    "'${MCC_XSUITE_LLVMTS}' (no SingleSource/UnitTests) -- running "
                    "c-torture only. jit/xoracle-coverage will not reach --min-cross 400 "
                    "on one suite; set -DMCC_XSUITE_LLVMTS=<path> to restore it")
            endif()
            if(EXISTS "${MCC_XSUITE_GCC}/gcc/testsuite/gcc.c-torture/execute")
                add_test(NAME "jit/xoracle-conformance"
                    COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/jitconform.py"
                            --phase both
                            "--out=${CMAKE_BINARY_DIR}/jitconform-cell"
                            "--gcc=${MCC_XSUITE_GCC}" ${_jitconf_ts}
                            --suite c-torture/execute --limit 400
                            "--mcc=$<TARGET_FILE:mcc>" --surface embed --label cell
                            --min-pass 100 --max-miscompile 0)
                set_tests_properties("jit/xoracle-conformance" PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

                if(_jitconf_ts)
                    add_test(NAME "jit/xoracle-coverage"
                        COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/xoracle.py"
                                --phase report
                                "--out=${CMAKE_BINARY_DIR}/jitconform-cell"
                                --min-cross 400 --min-exclusive 1)
                    set_tests_properties("jit/xoracle-coverage" PROPERTIES
                        FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300
                        DEPENDS "jit/xoracle-conformance")
                else()
                    mcc_skip_test("jit/xoracle-coverage"
                        "no llvm-test-suite at ${MCC_XSUITE_LLVMTS} -- --min-cross 400 is a two-suite floor and one suite tops out at 379, so this cell can only fail, and it would fail naming a coverage number when the cause is an absent path; set -DMCC_XSUITE_LLVMTS=<path>")
                endif()
            else()
                mcc_skip_test("jit/xoracle-conformance"
                    "no gcc.c-torture/execute corpus at ${MCC_XSUITE_GCC} -- point -DMCC_XSUITE_GCC=<gcc checkout> at one to measure the JIT against clang as cross oracle")
                mcc_skip_test("jit/xoracle-coverage"
                    "no gcc.c-torture/execute corpus at ${MCC_XSUITE_GCC} -- the coverage report reads jit/xoracle-conformance's output and there is none to read")
            endif()
        else()
            mcc_skip_test("jit/xoracle-conformance"
                "needs python3, -DMCC_EMBED_JIT=ON and a host x86_64/arm64 target; the JIT cross-oracle differential runs the engine in-process and cannot be emulated")
            mcc_skip_test("jit/xoracle-known-positive"
                "needs python3, -DMCC_EMBED_JIT=ON and a host x86_64/arm64 target; without them the selfcheck has no engine to prove the differential against")
            mcc_skip_test("jit/xoracle-coverage"
                "needs python3, -DMCC_EMBED_JIT=ON and a host x86_64/arm64 target; the coverage report reads jit/xoracle-conformance's output and there is none to read")
        endif()

        if(MCC_PYTHON3 AND MCC_EMBED_JIT AND WIN32 AND NOT MSVC)
            add_test(NAME embed-jit-smoke
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-jit-smoke.py"
                        "${CMAKE_BINARY_DIR}")
            set_tests_properties(embed-jit-smoke PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 180)
        else()
            mcc_skip_test(embed-jit-smoke
                "the --embed-jit PE smoke needs a python3 on PATH, -DMCC_EMBED_JIT=ON and a non-MSVC Windows host")
        endif()

        if(TARGET mccbuild AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
            add_test(NAME git-stamp COMMAND "$<TARGET_FILE:mccbuild>" --githash)
            set_tests_properties(git-stamp PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                PASS_REGULAR_EXPRESSION "@")
        else()
            mcc_skip_test(git-stamp
                "needs the mccbuild driver and a .git directory to stamp from (this is not a git checkout)")
        endif()

        if(TARGET mccbuild AND NOT CMAKE_CROSSCOMPILING AND "${MCC_TARGET_ARCH}" STREQUAL "")
            add_test(NAME host-detect COMMAND "$<TARGET_FILE:mccbuild>" --detect
                --cc "${CMAKE_C_COMPILER}"
                --expect-cpu "${MCC_CPU}" --expect-os "${MCC_TARGETOS}"
                --expect-triplet "${MCC_TRIPLET}")
            set_tests_properties(host-detect PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        else()
            mcc_skip_test(host-detect
                "mccbuild --detect compares against the host it is running on; not built, cross-compiling, or MCC_TARGET_ARCH overrides the target")
        endif()

        if(UNIX)
            add_test(NAME target-link-gate
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/ci/target-link-gate.sh"
                        "-DMCC_GPU_LANG_MSL=${MCC_GPU_LANG_MSL_VALUE}"
                        ${MCC_COMPUTE_BACKEND_LDFLAGS})
            set_tests_properties(target-link-gate PROPERTIES SKIP_RETURN_CODE 77
                TIMEOUT 900)
        endif()

        if(TARGET mccbuild)
            add_test(NAME arm-abi-detect COMMAND "$<TARGET_FILE:mccbuild>" --detect --arm-selftest)
            set_tests_properties(arm-abi-detect PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(arm-abi-detect "the mccbuild driver is not built in this configuration")
        endif()

        if(TARGET mccbuild AND _ref_cc)
            add_test(NAME cross-factory COMMAND "$<TARGET_FILE:mccbuild>"
                --cross x86_64 --cc "${_ref_cc}"
                --out "${CMAKE_BINARY_DIR}/cross-factory-smoke")
            set_tests_properties(cross-factory PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

            add_test(NAME cross-factory-i386 COMMAND "$<TARGET_FILE:mccbuild>"
                --cross i386 --cc "${_ref_cc}"
                --out "${CMAKE_BINARY_DIR}/cross-factory-i386")
            set_tests_properties(cross-factory-i386 PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        elseif(TARGET mccbuild)
            mcc_skip_test(cross-factory
                "needs a GCC-compatible cc (set MCC_REF_CC or put gcc on PATH)")
            mcc_skip_test(cross-factory-i386
                "needs a GCC-compatible cc (set MCC_REF_CC or put gcc on PATH)")
        else()
            mcc_skip_test(cross-factory "the mccbuild driver is not built in this configuration")
            mcc_skip_test(cross-factory-i386 "the mccbuild driver is not built in this configuration")
        endif()

        if(TARGET mccbuild)
            add_test(NAME config-defines COMMAND "$<TARGET_FILE:mccbuild>" --emit-defines
                --check "${CMAKE_BINARY_DIR}/config-defines.txt"
                --cpu "${MCC_CPU}" --targetos "${MCC_TARGETOS}" --libc "${MCC_CONFIG_LIBC}"
                --cpuver "${MCC_CPUVER}" --arm-eabi "${MCC_ARM_EABI}" --arm-vfp "${MCC_ARM_VFP}"
                --arm-hardfloat "${MCC_ARM_HARDFLOAT}" --arm-idiv "${MCC_ARM_IDIV}"
                --run-mmap-exec "${MCC_RUN_MMAP_EXEC}" --pie "${MCC_CONFIG_PIE}" --pic "${MCC_CONFIG_PIC}"
                --new-dtags "${MCC_CONFIG_NEW_DTAGS}" --codesign "${MCC_CONFIG_CODESIGN}"
               
                --diag "${MCC_DIAG}"
                --embed-jit "${MCC_EMBED_JIT}" --config-jit "${MCC_CONFIG_JIT}"
                --new-macho "${MCC_CONFIG_NEW_MACHO}" --dwarf "${MCC_CONFIG_DWARF}" --semlock "${MCC_CONFIG_SEMLOCK}"
                --triplet "${MCC_TRIPLET}" --os-release "${MCC_OS_RELEASE}" --sysroot "${MCC_SYSROOT}"
                --mccdir "${MCC_RESOLVED_MCCDIR}")
            set_tests_properties(config-defines PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        else()
            mcc_skip_test(config-defines "the mccbuild driver is not built in this configuration")
        endif()
    endif()
    foreach(_dsb_arch arm64 riscv64)
        mcc_cross_cc(${_dsb_arch} _dsb_cc _dsb_fix)
        if(NOT _dsb_cc STREQUAL "" AND TARGET dashsbytes)
            add_test(NAME dash-s-bytes-${_dsb_arch}
                COMMAND "$<TARGET_FILE:dashsbytes>"
                "${_dsb_cc}"
                "$<TARGET_FILE:seccmp>"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm/dash_s_roundtrip/prog.c"
                "${CMAKE_BINARY_DIR}/dash-s-bytes-${_dsb_arch}")
            set_tests_properties(dash-s-bytes-${_dsb_arch} PROPERTIES
                FIXTURES_REQUIRED "${_dsb_fix}")
        else()
            mcc_skip_test(dash-s-bytes-${_dsb_arch}
                "requires the mcc-${_dsb_arch} cross compiler (cmake --preset cross)")
        endif()
    endforeach()

    mcc_skip_test(asm-gas-directives
        "blocked: integrated assembler lacks sgdtq/sidtq/swapgs encodings (gas_directives.S:811)")

    add_test(NAME "asm/reloc-suffix"
        COMMAND "${CMAKE_COMMAND}"
                -DMCC=$<TARGET_FILE:mcc>
                -DBINDIR=${CMAKE_BINARY_DIR}
                -DCPU=${MCC_CPU}
                -DTARGETOS=${MCC_TARGETOS}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/asm_reloc_suffix.cmake")
    set_tests_properties("asm/reloc-suffix" PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77)

    set(MCC_GCCTESTSUITE_PATH "" CACHE PATH
        "gcc.c-torture directory for the `gcctestsuite` target")
    set(MCC_ARM_CROSS_COMPILE "" CACHE STRING
        "binutils prefix (e.g. arm-linux-gnueabi-) for `arm-asm-testsuite`")

    mcc_cross_cc(i386 _xcc_i386 _xfix_i386)
    if(NOT _xcc_i386 STREQUAL "")

        mcc_mingw_resolve()
        set(_fc_harness_args "")
        set(_fc_gcc "")
        set(_fc_m32 "")
        if(EXISTS "${_MCC_MINGW_I686_GCC}")
            set(_fc_gcc "${_MCC_MINGW_I686_GCC}")
            set(_fc_m32 "")
        elseif(EXISTS "${_MCC_MINGW_MULTILIB_GCC}")
            set(_fc_gcc "${_MCC_MINGW_MULTILIB_GCC}")
            set(_fc_m32 "-m32")
        endif()
        if(NOT _fc_gcc STREQUAL "")
            execute_process(COMMAND "${_fc_gcc}" --version
                RESULT_VARIABLE _fc_gcc_rc OUTPUT_QUIET ERROR_QUIET)
            if(_fc_gcc_rc EQUAL 0)
                set(_fc_harness_args --gcc "${_fc_gcc}" --m32 "${_fc_m32}")
            else()
                message(STATUS "i386-fastcall-abi: ${_fc_gcc} is not executable on "
                               "this host; falling back to the harness default reference")
            endif()
        endif()
        add_test(NAME i386-fastcall-abi COMMAND "$<TARGET_FILE:mccharness>" i386fastcall
            --imcc ${_xcc_i386} ${_fc_harness_args}
            --work ${CMAKE_BINARY_DIR}/i386-fastcall-abi)
        set_tests_properties(i386-fastcall-abi PROPERTIES FIXTURES_REQUIRED "${_xfix_i386}"
            SKIP_REGULAR_EXPRESSION "SKIP:")

        add_test(NAME i386-fastcall-abi-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/i386fastcall-docker.sh"
                "${_xcc_i386}" "${CMAKE_BINARY_DIR}/i386-fastcall-abi-docker")
        set_tests_properties(i386-fastcall-abi-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_i386}"
            SKIP_RETURN_CODE 77)

        add_test(NAME i386-tls-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/i386tls-docker.sh"
                "${_xcc_i386}" "${CMAKE_BINARY_DIR}/i386-tls-docker")
        set_tests_properties(i386-tls-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_i386}"
            SKIP_RETURN_CODE 77)

        add_test(NAME i386-codegen-diff-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/i386diff-docker.sh"
                "${_xcc_i386}" "${CMAKE_BINARY_DIR}/i386-libmccrt.a"
                "${CMAKE_BINARY_DIR}/i386-codegen-diff-docker")
        set_tests_properties(i386-codegen-diff-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_i386}"
            SKIP_RETURN_CODE 77)

        add_test(NAME i386-divmagic-soak-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/i386divmagic-docker.sh"
                "${_xcc_i386}" "${CMAKE_BINARY_DIR}/i386-libmccrt.a"
                "${CMAKE_BINARY_DIR}/i386-divmagic-soak-docker")
        set_tests_properties(i386-divmagic-soak-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_i386}"
            SKIP_RETURN_CODE 77)

        add_test(NAME i386-inline-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/i386inline-docker.sh"
                "${CMAKE_BINARY_DIR}/i386-inline-docker")
        set_tests_properties(i386-inline-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_i386}"
            SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(i386-fastcall-abi
            "requires an i386 cross-compiler (target mcc-i386 is not configured)")
        mcc_skip_test(i386-fastcall-abi-docker
            "requires the mcc-i386 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-i386)")
        mcc_skip_test(i386-tls-docker
            "requires the mcc-i386 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-i386)")
        mcc_skip_test(i386-codegen-diff-docker
            "requires the mcc-i386 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-i386)")
        mcc_skip_test(i386-divmagic-soak-docker
            "requires the mcc-i386 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-i386)")
        mcc_skip_test(i386-inline-docker
            "requires the mcc-i386 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-i386)")
    endif()

    add_test(NAME dwarf-gdb-docker
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/dwarfgdb-docker.sh"
            "${CMAKE_BINARY_DIR}/dwarf-gdb-docker")
    set_tests_properties(dwarf-gdb-docker PROPERTIES FIXTURES_REQUIRED MCC_BUILT
        SKIP_RETURN_CODE 77)

    foreach(_eharch arm64 riscv64 amd64)
        add_test(NAME eh-unwind-${_eharch}-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/ehunwind-docker.sh"
                "${_eharch}" "${CMAKE_BINARY_DIR}/eh-unwind-${_eharch}-docker")
        set_tests_properties(eh-unwind-${_eharch}-docker PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77)
    endforeach()

    mcc_cross_cc(x86_64 _xcc_x86_64 _xfix_x86_64)
    if(NOT _xcc_x86_64 STREQUAL "")
        add_test(NAME extlink-x86_64-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/extlink-docker.sh"
                "${_xcc_x86_64}" "linux/amd64"
                "${CMAKE_BINARY_DIR}/extlink-x86_64-docker")
        set_tests_properties(extlink-x86_64-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_x86_64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME abi-diff-amd64-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/abidiff-docker.sh"
                "amd64" "${CMAKE_BINARY_DIR}/abi-diff-amd64-docker")
        set_tests_properties(abi-diff-amd64-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_x86_64}"
            SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(extlink-x86_64-docker
            "requires the mcc-x86_64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-x86_64)")
        mcc_skip_test(abi-diff-amd64-docker
            "requires the mcc-x86_64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-x86_64)")
    endif()
    mcc_cross_cc(arm64 _xcc_arm64 _xfix_arm64)
    if(NOT _xcc_arm64 STREQUAL "")
        add_test(NAME extlink-arm64-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/extlink-docker.sh"
                "${_xcc_arm64}" "linux/arm64"
                "${CMAKE_BINARY_DIR}/extlink-arm64-docker")
        set_tests_properties(extlink-arm64-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_arm64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME arm64-inline-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/arm64inline-docker.sh"
                "${CMAKE_BINARY_DIR}/arm64-inline-docker")
        set_tests_properties(arm64-inline-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_arm64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME arm64-divmagic-soak-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/arm64divmagic-docker.sh"
                "${_xcc_arm64}"
                "${CMAKE_BINARY_DIR}/arm64-divmagic-soak-docker")
        set_tests_properties(arm64-divmagic-soak-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_arm64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME abi-diff-arm64-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/abidiff-docker.sh"
                "arm64" "${CMAKE_BINARY_DIR}/abi-diff-arm64-docker")
        set_tests_properties(abi-diff-arm64-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_arm64}"
            SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(extlink-arm64-docker
            "requires the mcc-arm64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-arm64)")
        mcc_skip_test(arm64-inline-docker
            "requires the mcc-arm64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-arm64)")
        mcc_skip_test(arm64-divmagic-soak-docker
            "requires the mcc-arm64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-arm64)")
        mcc_skip_test(abi-diff-arm64-docker
            "requires the mcc-arm64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-arm64)")
    endif()
    if(MCC_PYTHON3)
        add_test(NAME runtime-bench-check
            COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/runtime-bench.py"
                --mcc "$<TARGET_FILE:mcc>" --check-only)
        set_tests_properties(runtime-bench-check PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77 TIMEOUT 900)

        add_test(NAME "optbench/null-subject"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/optlevel-bench.py" --selfcheck)
        set_tests_properties("optbench/null-subject" PROPERTIES TIMEOUT 120)

        add_test(NAME "optbench/null-subject-known-positive"
            COMMAND "${CMAKE_COMMAND}"
                -DPY=${MCC_PYTHON3}
                -DBENCH=${CMAKE_CURRENT_SOURCE_DIR}/tools/optlevel-bench.py
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/optlevel_bench_mutate.cmake")
        set_tests_properties("optbench/null-subject-known-positive" PROPERTIES
            TIMEOUT 120)

        add_test(NAME "rir/bank-keying"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                --selfcheck-bankkeying)
        set_tests_properties("rir/bank-keying" PROPERTIES TIMEOUT 120)

        add_test(NAME "rir/bank-keying-known-positive"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                --selfcheck-bankkeying --mutate)
        set_tests_properties("rir/bank-keying-known-positive" PROPERTIES
            TIMEOUT 120)

        add_test(NAME "rir/corpus-guard"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                --selfcheck-corpus-guard)
        set_tests_properties("rir/corpus-guard" PROPERTIES TIMEOUT 120)

        add_test(NAME "rir/corpus-guard-known-positive"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/rir-coverage.py"
                --selfcheck-corpus-guard --mutate)
        set_tests_properties("rir/corpus-guard-known-positive" PROPERTIES
            TIMEOUT 120)

        add_test(NAME "optbench/levelbench-bank"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/optlevel-bench.py" --check
                "--bank=${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/levelbench.tsv"
                "--mccopt=${CMAKE_CURRENT_SOURCE_DIR}/src/mccopt.h")
        set_tests_properties("optbench/levelbench-bank" PROPERTIES TIMEOUT 120)

        add_test(NAME "optbench/levelbench-bank-known-positive"
            COMMAND "${CMAKE_COMMAND}"
                -DPY=${MCC_PYTHON3}
                -DBENCH=${CMAKE_CURRENT_SOURCE_DIR}/tools/optlevel-bench.py
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/optlevel_bench_bank_mutate.cmake")
        set_tests_properties("optbench/levelbench-bank-known-positive" PROPERTIES
            TIMEOUT 120)

        add_test(NAME "fmt/census-bank"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt-census.py" --check
                "--root=${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties("fmt/census-bank" PROPERTIES TIMEOUT 120)

        add_test(NAME "fmt/census-bank-known-positive"
            COMMAND "${CMAKE_COMMAND}"
                -DPY=${MCC_PYTHON3}
                -DCENSUS=${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt-census.py
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fmt_census_bank_mutate.cmake")
        set_tests_properties("fmt/census-bank-known-positive" PROPERTIES
            TIMEOUT 120)

        add_test(NAME "docs/refs"
            COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/docref-lint.py"
                "--root=${CMAKE_CURRENT_SOURCE_DIR}" --min-refs 440)
        set_tests_properties("docs/refs" PROPERTIES TIMEOUT 120)

        add_test(NAME "docs/refs-known-positive"
            COMMAND "${CMAKE_COMMAND}"
                -DPY=${MCC_PYTHON3}
                -DLINT=${CMAKE_CURRENT_SOURCE_DIR}/tools/docref-lint.py
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/docref_lint_mutate.cmake")
        set_tests_properties("docs/refs-known-positive" PROPERTIES TIMEOUT 120)

        if(NOT MCC_EMULATOR AND NOT CMAKE_CROSSCOMPILING)
            add_test(NAME opt-determinism
                COMMAND "${MCC_PYTHON3}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/opt-determinism.py"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
                    --runs 4 --from-build "${CMAKE_BINARY_DIR}" -- -O3 -c)
            set_tests_properties(opt-determinism PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

            add_test(NAME opt-determinism-known-positive
                COMMAND "${CMAKE_COMMAND}"
                    -DPY=${MCC_PYTHON3}
                    -DTOOL=${CMAKE_CURRENT_SOURCE_DIR}/tools/opt-determinism.py
                    -DMCC=$<TARGET_FILE:mcc>
                    -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c
                    -DBDIR=${CMAKE_BINARY_DIR}
                    -DRUNS=4 -DOPT=-O3
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/opt_determinism_mutate.cmake")
            set_tests_properties(opt-determinism-known-positive PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800)

            add_test(NAME opt-search-determinism
                COMMAND "${MCC_PYTHON3}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/opt-search-determinism.py"
                    "$<TARGET_FILE:mcc>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/programs/grep.c"
                    --opt=-O13)
            set_tests_properties(opt-search-determinism PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900
                RUN_SERIAL TRUE)

            add_test(NAME opt-search-determinism-known-positive
                COMMAND "${CMAKE_COMMAND}"
                    -DPY=${MCC_PYTHON3}
                    -DTOOL=${CMAKE_CURRENT_SOURCE_DIR}/tools/opt-search-determinism.py
                    -DMCC=$<TARGET_FILE:mcc>
                    -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/exec/programs/grep.c
                    -DOPT=-O13
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/opt_search_determinism_mutate.cmake")
            set_tests_properties(opt-search-determinism-known-positive PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800
                RUN_SERIAL TRUE)

            add_test(NAME untyped-probe
                COMMAND "${MCC_PYTHON3}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/untyped-probe.py"
                    "${CMAKE_BINARY_DIR}" "O0,O1,O2,O3" self)
            set_tests_properties(untyped-probe PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)

            add_test(NAME untyped-probe-known-positive
                COMMAND "${CMAKE_COMMAND}"
                    -DPY=${MCC_PYTHON3}
                    -DTOOL=${CMAKE_CURRENT_SOURCE_DIR}/tools/untyped-probe.py
                    -DBDIR=${CMAKE_BINARY_DIR}
                    -DLEVELS=O2
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/untyped_probe_known_positive.cmake")
            set_tests_properties(untyped-probe-known-positive PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)
        else()
            mcc_skip_test(opt-determinism
                "the determinism gate recompiles src/mcc.c with the host mcc; an emulated or cross build is not running the compiler under test")
            mcc_skip_test(opt-determinism-known-positive
                "the determinism gate recompiles src/mcc.c with the host mcc; an emulated or cross build is not running the compiler under test")
            mcc_skip_test(opt-search-determinism
                "the search-tier reproducibility gate compiles a subject twice with the host mcc, once against an oversubscribed box; an emulated or cross build is not running the compiler under test")
            mcc_skip_test(opt-search-determinism-known-positive
                "the search-tier reproducibility gate compiles a subject twice with the host mcc, once against an oversubscribed box; an emulated or cross build is not running the compiler under test")
            mcc_skip_test(untyped-probe
                "the untyped census reads a report the host mcc emits while compiling src/mcc.c; an emulated or cross build is not running the compiler under test")
            mcc_skip_test(untyped-probe-known-positive
                "the untyped census reads a report the host mcc emits while compiling src/mcc.c; an emulated or cross build is not running the compiler under test")
        endif()

        if(NOT MCC_EMULATOR AND NOT CMAKE_CROSSCOMPILING
           AND NOT MCC_TARGETOS STREQUAL "WIN32")
            add_test(NAME runtime-bench-gatewin
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/runtime-bench.py"
                    --mcc "$<TARGET_FILE:mcc>" --assert-gate-wins --runs 7)
            set_tests_properties(runtime-bench-gatewin PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 RUN_SERIAL TRUE TIMEOUT 900)
        else()
            mcc_skip_test(runtime-bench-gatewin
                "gate-win assertion runs on a native optimizer build only (emulated/cross timing and counters are meaningless)")
        endif()

        if(UNIX AND NOT MCC_EMULATOR AND NOT CMAKE_CROSSCOMPILING)
            add_test(NAME opt-cache-determinism
                COMMAND "${MCC_PYTHON3}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/opt-cache-determinism.py"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}/src/mcc.c"
                    --opt=-O3 --from-build "${CMAKE_BINARY_DIR}")
            set_tests_properties(opt-cache-determinism PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800
                LABELS native)

            if(MCC_OPT_LADDER_BENCH)
                add_test(NAME optlevel-bench
                    COMMAND "${MCC_PYTHON3}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tools/optlevel-bench.py"
                        --mcc "$<TARGET_FILE:mcc>" --builddir "${CMAKE_BINARY_DIR}"
                        --out "${CMAKE_BINARY_DIR}/levelbench.tsv"
                        --json "${CMAKE_BINARY_DIR}/levelbench.json"
                        --check
                        "--bank=${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/levelbench.tsv"
                        "--mccopt=${CMAKE_CURRENT_SOURCE_DIR}/src/mccopt.h")
                set_tests_properties(optlevel-bench PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 RUN_SERIAL TRUE
                    TIMEOUT 7200 LABELS "optbench")

                add_test(NAME selfhost-optbench
                    COMMAND "${MCC_PYTHON3}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-optbench.py"
                        "${CMAKE_BINARY_DIR}" --check --cycles
                        --out "${CMAKE_BINARY_DIR}/selfhost-levelbench.tsv"
                        --json "${CMAKE_BINARY_DIR}/selfhost-levelbench.json")
                set_tests_properties(selfhost-optbench PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 RUN_SERIAL TRUE
                    TIMEOUT 10800 LABELS "optbench")
            else()
                set(_ladder_why "the -O ladder benchmarks take hours and are opt-in: cmake -DMCC_OPT_LADDER_BENCH=ON, then ctest -L optbench")
                mcc_skip_test(optlevel-bench "${_ladder_why}" LABELS optbench)
                mcc_skip_test(selfhost-optbench "${_ladder_why}" LABELS optbench)
            endif()
        else()
            mcc_skip_test(opt-cache-determinism
                "the -fopt-cache determinism gate recompiles src/mcc.c with the host mcc on a POSIX host; an emulated or cross build is not running the compiler under test"
                LABELS native)
            mcc_skip_test(optlevel-bench
                "the -O ladder benchmarks count instructions retired on the host they run on; an emulated or cross build has no such counter")
            mcc_skip_test(selfhost-optbench
                "the -O ladder benchmarks count instructions retired on the host they run on; an emulated or cross build has no such counter")
        endif()
    else()
        set(_nopy2_why "the bench/census/determinism drivers are python3 scripts and no python3 is on PATH")
        mcc_skip_test("docs/refs" "${_nopy2_why}")
        mcc_skip_test("docs/refs-known-positive" "${_nopy2_why}")
        mcc_skip_test("runtime-bench-check" "${_nopy2_why}")
        mcc_skip_test("optbench/null-subject" "${_nopy2_why}")
        mcc_skip_test("optbench/null-subject-known-positive" "${_nopy2_why}")
        mcc_skip_test("rir/bank-keying" "${_nopy2_why}")
        mcc_skip_test("rir/bank-keying-known-positive" "${_nopy2_why}")
        mcc_skip_test("rir/corpus-guard" "${_nopy2_why}")
        mcc_skip_test("rir/corpus-guard-known-positive" "${_nopy2_why}")
        mcc_skip_test("optbench/levelbench-bank" "${_nopy2_why}")
        mcc_skip_test("optbench/levelbench-bank-known-positive" "${_nopy2_why}")
        mcc_skip_test("fmt/census-bank" "${_nopy2_why}")
        mcc_skip_test("fmt/census-bank-known-positive" "${_nopy2_why}")
        mcc_skip_test("opt-determinism" "${_nopy2_why}")
        mcc_skip_test("opt-determinism-known-positive" "${_nopy2_why}")
        mcc_skip_test("opt-search-determinism" "${_nopy2_why}")
        mcc_skip_test("opt-search-determinism-known-positive" "${_nopy2_why}")
        mcc_skip_test("untyped-probe" "${_nopy2_why}")
        mcc_skip_test("untyped-probe-known-positive" "${_nopy2_why}")
        mcc_skip_test("runtime-bench-gatewin" "${_nopy2_why}")
        mcc_skip_test("opt-cache-determinism" "${_nopy2_why}")
        mcc_skip_test("optlevel-bench" "${_nopy2_why}")
        mcc_skip_test("selfhost-optbench" "${_nopy2_why}")
    endif()

    if(UNIX)
        add_test(NAME selfhost-riscv64-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-riscv64-docker.sh")
        set_tests_properties(selfhost-riscv64-docker PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77 TIMEOUT 1800)

        add_test(NAME selfhost-arm64-native
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-cross-native.sh" arm64)
        set_tests_properties(selfhost-arm64-native PROPERTIES FIXTURES_REQUIRED MCC_BUILT
            SKIP_RETURN_CODE 77 TIMEOUT 1800)

        add_test(NAME jit/run-parity-host
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-parity.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/parity")
        set_tests_properties(jit/run-parity-host PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 600)

        add_test(NAME jit/pool-cap
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-poolcap.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}")
        set_tests_properties(jit/pool-cap PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 120)

        add_test(NAME jit/budget
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-jitbudget.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}")
        set_tests_properties(jit/budget PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 120)

        # T-lin-10006: the __m512 / __m256h / __m128h intrinsic type spellings parse via
        # <immintrin.h> and compute (mcc lowers generically, no AVX-512 ISA), matching gcc.
        add_test(NAME simd/avx512-types
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/simd/run-avx512-types.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(simd/avx512-types PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 120)

        add_test(NAME jit/kgc-route-parity
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-kgcroute.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/parity")
        set_tests_properties(jit/kgc-route-parity PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 600)

        add_test(NAME jit/kgc-effect-parity
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-kgceffect.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}"
                    "${CMAKE_BINARY_DIR}/kgc-effect-parity"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/effects")
        set_tests_properties(jit/kgc-effect-parity PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 600)

        add_test(NAME jit/kgc-effect-parity-known-positive
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-kgceffect.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}"
                    "${CMAKE_BINARY_DIR}/kgc-effect-parity-kp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/effects-broken"
                    known-positive)
        set_tests_properties(jit/kgc-effect-parity-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 600)

        add_test(NAME jit/bind-local
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-bindlocal.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}"
                    "${CMAKE_BINARY_DIR}/jit-bind-local"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/bindlocal")
        set_tests_properties(jit/bind-local PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 600)

        add_test(NAME jit/bind-local-known-positive
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/run-bindlocal.sh"
                    "${_mcc_exe}" "${CMAKE_BINARY_DIR}"
                    "${CMAKE_BINARY_DIR}/jit-bind-local-kp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/jit/bindlocal"
                    known-positive)
        set_tests_properties(jit/bind-local-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 LABELS native TIMEOUT 600)

        add_test(NAME run-parity-riscv64
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-run-parity.sh" riscv64)
        set_tests_properties(run-parity-riscv64 PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT"
            SKIP_RETURN_CODE 77 RUN_SERIAL TRUE TIMEOUT 1800 ENVIRONMENT "MCC_CROSS_DIR=${MCC_CROSS_DIR}")

        add_test(NAME run-parity-arm
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-run-parity.sh" arm)
        set_tests_properties(run-parity-arm PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT"
            SKIP_RETURN_CODE 77 RUN_SERIAL TRUE TIMEOUT 1800 ENVIRONMENT "MCC_CROSS_DIR=${MCC_CROSS_DIR}")

        add_test(NAME run-parity-arm64
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/selfhost-run-parity.sh" arm64)
        set_tests_properties(run-parity-arm64 PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT"
            SKIP_RETURN_CODE 77 RUN_SERIAL TRUE TIMEOUT 1800 ENVIRONMENT "MCC_CROSS_DIR=${MCC_CROSS_DIR}")
    else()
        # gate-contract manifest gates registered only in this UNIX branch need a
        # host-skip twin so they read as host-skipped (not "declared but not
        # registered") on non-POSIX targets such as PE/Win32. See T-win-50001.
        mcc_skip_test(jit/kgc-effect-parity "the JIT kgc-effect parity gate needs a POSIX host build")
        mcc_skip_test(jit/kgc-effect-parity-known-positive "the JIT kgc-effect parity gate needs a POSIX host build")
        mcc_skip_test(jit/bind-local "the JIT bind-local gate needs a POSIX host build")
        mcc_skip_test(jit/bind-local-known-positive "the JIT bind-local gate needs a POSIX host build")
    endif()

    mcc_cross_cc(riscv64 _xcc_riscv64 _xfix_riscv64)
    if(NOT _xcc_riscv64 STREQUAL "")
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_extlink_host_plat "linux/arm64")
        else()
            set(_extlink_host_plat "linux/amd64")
        endif()
        add_test(NAME extlink-riscv64-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/extlink-docker.sh"
                "${_xcc_riscv64}" "${_extlink_host_plat}"
                "${CMAKE_BINARY_DIR}/extlink-riscv64-docker" "riscv64-linux-gnu-")
        set_tests_properties(extlink-riscv64-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_riscv64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME riscv64-promote-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/riscv64promo-docker.sh"
                "${CMAKE_BINARY_DIR}/riscv64-promote-docker")
        set_tests_properties(riscv64-promote-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_riscv64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME riscv64-inline-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/riscv64inline-docker.sh"
                "${CMAKE_BINARY_DIR}/riscv64-inline-docker")
        set_tests_properties(riscv64-inline-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_riscv64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME riscv64-divmagic-soak-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/riscv64divmagic-docker.sh"
                "${_xcc_riscv64}"
                "${CMAKE_BINARY_DIR}/riscv64-divmagic-soak-docker")
        set_tests_properties(riscv64-divmagic-soak-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_riscv64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME riscv64-const-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/riscv64const-docker.sh"
                "${_xcc_riscv64}"
                "${CMAKE_BINARY_DIR}/riscv64-const-docker")
        set_tests_properties(riscv64-const-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_riscv64}"
            SKIP_RETURN_CODE 77)

        add_test(NAME abi-diff-riscv64-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/abidiff-docker.sh"
                "riscv64" "${CMAKE_BINARY_DIR}/abi-diff-riscv64-docker")
        set_tests_properties(abi-diff-riscv64-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_riscv64}"
            SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(extlink-riscv64-docker
            "requires the mcc-riscv64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-riscv64)")
        mcc_skip_test(riscv64-promote-docker
            "requires the mcc-riscv64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-riscv64)")
        mcc_skip_test(riscv64-inline-docker
            "requires the mcc-riscv64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-riscv64)")
        mcc_skip_test(riscv64-divmagic-soak-docker
            "requires the mcc-riscv64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-riscv64)")
        mcc_skip_test(riscv64-const-docker
            "requires the mcc-riscv64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-riscv64)")
        mcc_skip_test(abi-diff-riscv64-docker
            "requires the mcc-riscv64 cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-riscv64)")
    endif()

    mcc_cross_cc(arm _xcc_arm _xfix_arm)
    if(NOT _xcc_arm STREQUAL "")
        add_test(NAME arm-inline-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/arminline-docker.sh"
                "${CMAKE_BINARY_DIR}/arm-inline-docker")
        set_tests_properties(arm-inline-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_arm}"
            SKIP_RETURN_CODE 77)

        add_test(NAME abi-diff-arm-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/abidiff-docker.sh"
                "arm" "${CMAKE_BINARY_DIR}/abi-diff-arm-docker")
        set_tests_properties(abi-diff-arm-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_arm}"
            SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(arm-inline-docker
            "requires the mcc-arm cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-arm)")
        mcc_skip_test(abi-diff-arm-docker
            "requires the mcc-arm cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-arm)")
    endif()

    mcc_cross_cc(arm _xcc_arm _xfix_arm)
    if(NOT _xcc_arm STREQUAL "")
        add_test(NAME arm-interwork-docker
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/arminterwork-docker.sh"
                "${CMAKE_BINARY_DIR}/arm-interwork-docker")
        set_tests_properties(arm-interwork-docker PROPERTIES FIXTURES_REQUIRED "${_xfix_arm}"
            SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(arm-interwork-docker
            "requires the mcc-arm cross compiler (build one with cmake --preset cross, or point -DMCC_CROSS_DIR at a tree that has mcc-arm)")
    endif()

    add_custom_target(gcctestsuite
        COMMAND "$<TARGET_FILE:mccharness>" gcctestsuite --mcc $<TARGET_FILE:mcc>
                --bdir "${CMAKE_BINARY_DIR}" --idir "${CMAKE_CURRENT_SOURCE_DIR}"
                --builddir "${CMAKE_BINARY_DIR}" --path "${MCC_GCCTESTSUITE_PATH}"
        DEPENDS mcc mccharness USES_TERMINAL
        COMMENT "gcctestsuite: gcc c-torture over mcc")

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
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/compile_errors.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/register_array_decay_accept.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/va_start_nonsym.c")

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
        if(_n STREQUAL "va_start_nonsym" AND MCC_TARGETOS STREQUAL "WIN32")
            mcc_skip_test(compile.${_n} "va_start with a non-lvalue second argument: the Win64 varargs ABI needs the named parameter's address to locate the overflow area, so mcc rejects it; SysV ignores the argument and accepts, so this compile-success orphan only holds on SysV")
            continue()
        endif()
        add_test(NAME compile.${_n} COMMAND ${MCC_EMULATOR} "${_mcc_exe}"
            "-B${_bdir}" "-I${_idir}" ${_orphan_x11_inc} -c "${_f}"
            -o "${CMAKE_BINARY_DIR}/orphan_${_n}.o")
        set_tests_properties(compile.${_n} PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
    endforeach()

    if(NOT MCC_EMULATOR)
        file(GLOB _dgerror_srcs CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/dg-error/*.c")
        foreach(_f IN LISTS _dgerror_srcs)
            get_filename_component(_n "${_f}" NAME_WE)
            if(_n MATCHES "_lin$" AND NOT CMAKE_HOST_UNIX)
                mcc_skip_test(diag.dg-error.${_n}
                    "case depends on POSIX device files under /dev")
                continue()
            endif()
            if(_n STREQUAL "scalar_storage_order_be" AND MCC_TARGETOS STREQUAL "WIN32")
                mcc_skip_test(diag.dg-error.${_n}
                    "no unsupported scalar-float SSO leaf exists on the Windows x64 ABI: long double is 64-bit (== double, swappable) and __float128 is not a target type; the reversed-scalar-leaf refusal is exercised on the SysV x86 (long double) and arm64/riscv64 (__float128) targets")
                continue()
            endif()
            add_test(NAME diag.dg-error.${_n} COMMAND "${CMAKE_COMMAND}"
                "-DMCC=${_mcc_exe}" "-DBDIR=${_bdir}" "-DSRC=${_f}"
                "-DOUT=${CMAKE_BINARY_DIR}/dgerror_${_n}.o"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/run_dgerror.cmake")
            set_tests_properties(diag.dg-error.${_n} PROPERTIES FIXTURES_REQUIRED MCC_BUILT)
        endforeach()
        if(CMAKE_HOST_UNIX)
            add_test(NAME diag.parse-depth
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/parse-depth.sh"
                        "${_mcc_exe}" "${_bdir}" "${CMAKE_BINARY_DIR}/parse-depth-work")
            set_tests_properties(diag.parse-depth PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                SKIP_RETURN_CODE 77 TIMEOUT 900)
        else()
            mcc_skip_test(diag.parse-depth
                "cell lowers its own stack rlimit; needs a POSIX shell")
        endif()

        find_program(MCC_OBJDUMP NAMES objdump)
        if(MCC_PYTHON3 AND MCC_OBJDUMP AND CMAKE_HOST_UNIX
           AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$"
           AND CMAKE_C_COMPILER_ID STREQUAL "GNU"
           AND NOT CMAKE_C_FLAGS MATCHES "-O[123gsz]"
           AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL ""))
            add_test(NAME diag.parse-frames
                COMMAND "${MCC_PYTHON3}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/parse-frames.py"
                        "${CMAKE_BINARY_DIR}" --mcc "${_mcc_exe}")
            set_tests_properties(diag.parse-frames PROPERTIES FIXTURES_REQUIRED MCC_BUILT
                TIMEOUT 300)
        else()
            mcc_skip_test(diag.parse-frames
                "prices MCC_MAX_PARSE_DEPTH from the x86-64 -O0 prologues of the built mcc; needs python3, objdump, an x86-64 host and an unoptimised gcc build")
        endif()
    else()
        mcc_skip_test(diag.dg-error
            "negative diagnostic tier runs mcc directly; not registered under an emulator")
        mcc_skip_test(diag.parse-depth
            "cell runs mcc directly; not registered under an emulator")
        mcc_skip_test(diag.parse-frames
            "cell reads host prologues out of the built mcc; not registered under an emulator")
    endif()

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

    add_test(NAME mcc_cross_build COMMAND "${CMAKE_COMMAND}"
        "-DXDIR=${MCC_CROSS_DIR}" "-DBDIR=${CMAKE_BINARY_DIR}"
        "-DXREQUIRED=${MCC_CROSS_REQUIRED}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cross_build.cmake")
    set_tests_properties(mcc_cross_build PROPERTIES
        FIXTURES_SETUP MCC_CROSS_BUILT TIMEOUT 3600)

    add_test(NAME pe-wine-conformance
        COMMAND "$<TARGET_FILE:mccharness>" pewine
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --work "${CMAKE_BINARY_DIR}/pe-wine-work")
    set_tests_properties(pe-wine-conformance PROPERTIES
        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "wine" SKIP_RETURN_CODE 77
        RESOURCE_LOCK "wine"
        ENVIRONMENT "MCC_WINE=${MCC_WINE};MCC_WINE_REQUIRED=${MCC_WINE_REQUIRED}"
        TIMEOUT 600)

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

    if(MCC_TARGETOS STREQUAL "WIN32" AND NOT CMAKE_CROSSCOMPILING
            AND MCC_CPU STREQUAL "x86_64" AND COMMAND mcc_mingw_resolve)
        mcc_mingw_resolve()
        if(_MCC_MINGW_X86_64_GCC AND EXISTS "${_MCC_MINGW_X86_64_GCC}")
            add_test(NAME pe/coff-obj-diff
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/coff-obj-diff.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_BINARY_DIR}/coff-obj-diff-work")
            set_tests_properties(pe/coff-obj-diff PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 300)
            get_filename_component(_mingw_bin0 "${_MCC_MINGW_X86_64_GCC}" DIRECTORY)
            add_test(NAME pe/unwind-backtrace
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-unwind-backtrace.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-unwind-backtrace.c"
                        "${CMAKE_BINARY_DIR}/pe-unwind-work"
                        "${_mingw_bin0}/objdump.exe")
            set_tests_properties(pe/unwind-backtrace PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 300)

            add_test(NAME pe/seh
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-seh.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-seh.c"
                        "${CMAKE_BINARY_DIR}/pe-seh-work")
            set_tests_properties(pe/seh PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 300)

            add_test(NAME pe/torture-classes
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-torture-classes.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-torture-classes.c"
                        "${CMAKE_BINARY_DIR}/pe-torture-classes-work")
            set_tests_properties(pe/torture-classes PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 300)

            add_test(NAME pe/ms-bitfield-sizing
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-msbitfield-sizing.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-msbitfield-sizing.c"
                        "${CMAKE_BINARY_DIR}/pe-msbitfield-sizing-work")
            set_tests_properties(pe/ms-bitfield-sizing PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 300)

            add_test(NAME pe/bitfield-abi
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-bitfield-abi.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-bitfield-abi.c"
                        "${CMAKE_BINARY_DIR}/pe-bitfield-abi-work")
            set_tests_properties(pe/bitfield-abi PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 300)

            set(_pe_xoracle_floor 250)
            add_test(NAME pe/x-oracle
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-xoracle.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_BINARY_DIR}/pe-xoracle-work" "${_pe_xoracle_floor}")
            set_tests_properties(pe/x-oracle PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 900)
            add_test(NAME pe/x-oracle-mutate
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-xoracle.sh"
                        "$<TARGET_FILE:mcc>" "${_MCC_MINGW_X86_64_GCC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_BINARY_DIR}/pe-xoracle-mutate-work" "${_pe_xoracle_floor}"
                        --mutate)
            set_tests_properties(pe/x-oracle-mutate PROPERTIES
                FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                LABELS "pe" TIMEOUT 900)

            get_filename_component(_mingw_bin "${_MCC_MINGW_X86_64_GCC}" DIRECTORY)
            if(EXISTS "${_mingw_bin}/objdump.exe" AND EXISTS "${_mingw_bin}/addr2line.exe")
                add_test(NAME pe/dwarf-lines
                    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-dwarf-lines.sh"
                            "$<TARGET_FILE:mcc>" "${_mingw_bin}/objdump.exe"
                            "${_mingw_bin}/addr2line.exe"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-dwarf-lines.c"
                            "${CMAKE_BINARY_DIR}/pe-dwarf-lines-work")
                set_tests_properties(pe/dwarf-lines PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    LABELS "pe" TIMEOUT 300)
            else()
                mcc_skip_test(pe/dwarf-lines
                    "PE DWARF line-table conformance (needs vendored mingw objdump+addr2line)")
            endif()

            find_program(MCC_LLVM_READOBJ NAMES llvm-readobj)
            find_program(MCC_LLD_LINK NAMES lld-link)
            find_program(MCC_LLVM_PDBUTIL NAMES llvm-pdbutil)
            if(MCC_LLVM_READOBJ)
                add_test(NAME pe/codeview
                    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-codeview.sh"
                            "$<TARGET_FILE:mcc>" "${MCC_LLVM_READOBJ}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross/pe-dwarf-lines.c"
                            "${CMAKE_BINARY_DIR}/pe-codeview-work"
                            "${MCC_LLD_LINK}" "${MCC_LLVM_PDBUTIL}")
                set_tests_properties(pe/codeview PROPERTIES
                    FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77
                    LABELS "pe" TIMEOUT 300)
            else()
                mcc_skip_test(pe/codeview
                    "CodeView .debug$S conformance (needs llvm-readobj)")
            endif()
        else()
            mcc_skip_test(pe/coff-obj-diff
                "COFF object writer differential (needs vendored mingw-w64 x86_64 gcc)")
            mcc_skip_test(pe/unwind-backtrace
                "native RtlVirtualUnwind conformance (needs vendored mingw-w64 x86_64 gcc)")
            mcc_skip_test(pe/seh
                "needs vendored mingw-w64 x86_64 gcc")
            mcc_skip_test(pe/torture-classes
                "needs vendored mingw-w64 x86_64 gcc")
            mcc_skip_test(pe/ms-bitfield-sizing
                "needs vendored mingw-w64 x86_64 gcc")
            mcc_skip_test(pe/bitfield-abi
                "needs vendored mingw-w64 x86_64 gcc")
            mcc_skip_test(pe/x-oracle
                "needs vendored mingw-w64 x86_64 gcc")
            mcc_skip_test(pe/x-oracle-mutate
                "needs vendored mingw-w64 x86_64 gcc")
            mcc_skip_test(pe/dwarf-lines
                "needs vendored mingw-w64 x86_64 gcc")
            mcc_skip_test(pe/codeview
                "needs vendored mingw-w64 x86_64 gcc")
        endif()
    else()
        mcc_skip_test(pe/coff-obj-diff
            "COFF object writer differential (native x86_64 WIN32 host with mingw only)")
        mcc_skip_test(pe/unwind-backtrace
            "native RtlVirtualUnwind conformance (native x86_64 WIN32 host with mingw only)")
        mcc_skip_test(pe/seh
            "native x86_64 WIN32 host with mingw only")
        mcc_skip_test(pe/torture-classes
            "native x86_64 WIN32 host with mingw only")
        mcc_skip_test(pe/ms-bitfield-sizing
            "native x86_64 WIN32 host with mingw only")
        mcc_skip_test(pe/bitfield-abi
            "native x86_64 WIN32 host with mingw only")
        mcc_skip_test(pe/x-oracle
            "native x86_64 WIN32 host with mingw only")
        mcc_skip_test(pe/x-oracle-mutate
            "native x86_64 WIN32 host with mingw only")
        mcc_skip_test(pe/dwarf-lines
            "native x86_64 WIN32 host with mingw only")
        mcc_skip_test(pe/codeview
            "native x86_64 WIN32 host with mingw only")
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
        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_test(NAME macho-weakdef
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/weakdef.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-weakdef-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-weakdef PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-weakdef
            "the trie/symtab pairing needs a linked Mach-O image, which needs a Darwin host")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # T-mac-30098: the Mach-O object reader must map N_WEAK_DEF/N_WEAK_REF
        # to STB_WEAK, or an external clang/system .o with weak symbols is
        # unlinkable ("defined twice").
        add_test(NAME macho-weak-read
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/weak-read.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-weak-read-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-weak-read PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)
        add_test(NAME macho-weak-read-known-positive
            COMMAND "${CMAKE_COMMAND}" -E env "WEAK_READ_MUTATE=1"
                    sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/weak-read.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-weak-read-kp-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-weak-read-known-positive PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-weak-read
            "linking external weak-symbol objects into a Mach-O image needs a Darwin host")
        mcc_skip_test(macho-weak-read-known-positive
            "linking external weak-symbol objects into a Mach-O image needs a Darwin host")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # T-mac-30157: hidden/internal visibility → Mach-O private_extern.
        add_test(NAME macho-visibility
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/visibility.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-visibility-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-visibility PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-visibility
            "checking private_extern via nm -m needs a linked Mach-O image on a Darwin host")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_test(NAME macho-clanglink
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/clanglink.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-clanglink-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-clanglink PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-clanglink
            "running both the clang-linked and the mcc-linked image needs a Darwin host")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_test(NAME macho-tlv
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/tlv.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-tlv-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-tlv PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-tlv
            "linking and running a Darwin TLV image needs a Darwin host")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_test(NAME macho-tlv-x86_64
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/tlv.sh"
                    "${MCC_CROSS_DIR}/mcc-x86_64-osx" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-tlv-x86_64-work" "-B${MCC_CROSS_DIR}"
                    x86_64)
        set_tests_properties(macho-tlv-x86_64 PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-tlv-x86_64
            "linking and running a Darwin TLV image needs a Darwin host")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_test(NAME macho-adhocsign
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/adhocsign.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-adhocsign-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-adhocsign PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-adhocsign
            "verifying a code signature needs a Darwin host (kernel acceptance + codesign -v)")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_test(NAME macho-entry
            COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/entry.sh"
                    "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-entry-work" "-B${CMAKE_BINARY_DIR}")
        set_tests_properties(macho-entry PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-entry
            "running a Mach-O EXE with a custom -e/--entry needs a Darwin host")
    endif()

    if(UNIX)
        add_test(NAME macho-archive
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/archive.sh"
                    "${MCC_CROSS_DIR}/mcc-x86_64-osx" "${MCC_CROSS_DIR}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/make_fixture.py"
                    "${CMAKE_BINARY_DIR}/macho-archive-work")
        set_tests_properties(macho-archive PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77
            TIMEOUT 300)
    endif()

    if(UNIX)
        add_test(NAME macho-reloc
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/reloc.sh"
                    "${MCC_CROSS_DIR}/mcc-x86_64-osx" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-reloc-work")
        set_tests_properties(macho-reloc PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77
            TIMEOUT 300)
    endif()

    if(UNIX)
        add_test(NAME macho-reloc-arm64
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/reloc-arm64.sh"
                    "${MCC_CROSS_DIR}/mcc-arm64-osx" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-reloc-arm64-work")
        set_tests_properties(macho-reloc-arm64 PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77
            TIMEOUT 300)
    endif()

    if(UNIX)
        add_test(NAME macho-archive-reloc
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/archive-reloc.sh"
                    "${MCC_CROSS_DIR}/mcc-x86_64-osx" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-archive-reloc-work")
        set_tests_properties(macho-archive-reloc PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77
            TIMEOUT 300)
    endif()

    if(UNIX)
        add_test(NAME objsec-reject-malformed
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/objsec/reject-malformed.sh"
                    "$<TARGET_FILE:mcc>" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/objsec-reject-malformed-work")
        set_tests_properties(objsec-reject-malformed PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "objsec" SKIP_RETURN_CODE 77
            TIMEOUT 120)
    endif()

    if(UNIX)
        add_test(NAME headers-unreachable-c23
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/headers/unreachable-c23.sh"
                    "$<TARGET_FILE:mcc>"
                    "${CMAKE_BINARY_DIR}/headers-unreachable-c23-work")
        set_tests_properties(headers-unreachable-c23 PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "headers" SKIP_RETURN_CODE 77
            TIMEOUT 60)

        add_test(NAME diag.return-type-gating
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/return-type-gating.sh"
                    "$<TARGET_FILE:mcc>"
                    "${CMAKE_BINARY_DIR}/diag-return-type-gating-work")
        set_tests_properties(diag.return-type-gating PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "diagnostics" SKIP_RETURN_CODE 77
            TIMEOUT 60)

        add_test(NAME diag.case-range-warn
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/case-range-warn.sh"
                    "$<TARGET_FILE:mcc>"
                    "${CMAKE_BINARY_DIR}/diag-case-range-warn-work")
        set_tests_properties(diag.case-range-warn PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "diagnostics" SKIP_RETURN_CODE 77
            TIMEOUT 60)

        add_test(NAME diag.pragma-weak
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/pragma-weak.sh"
                    "$<TARGET_FILE:mcc>"
                    "${CMAKE_BINARY_DIR}/diag-pragma-weak-work")
        set_tests_properties(diag.pragma-weak PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "diagnostics" SKIP_RETURN_CODE 77
            TIMEOUT 60)

        add_test(NAME diag.format-attribute
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/diagnostics/format-attribute.sh"
                    "$<TARGET_FILE:mcc>"
                    "${CMAKE_BINARY_DIR}/diag-format-attribute-work")
        set_tests_properties(diag.format-attribute PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT LABELS "diagnostics" SKIP_RETURN_CODE 77
            TIMEOUT 60)
    endif()

    if(UNIX)
        foreach(_ma x86_64 arm64)
            add_test(NAME macho-got-sub-${_ma}
                COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/got-sub.sh"
                        "${MCC_CROSS_DIR}/mcc-${_ma}-osx" "${MCC_CROSS_DIR}"
                        "${CMAKE_BINARY_DIR}/macho-got-sub-${_ma}-work" "${_ma}")
            set_tests_properties(macho-got-sub-${_ma} PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77
                TIMEOUT 300)
        endforeach()
    endif()

    if(UNIX)
        add_test(NAME macho-embedjit-arm64-osx
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/macho/embedjit.sh"
                    "${MCC_CROSS_DIR}/mcc-arm64-osx" "${MCC_CROSS_DIR}"
                    "${CMAKE_BINARY_DIR}/macho-embedjit-arm64-work" arm64)
        set_tests_properties(macho-embedjit-arm64-osx PROPERTIES
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77
            TIMEOUT 300)
    endif()

    if(UNIX)
        add_test(NAME cli/march-isa
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/march-isa.sh"
                    "${_mcc_exe}" "${_bdir}"
                    "${CMAKE_BINARY_DIR}/march-isa-work")
        set_tests_properties(cli/march-isa PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)
    endif()

    if(UNIX)
        add_test(NAME cli/function-sections
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/function-sections.sh"
                    "${_mcc_exe}" "${_bdir}"
                    "${CMAKE_BINARY_DIR}/function-sections-work")
        set_tests_properties(cli/function-sections PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)
    endif()

    if(UNIX AND MCC_CC_NAME STREQUAL "mcc" AND CMAKE_GENERATOR MATCHES "Ninja"
       AND NOT CMAKE_VERSION VERSION_LESS "4.4")
        add_test(NAME build/mcc-depfile-wired
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/build/depfile-wired.sh"
                    "${CMAKE_BINARY_DIR}")
        set_tests_properties(build/mcc-depfile-wired PROPERTIES
            FIXTURES_REQUIRED MCC_BUILT TIMEOUT 60)
    else()
        mcc_skip_test(build/mcc-depfile-wired
            "T-lin-10068 mcc depfile wiring; runs only in an mcc-profile Ninja build on CMake>=4.4")
    endif()

    add_test(NAME macho-codegen-run
        COMMAND "$<TARGET_FILE:mccharness>" machocodegen
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --work "${CMAKE_BINARY_DIR}/macho-codegen-work")
    set_tests_properties(macho-codegen-run PROPERTIES
        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-image-run
        COMMAND "$<TARGET_FILE:mccharness>" machoimage
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --objcheck "$<TARGET_FILE:objcheck>"
                --work "${CMAKE_BINARY_DIR}/macho-image-work")
    set_tests_properties(macho-image-run PROPERTIES
        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-apple-libc
        COMMAND "$<TARGET_FILE:mccharness>" machoapplelibc
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --xb "${MCC_CROSS_DIR}"
                --objcheck "$<TARGET_FILE:objcheck>"
                --work "${CMAKE_BINARY_DIR}/macho-apple-libc-work")
    set_tests_properties(macho-apple-libc PROPERTIES
        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-conformance-native
        COMMAND "$<TARGET_FILE:mccharness>" machonative
                --src "${CMAKE_CURRENT_SOURCE_DIR}" --mcc "$<TARGET_FILE:mcc>"
                --objcheck "$<TARGET_FILE:objcheck>"
                --bdir "${CMAKE_BINARY_DIR}" --work "${CMAKE_BINARY_DIR}/macho-native-work")
    set_tests_properties(macho-conformance-native PROPERTIES
        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)

    add_test(NAME macho-stack-protector
        COMMAND "$<TARGET_FILE:mccharness>" stackguard
                --mcc "$<TARGET_FILE:mcc>" --objcheck "$<TARGET_FILE:objcheck>"
                --bdir "${CMAKE_BINARY_DIR}" --work "${CMAKE_BINARY_DIR}/stackguard-work")
    set_tests_properties(macho-stack-protector PROPERTIES
        FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)

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
            FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77)
    else()
        mcc_skip_test(macho-universal
            "the machofat tool is POSIX-only (not built on a WIN32 host)")
    endif()

    option(MCC_DARWIN_HOST "Tests run on a macOS/darling host with real libSystem" OFF)
    if(MCC_DARWIN_HOST AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        foreach(_dt malloc stdio dyld pthread gcd mach objc)
            set(_dt_extra "")
            if(_dt STREQUAL "objc")
                set(_dt_extra -lobjc)
            endif()
            add_test(NAME "macho-libsystem/${_dt}"
                COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/darwin/run.sh"
                        "$<TARGET_FILE:mcc>" "${CMAKE_CURRENT_SOURCE_DIR}"
                        "${CMAKE_BINARY_DIR}/darwin-libsystem-work"
                        "libsystem_${_dt}" "-B${CMAKE_BINARY_DIR}" ${_dt_extra})
            set_tests_properties("macho-libsystem/${_dt}" PROPERTIES
                FIXTURES_REQUIRED "MCC_BUILT;MCC_CROSS_BUILT" LABELS "macho" SKIP_RETURN_CODE 77
                TIMEOUT 300)
        endforeach()
    else()
        foreach(_dt malloc stdio dyld pthread gcd mach objc)
            mcc_skip_test("macho-libsystem/${_dt}"
                "libmalloc/locale-stdio/dyld/pthread/GCD/ObjC need a macOS or darling host (-DMCC_DARWIN_HOST=ON); kernel-fused, no portable variant"
                LABELS macho)
        endforeach()
    endif()

    option(MCC_QEMU_TESTS
        "Download Gentoo stage3 rootfs and run cross conformance tests under qemu-user" OFF)
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

            set(_qwhy "")
            if(NOT MCC_QEMU_TESTS)
                set(_qwhy "the qemu-user conformance matrix is opt-in because it downloads a Gentoo stage3 rootfs per arch/libc: cmake -DMCC_QEMU_TESTS=ON, then ctest -L qemu")
            elseif(_qcc STREQUAL "")
                set(_qwhy "no compiler for ${_arch} (native target is ${MCC_CPU}; build mcc-${_arch} via -DMCC_ENABLE_CROSS=ON)")
            elseif(NOT _QEMU_${_arch})
                set(_qwhy "${_qbin} not found (install the qemu-user package)")
            endif()

            set(_root "${MCC_QEMU_DLDIR}/gentoo-stage3-${_arch}-${_libc}")
            if(_qwhy STREQUAL "")
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
                    --srcdir "${_qsrc}" --arch "${_arch}"
                    --workdir "${CMAKE_BINARY_DIR}/qemu-work/${_arch}-${_libc}")
                set_tests_properties(${_tn} PROPERTIES
                    FIXTURES_REQUIRED "MCC_BUILT;FX_${_tn}" LABELS qemu TIMEOUT 600)

                foreach(_qopt IN ITEMS O2 O3 Os)
                    add_test(NAME ${_tn}-${_qopt} COMMAND "$<TARGET_FILE:mccharness>" qemurun
                        --mcc "${_qcc}" --mccbase "${CMAKE_BINARY_DIR}"
                        --sysroot "${_root}" --qemu "${_QEMU_${_arch}}"
                        --srcdir "${_qsrc}" --arch "${_arch}" --opt "-${_qopt}"
                        --workdir "${CMAKE_BINARY_DIR}/qemu-work/${_arch}-${_libc}-${_qopt}")
                    set_tests_properties(${_tn}-${_qopt} PROPERTIES
                        FIXTURES_REQUIRED "MCC_BUILT;FX_${_tn}" LABELS qemu TIMEOUT 600)
                endforeach()
            else()
                mcc_skip_test(${_tn}-fetch "${_qwhy}" LABELS qemu)
                mcc_skip_test(${_tn} "${_qwhy}" LABELS qemu)
                foreach(_qopt IN ITEMS O2 O3 Os)
                    mcc_skip_test(${_tn}-${_qopt} "${_qwhy}" LABELS qemu)
                endforeach()
            endif()

            if(NOT _arch STREQUAL MCC_CPU)
                if(_qwhy STREQUAL "")
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
                        ENVIRONMENT "MCC_TEST_EMU=;MCC_TEST_RUNEMU=${_QEMU_${_arch}} -L ${_root};MCC_TEST_SYSROOT=${_root};MCC_TEST_CPU=${_arch};MCC_TEST_OS=${MCC_TARGETOS};MCC_TEST_BCHECK=0;MCC_TEST_BACKTRACE=${_mcc_diag_bt}")
                else()
                    mcc_skip_test(${_tn}-exec "${_qwhy}" LABELS qemu)
                endif()
            endif()
        endforeach()
    endforeach()

    if(MCC_QEMU_TESTS
       AND (";${MCC_QEMU_ARCHS};" MATCHES ";arm64;")
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
            "requires -DMCC_QEMU_TESTS=ON with arm64+glibc in the matrix, and the arm64-osx cross compiler (cmake --preset cross)"
            LABELS qemu)
    endif()

    if(MCC_QEMU_TESTS)
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

    set(MCC_CI_DOCKER_PRESET "linux-gcc" CACHE STRING
        "CMake preset the `ci-docker` target runs inside the Linux CI image")
    set(MCC_CI_DOCKER_PLATFORM "" CACHE STRING
        "docker --platform for `ci-docker` (e.g. linux/amd64 to get 32-bit wine \
and the x86 PE cells on an arm64 workstation; empty = native)")
    set(_cid_plat "")
    if(MCC_CI_DOCKER_PLATFORM)
        list(APPEND _cid_plat --platform "${MCC_CI_DOCKER_PLATFORM}")
    endif()
    add_custom_target(ci-docker
        COMMAND "${DOCKER_EXE}" build ${_cid_plat} -t mcc-ci-linux
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ci/docker"
        COMMAND "${DOCKER_EXE}" run --rm ${_cid_plat}
                -e "PRESET=${MCC_CI_DOCKER_PRESET}"
                -v "${CMAKE_CURRENT_SOURCE_DIR}:/work"
                -v "${MCC_VENDOR_DIR}:/vendor"
                -v "${MCC_DIST_DIR}:/dist"
                mcc-ci-linux
        USES_TERMINAL VERBATIM
        COMMENT "Linux CI preset ${MCC_CI_DOCKER_PRESET} in Docker (image mcc-ci-linux)")
else()
    message(STATUS "mcc: docker not found — `qemu-docker`/`ci-docker` targets unavailable")
endif()

get_property(_mcc_all_tests DIRECTORY PROPERTY TESTS)
foreach(_t IN LISTS _mcc_all_tests)
    get_property(_l TEST "${_t}" PROPERTY LABELS)
    if(_t MATCHES "-docker$" AND NOT "docker" IN_LIST _l)
        set_property(TEST "${_t}" APPEND PROPERTY LABELS docker)
        list(APPEND _l docker)
    endif()
    set(_mcc_emu OFF)
    foreach(_el IN ITEMS qemu wine macho docker)
        if("${_el}" IN_LIST _l)
            set(_mcc_emu ON)
        endif()
    endforeach()
    if(NOT _mcc_emu)
        set_property(TEST "${_t}" APPEND PROPERTY LABELS native)
    endif()
endforeach()
unset(_mcc_emu)

set(_mcc_headless 0)
foreach(_t IN LISTS _mcc_all_tests)
    get_property(_hl TEST "${_t}" PROPERTY LABELS)
    if(NOT "wine" IN_LIST _hl)
        set_property(TEST "${_t}" APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")
        math(EXPR _mcc_headless "${_mcc_headless} + 1")
    endif()
endforeach()
if(MCC_BUILD_TESTS AND _mcc_headless LESS 9000)
    message(FATAL_ERROR
        "only ${_mcc_headless} cell(s) were scrubbed of DISPLAY, below the floor of 9000. "
        "Any cell that brings a Vulkan instance up reaches an ICD whose X11 connection has no "
        "timeout, so one unresponsive display server wedges it for as long as ctest will wait, "
        "and slicerun reports that as an empty funnel rather than as a hang. mcc requests zero "
        "instance extensions and never needs a display; only the wine cells do")
endif()
unset(_mcc_headless)
unset(_hl)
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
# tools/strategy-ledger.sh publishes which optimizer strategies actually fire
# on the exec corpus and was registered nowhere, so a strategy that stopped
# firing said nothing to anyone. This is the ratchet for it: every banked FIRES
# row must still fire. One-directional -- a new arrival is reported, not failed.
if(UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR
   AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TARGETOS STREQUAL "Darwin")
    add_test(NAME "optfire/strategy-ledger"
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/strategy-ledger-check.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/strategy-ledger.sh"
                "${CMAKE_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/strategy-ledger.txt"
                "${CMAKE_BINARY_DIR}/strategy-ledger")
    set_tests_properties("optfire/strategy-ledger" PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800)
else()
    mcc_skip_test("optfire/strategy-ledger"
        "the strategy fire ledger runs the exec corpus natively (native ELF host only)")
endif()

# tools/gate-ledger.sh does the same job one level down: for every MCC_OPT_ROW
# in src/mccopt.h it toggles that one gate across the exec corpus and records
# whether the recorded AST moved, only the object moved, or nothing moved. Also
# registered nowhere until now. The bank is the set of gates that still do
# something; a banked gate going dark fails, a new one lighting up is reported.
if(UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR
   AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TARGETOS STREQUAL "Darwin")
    add_test(NAME "optfire/gate-ledger"
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/gate-ledger-check.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/gate-ledger.sh"
                "${CMAKE_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/optfire/gate-ledger.txt"
                "${CMAKE_BINARY_DIR}/gate-ledger")
    set_tests_properties("optfire/gate-ledger" PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 1800)
else()
    mcc_skip_test("optfire/gate-ledger"
        "the per-gate fire ledger runs the exec corpus natively (native ELF host only)")
endif()

# tools/xsuite-report.py summarizes a tools/xsuite.py results.jsonl and feeds
# figures onto docs/TODO.md. It cannot be run for real here -- xsuite.py needs
# external gcc and llvm checkouts and no results.jsonl is committed -- so what
# is gated is the formatter, against a fixture holding the shapes it used to
# report wrongly: a suite/opt pair that never ran (was printed as 0.0%, as if
# the suite failed everything) and a pair that ran and genuinely passed nothing
# (must still print 0.0%). No compiler needed, so this runs everywhere python3
# does.
if(UNIX)
    add_test(NAME "xsuite/report"
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/xsuite/report-check.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/xsuite-report.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/xsuite/report-fixture.jsonl"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/xsuite/report-golden.txt"
                "${CMAKE_BINARY_DIR}/xsuite-report")
    set_tests_properties("xsuite/report" PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 120)
else()
    mcc_skip_test("xsuite/report" "the xsuite report check needs a POSIX shell")
endif()

# The process-lifetime bracket (src/mccrt.h): the GPU device and the JIT worker
# pool start at main's entry and stop at its exit, instead of starting lazily and
# stopping from atexit handlers whose order depended on which lazy path ran
# first. Needs no device -- it asserts that a boot was ATTEMPTED, never that one
# succeeded, so it means the same thing on a host without a GPU.
if(UNIX)
    add_test(NAME "gpu/lifecycle"
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/gpu/lifecycle.sh"
                "$<TARGET_FILE:mcc>" "${CMAKE_BINARY_DIR}/gpu-lifecycle")
    set_tests_properties("gpu/lifecycle" PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 300)
else()
    mcc_skip_test("gpu/lifecycle" "the lifetime bracket check needs a POSIX shell")
endif()

# N7's fix: an INDEPENDENT oracle for ast_eval_binop().
#
# That function is reached ~66M times compiling tests/smoke/subject.c at -O4, and
# a deliberate `r = s + 1` injected into its 32-bit signed `+` arm changed
# NOTHING observable -- pairs/certified/differ, all 1782 --dump rows on all six
# engines, the sweep digest and eleven smoke cells were byte-identical. The
# differential compares two arenas that are both evaluated by it, so a shared
# fault cancels by construction; self-comparison cannot see it.
#
# This compiles the arithmetic half of src/ast_eval_slice.h on its own (behind
# its AST_EVAL_SLICE_ARITH_ONLY guard) with the HOST compiler, links no part of
# mcc, and compares every opcode against a reference computed a different way in
# 128-bit arithmetic. It catches the injected fault on the first case it tries.
if(UNIX)
    add_executable(ast_eval_binop_oracle
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/eval_binop_oracle.c")
    add_test(NAME "ast/eval-binop-oracle" COMMAND ast_eval_binop_oracle)
    add_test(NAME "ast/eval-binop-oracle-known-positive"
        COMMAND "${MCC_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/eval_binop_oracle_kp.py"
                "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}"
                "${CMAKE_C_COMPILER}")
    set_tests_properties("ast/eval-binop-oracle-known-positive" PROPERTIES
        TIMEOUT 300)
    # mcc_build builds the whole tree, so it is what makes this executable exist
    # on a cold ctest; without the fixture the cell reports "Not Run".
    set_tests_properties("ast/eval-binop-oracle" PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT TIMEOUT 300)
else()
    mcc_skip_test("ast/eval-binop-oracle" "the binop oracle needs a POSIX host build")
    mcc_skip_test("ast/eval-binop-oracle-known-positive" "the binop oracle needs a POSIX host build")
endif()

# tools/c2_sweep.sh totals the per-body Replay-IR counters over every .c under
# tests/ and publishes them onto docs/TODO.md. Last of the four tools that were
# registered nowhere. The census half (fn/faithful/arenafn/ok) gets floors; the
# c2* half is behind the MCC_REPLAY_IR_C2 build option and compiled out by
# default, so instead of banking a vacuous zero on it the check asserts the tool
# declares which state it is in.
if(UNIX AND NOT CMAKE_CROSSCOMPILING AND NOT MCC_EMULATOR
   AND NOT MCC_TARGETOS STREQUAL "WIN32" AND NOT MCC_TARGETOS STREQUAL "Darwin"
   AND MCC_CPU STREQUAL "x86_64")
    add_test(NAME "rir/c2-sweep"
        COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/rir/c2-sweep-check.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/c2_sweep.sh"
                "${CMAKE_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/rir/c2-sweep-floor.txt"
                "${CMAKE_BINARY_DIR}/c2-sweep")
    set_tests_properties("rir/c2-sweep" PROPERTIES
        FIXTURES_REQUIRED MCC_BUILT SKIP_RETURN_CODE 77 TIMEOUT 900)
else()
    mcc_skip_test("rir/c2-sweep"
        "the c2 sweep floors are x86_64 native counts (native x86_64 ELF host only)")
endif()

# One device, one lock -- OFF by default, because it was measured.
#
# slice/quiesce tears the Vulkan device down while every other device cell is
# using it, so the two cannot share a machine; that is why it produced 55/55 and
# 54/55 on two runs of the same binary at the same commit. RESOURCE_LOCK is the
# honest fix for that, and this is what it costs on x86_64-linux:
#
#   18 device cells at -j6, three runs each
#     without the lock   90.4 / 49.9 / 52.6 s   mean ~64 s, spread 1.8x
#     with the lock     188.7 / 181.4 / 182.2 s mean ~184 s, spread 1.04x
#
# So it buys determinism for about 2.9x wall on that set. It is off by default
# because the failure it prevents did not occur: a full -j6 ctest over all 9937
# cells on 2026-08-13 had ZERO timeouts and slice/quiesce passed, so the 900 s
# smoke-plus-slice timeout this was meant to stop is not currently reproducible
# here. Turn it on with -DMCC_DEVICE_LOCK=ON when the flake returns, or on a
# host where the device is slower or shared; the numbers above are what to
# expect. The names are matched rather than listed because most are generated
# in loops, and a list would silently stop covering a cell the day one is added.
option(MCC_DEVICE_LOCK
    "Serialise device-using tests through one ctest RESOURCE_LOCK (see the note above; ~2.9x wall on the device set)" OFF)
if(MCC_DEVICE_LOCK)
    get_property(_mcc_all_tests DIRECTORY PROPERTY TESTS)
    set(_mcc_device_tests "")
    foreach(_t IN LISTS _mcc_all_tests)
        if(_t MATCHES "^(gpu/|smoke/device)" OR _t STREQUAL "slice/quiesce"
           OR _t STREQUAL "slice/cost")
            list(APPEND _mcc_device_tests "${_t}")
        endif()
    endforeach()
    if(_mcc_device_tests)
        set_tests_properties(${_mcc_device_tests} PROPERTIES RESOURCE_LOCK "mcc_device")
        list(LENGTH _mcc_device_tests _mcc_ndev)
        message(STATUS "mcc tests: ${_mcc_ndev} device cell(s) share RESOURCE_LOCK mcc_device")
    endif()
endif()

# T-lin-10003: the tree-wide gates, as one label. These are the cells that any
# edit anywhere is subject to -- they scan the whole source tree, read a bank of
# figures quoted on the board, or assert the suite still registers what it says
# it does -- as opposed to the cells that test the slice you happened to touch.
# Three tasks in one afternoon (T-lin-10360, T-lin-10361, T-lin-10362) went DONE
# and archived with one of these red behind them, in every case by a session
# that had correctly verified its own slice. `ctest -L treegate` is the whole
# set in about half a minute and is what to run before a DONE commit.
# Named individually and not by pattern: membership is a judgement about what a
# cell measures, and a regex over names would quietly acquire cells nobody meant
# to put in the pre-commit path.
set(MCC_TREEGATE_CELLS
    "ci/must-run-registered" "ci/gate-contract" "ci/gate-contract-known-positive"
    "ci/registration-stubs" "census/gates-armed" "trace-gate-invariant"
    "trace-gate-known-positive"
    "fmt/census-bank" "fmt/census-bank-known-positive" "fmt/census-oracle"
    "docs/refs" "docs/refs-known-positive")
# The second tier. `treegate` is bounded at ~5 s a cell so that it is always
# worth running, and that bound is exactly why it cannot catch a corpus change:
# every cell that would notice a file appearing under tests/ has to walk the
# corpus, and walking the corpus costs minutes. T-lin-10363 is that gap arriving
# in practice -- one new exec fixture moved ast/o0-baseline from files=311 to
# 312 and left rir-coverage-census with 43 unnoted functions, and `-L treegate`
# was green over both. Run this one when you add or remove a file under tests/,
# or when you change codegen: `ctest -L corpusgate`, about four minutes.
set(MCC_CORPUSGATE_CELLS
    "ast/o0-baseline" "ast/o0-baseline-known-positive"
    "ast/o0-baseline-gated" "ast/o0-baseline-gated-known-positive"
    "rir-coverage-census")
set(_mcc_treegate_seen "")
foreach(_tg IN LISTS MCC_TREEGATE_CELLS)
    if(TEST "${_tg}")
        get_test_property("${_tg}" LABELS _tg_labels)
        if(NOT _tg_labels)
            set(_tg_labels "")
        endif()
        list(APPEND _tg_labels treegate)
        list(REMOVE_DUPLICATES _tg_labels)
        set_tests_properties("${_tg}" PROPERTIES LABELS "${_tg_labels}")
        list(APPEND _mcc_treegate_seen "${_tg}")
    endif()
endforeach()
set(_mcc_corpusgate_seen "")
foreach(_cg IN LISTS MCC_CORPUSGATE_CELLS)
    if(TEST "${_cg}")
        get_test_property("${_cg}" LABELS _cg_labels)
        if(NOT _cg_labels)
            set(_cg_labels "")
        endif()
        list(APPEND _cg_labels corpusgate)
        list(REMOVE_DUPLICATES _cg_labels)
        set_tests_properties("${_cg}" PROPERTIES LABELS "${_cg_labels}")
        list(APPEND _mcc_corpusgate_seen "${_cg}")
    endif()
endforeach()
list(LENGTH _mcc_treegate_seen _mcc_ntreegate)
list(LENGTH MCC_TREEGATE_CELLS _mcc_ntreegate_want)
if(_mcc_ntreegate LESS _mcc_ntreegate_want)
    message(FATAL_ERROR "mcc tests: the treegate label covers ${_mcc_ntreegate} "
        "of ${_mcc_ntreegate_want} named cells. Every branch of a capability gate "
        "registers the same cells (ci/registration-stubs), so a name here that no "
        "branch registers is a typo or a deleted cell, and letting it pass would "
        "make `ctest -L treegate` quietly check less than it says")
endif()
list(LENGTH _mcc_corpusgate_seen _mcc_ncorpusgate)
list(LENGTH MCC_CORPUSGATE_CELLS _mcc_ncorpusgate_want)
if(_mcc_ncorpusgate LESS _mcc_ncorpusgate_want)
    message(FATAL_ERROR "mcc tests: the corpusgate label covers "
        "${_mcc_ncorpusgate} of ${_mcc_ncorpusgate_want} named cells, for the "
        "same reason the treegate list must be whole")
endif()
message(STATUS "mcc tests: ${_mcc_ntreegate} tree-wide gate cell(s) labelled "
    "treegate, ${_mcc_ncorpusgate} corpus-sensitive labelled corpusgate")

include(CPack)
