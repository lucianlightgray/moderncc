# ===========================================================================
# Tiny C Compiler - CMake build
# ===========================================================================
#
# This single file ports the hand-written `configure` script + recursive
# `Makefile` / `lib/Makefile` / `tests/Makefile` to CMake. It builds tcc,
# libtcc, the libtcc1 runtime (compiled by the freshly-built tcc), cross
# compilers, docs, the install layout, and the pp/tests2/libtcc test suites.
# See the dynamic preprocessor flag catalog in section 3a for the config.h flags.
#
# Everything lives in this one CMakeLists.txt on purpose: the target detection,
# the config.h template, the libtcc1 runtime build and the test suite are all
# inlined as sections below.
#
# Source-of-truth mapping:
#   configure        -> options, target/OS detection, config.h / config.mak gen
#   Makefile         -> tcc exe, libtcc[.a|.so|.dll|.dylib], cross compilers, docs
#   lib/Makefile     -> libtcc1.a runtime (built BY the freshly-built tcc)
#   tests/Makefile   -> test suite
#
# This build covers every configure/Makefile feature with a CMake analogue; the
# only omissions are the intentional ones catalogued in section 16. Inline `NOTE:`
# comments flag deliberate divergences from configure/Makefile.
#
# HISTORICAL NOTE: the original autotools build (configure + the recursive
# Makefiles + conftest.c probe) was REMOVED once this file reached parity. The
# `configure ~NNN` / `Makefile ~NNN` / `lib/Makefile` line references throughout
# this file point at that deleted build and are kept only as provenance; the
# last commit that still contained those files is tagged `autotools-final`.
# See MIGRATION.md for the removal/modernization plan.
# ===========================================================================

# 3.18+ is required for cmake_language(EVAL) used by the config node graph's
# relevance-visibility pass (section 1z); 3.22 makes grouped if() conditions in
# that pass policy-clean (CMP0127 NEW). Raised from 3.16 for those features.
cmake_minimum_required(VERSION 3.22)

# --- Version ----------------------------------------------------------------
# The version string lives here directly (there is no longer a VERSION file).
# It may carry a non-numeric suffix (e.g. "0.9.28rc"); strip it so project()
# gets a clean x.y.z, but keep the full string for config.h / dylib.
set(TCC_VERSION "0.9.28rc")
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" TCC_VERSION_NUMERIC "${TCC_VERSION}")

project(tcc
    VERSION "${TCC_VERSION_NUMERIC}"
    LANGUAGES C ASM
    DESCRIPTION "Tiny C Compiler"
    HOMEPAGE_URL "https://repo.or.cz/tinycc.git")

# Emit compile_commands.json for clangd / IDE tooling across platforms.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CMP0174 (CMake 3.31+): make cmake_parse_arguments() set a single-value keyword
# to "" when followed by an empty string, instead of unsetting it. tcc_config_node
# passes DEFAULT "" for the empty-default STRING nodes; opting into NEW silences
# the dev warning and keeps the empty default well-defined. Guarded so it is a
# no-op on the 3.22-3.30 range where the policy does not exist.
if(POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()

# ===========================================================================
# 1. OPTIONS  (mirror of ./configure flags)
# ===========================================================================
# Custom-config hook (Makefile: `-include config-extra.mak` + `make EXTRA-DEFS=`).
# A user-supplied config-extra.cmake (next to this file, or -DTCC_CONFIG_EXTRA=path)
# is read FIRST so it can pre-seed any TCC_* variable below — e.g. cross search
# paths (TCC_TRIPLET/_SYSROOT/_LIBPATHS/...) or extra defines (TCC_EXTRA_CFLAGS).
set(TCC_CONFIG_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/config-extra.cmake"
    CACHE FILEPATH "Optional extra CMake config, included before options")
if(EXISTS "${TCC_CONFIG_EXTRA}")
    include("${TCC_CONFIG_EXTRA}")
    message(STATUS "tcc: included ${TCC_CONFIG_EXTRA}")
endif()

# ===========================================================================
# 1z. CONFIG NODE GRAPH  (interactive-config automation, all inline)
# ===========================================================================
# A thin metadata layer over the ordinary option()/set(CACHE) declarations
# below. It does NOT change any default or behavior -- every existing
# -DTCC_* flag keeps working byte-for-byte. What it adds, driven by a
# registry of "nodes":
#   * dropdowns      -- STRINGS combo boxes in ccmake/cmake-gui for enums.
#   * relevance      -- hide knobs that don't apply to the resolved target
#                       (ARM-only, Darwin-only, shared-libtcc-only, ...).
#                       Hiding flips the cache TYPE to INTERNAL; the stored
#                       VALUE is preserved (verified), so it is purely
#                       cosmetic -- the consuming if() branches still decide
#                       behavior and an explicit -D is never overridden.
#   * validation     -- STRINGS does NOT enforce, so enum/range/combination
#                       checks live in tcc_validate_config() (section 2z).
#   * profiles       -- TCC_TOOLCHAIN_PROFILE seeds safe feature defaults
#                       (section 1y) with an explicit override guard.
# The parent->child reveal edges are encoded as each node's VISIBLE_WHEN.
# Derived facts (TCC_CPU/TCC_TARGETOS/sizes) are reported, never edited.
#
# tcc_config_node(<NAME> TYPE <BOOL|STRING|PATH> DEFAULT <v> HELP <text>
#                 [GROUP <g>] [CHOICES <a> <b> ...] [EMPTY_OK] [ADVANCED]
#                 [VISIBLE_WHEN <cmake-if-condition string>])
function(tcc_config_node name)
    # PARSE_ARGV (not ${ARGN}) so HELP/VISIBLE_WHEN strings containing ';'
    # survive intact instead of being re-split into a list.
    cmake_parse_arguments(PARSE_ARGV 1 N "EMPTY_OK;ADVANCED"
        "TYPE;DEFAULT;HELP;GROUP;VISIBLE_WHEN" "CHOICES")

    # Effective default: a toolchain profile may have pre-seeded one.
    set(_def "${N_DEFAULT}")
    if(DEFINED TCC_SEED_${name})
        set(_def "${TCC_SEED_${name}}")
    endif()

    # Declare the cache entry. Idempotent: an existing cache value (e.g. a
    # user -D, or a previous configure) always wins -- back-compat preserved.
    if(N_TYPE STREQUAL "BOOL")
        option(${name} "${N_HELP}" "${_def}")
    else()
        set(${name} "${_def}" CACHE "${N_TYPE}" "${N_HELP}")
    endif()

    # Dropdown choices (GUI affordance only; enforced by tcc_validate_config).
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

    # Register metadata for the visibility / validation / doc passes.
    set_property(GLOBAL APPEND PROPERTY TCC_NODES "${name}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_DEFAULT "${N_DEFAULT}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_TYPE    "${N_TYPE}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_GROUP   "${N_GROUP}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_HELP    "${N_HELP}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_CHOICES "${N_CHOICES}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_EMPTYOK "${N_EMPTY_OK}")
    set_property(GLOBAL PROPERTY TCC_NODE_${name}_WHEN    "${N_VISIBLE_WHEN}")
endfunction()

# Hide nodes whose VISIBLE_WHEN is false for the resolved target by flipping
# their cache TYPE to INTERNAL (value-preserving); restore the real TYPE when
# relevant. Run AFTER target/OS/CPU detection so the conditions are settled
# (section 2z). Purely cosmetic: never mutates a node's value.
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

# A read-only derived fact (target cpu/os, sizes, ...). Not a cache entry and
# never edited; reported by tcc_report_config() and the rendered node table.
function(tcc_status_node name value)
    set_property(GLOBAL APPEND PROPERTY TCC_STATUS_NODES "${name}")
    set_property(GLOBAL PROPERTY TCC_STATUS_${name} "${value}")
endfunction()

# Record which TCC_* cache vars are already present at this point -- i.e. set by
# the user via -D or config-extra.cmake. Used by the superbuild orchestrator to
# forward the user's explicit options to each child. Computed once and persisted.
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

# Seed a profile's default for a node by setting TCC_SEED_<var>, which
# tcc_config_node() consults as the node's DEFAULT. This is a *default only*:
# it is applied via a non-FORCE option()/set(CACHE), so an explicit -D, a
# config-extra value, or a prior cache value ALWAYS wins (the "individual
# overrides" contract). Call BEFORE the node is declared.
# Trade-off: switching profiles IN PLACE does not auto-revert a previously
# seeded knob (set it explicitly with -D). This is moot for the multi-toolchain
# superbuild, where each child is a fresh build dir.
macro(tcc_profile_seed var val)
    set(TCC_SEED_${var} "${val}")
endmacro()

# Map a toolchain name to a C compiler command for a multi-toolchain sub-build.
# Empty result = 'auto' (let the child inherit the default compiler). Each
# mapping is overridable with -DTCC_CC_<name>=<path> (e.g. -DTCC_CC_tcc=/x/tcc),
# and the resolved path is cached as TCC_TOOLCHAIN_CC_<name>.
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

# Map a target-platform name to the extra CMake configure args that produce it,
# for a multi-target sub-build. Built-ins:
#   native -> (host build)            cross -> -DTCC_ENABLE_CROSS=ON (host-runnable
#                                              cross compilers for other arches)
# Any other name must be defined by the user as a list in TCC_TARGET_ARGS_<name>
# (e.g. -DTCC_TARGET_ARGS_win="-DCMAKE_TOOLCHAIN_FILE=...;-DTCC_BUILD_TESTS=OFF").
# Returns __UNKNOWN__ for an unmapped name so the caller can error.
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

# Validation pass. STRINGS only populates the GUI dropdown; it does NOT reject
# bad input -- so every enumerated node is range-checked here (FATAL), plus the
# cross-cutting invalid-combination rules surfaced by the analysis. Run AFTER
# all detection + per-OS defaults, BEFORE config.h/targets. Three actions:
# FATAL_ERROR (impossible), WARNING (inert/foot-gun, proceed), STATUS (note).
# Strict back-compat (the DEFAULT): this pass NEVER mutates a node's value -- it
# only reports. The single exception is opt-in TCC_CONFIG_AUTOCORRECT, which
# turns the two documented foot-gun / non-runnable combinations into value
# corrections via tcc_autocorrect() below (each reported); strict mode is restored
# by leaving TCC_CONFIG_AUTOCORRECT OFF.

# Force a node's cached value (used only by autocorrect), preserving its
# registered help/type and reporting the change. Cache FORCE persists globally,
# so downstream config.h emit + target wiring see the corrected value.
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
    # --- generic enum/range checks driven by the node table -----------------
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
                # WARNING, not FATAL: these values were silently accepted before
                # the node graph (the config.h emit still handles any value), so
                # a hard error would be a back-compat regression. We flag the
                # likely typo but never break a previously-valid configure.
                message(WARNING
                    "tcc config: ${_n}='${${_n}}' is not one of the known values "
                    "(${_allowed}); proceeding, but check for a typo.")
            endif()
        endif()
    endforeach()

    # --- numeric leaf (SEMLOCK is free-form numeric, no enum) ---------------
    # A non-numeric SEMLOCK is emitted as a bare token and breaks the C compile,
    # so this one stays FATAL.
    if(NOT "${TCC_CONFIG_SEMLOCK}" STREQUAL "" AND
       NOT "${TCC_CONFIG_SEMLOCK}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "tcc config: TCC_CONFIG_SEMLOCK='${TCC_CONFIG_SEMLOCK}' must be a number")
    endif()

    # --- impossible combinations (FATAL) ------------------------------------
    # tcc_s (-fsanitize) and tcc_p (-pg) are compiled by the HOST cc, which must
    # be gcc/clang. tcc_c (coverage) is NOT here: it is compiled by the freshly
    # built tcc, so the host compiler is irrelevant to it.
    if((TCC_BUILD_SANITIZE OR TCC_BUILD_PROFILE)
       AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "tcc config: TCC_BUILD_SANITIZE/PROFILE need a GCC/Clang host compiler "
            "(have '${CMAKE_C_COMPILER_ID}'); disable them or set -DCMAKE_C_COMPILER "
            "to gcc/clang.")
    endif()
    # tcc_p links -static; macOS ships no crt0.o for static executables.
    if(TCC_BUILD_PROFILE AND TCC_TARGETOS STREQUAL "Darwin")
        message(FATAL_ERROR
            "tcc config: TCC_BUILD_PROFILE (tcc_p, -static) is unsupported on Darwin "
            "(no static crt0.o).")
    endif()
    # Whole-build cross via a toolchain file (CMAKE_CROSSCOMPILING): the build
    # MUST run the freshly-built native tcc to compile libtcc1.a (an ALL target).
    # If that tcc is a foreign binary and no emulator is configured, the build
    # would die later with a confusing exec-format error -- so fail now, clearly.
    if(CMAKE_CROSSCOMPILING AND "${TCC_EMULATOR}" STREQUAL "")
        if(TCC_CONFIG_AUTOCORRECT)
            # Non-strict: avoid running the foreign tcc. Build the mandatory
            # libtcc1.a with the (cross) host CC instead (USEGCC), and drop the
            # optional steps that would JIT/run the foreign tcc or its output.
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
            # The WIN32 shared-libtcc import lib is generated by `tcc -impdef`,
            # which still has to run; autocorrect cannot avoid that one.
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

    # --- foot-guns / inert settings (WARNING; behavior unchanged) -----------
    # bcheck's runtime object is built only inside the backtrace guard, so
    # `tcc -b` would fail to link. We do NOT silently flip the value
    # (back-compat); we warn loudly.
    if(TCC_CONFIG_BCHECK AND NOT TCC_CONFIG_BACKTRACE)
        if(TCC_CONFIG_AUTOCORRECT)
            # The user asked for bcheck, so honor that intent: enable the backtrace
            # runtime it rides on (rather than silently dropping bcheck).
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
    # A CONFIG_TCC_ASM=OFF tcc has no integrated assembler, so it cannot build the
    # libtcc1 runtime's hand-written asm (file-scope __asm__ in lib/atomic.c etc.). The runtime
    # MUST therefore be compiled with the host CC (USEGCC). This is mandatory, not
    # a preference, so force it (with a clear status line) rather than failing.
    if(NOT TCC_CONFIG_ASM AND NOT TCC_LIBTCC1_USEGCC)
        tcc_autocorrect(TCC_LIBTCC1_USEGCC ON
            "asm disabled: libtcc1 .S sources must be built with the host CC")
    endif()
    # Profile names a build chain that isn't the one actually detected.
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
    # ELF-only knobs on a PE/Mach-O target are inert (incl. the libc selector).
    if((TCC_TARGETOS STREQUAL "WIN32" OR TCC_TARGETOS STREQUAL "Darwin") AND
       (TCC_CONFIG_NEW_DTAGS OR TCC_CONFIG_PIE OR TCC_CONFIG_PIC
        OR NOT TCC_CONFIG_LIBC STREQUAL ""))
        message(WARNING
            "tcc config: NEW_DTAGS/PIE/PIC/LIBC are ELF-only and inert on the "
            "${TCC_TARGETOS} target.")
    endif()
    # Darwin-only knobs explicitly set to any non-empty value on a non-Darwin target.
    if(NOT TCC_TARGETOS STREQUAL "Darwin")
        if(NOT TCC_CONFIG_CODESIGN STREQUAL "" OR NOT TCC_CONFIG_NEW_MACHO STREQUAL "")
            message(WARNING
                "tcc config: CODESIGN/NEW_MACHO only affect Mach-O output; inert on the "
                "${TCC_TARGETOS} target.")
        endif()
    endif()
endfunction()

# Emit a compact STATUS report of the resolved configuration: the derived
# target facts plus every relevant (visible) knob and its value. Gives the
# user the same picture ccmake would, on a plain `cmake -S . -B ...`.
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
            continue()   # hidden as irrelevant for this target; skip in report
        endif()
        message(STATUS "  ${_n} = ${${_n}}")
    endforeach()
    message(STATUS "===================================================")
endfunction()

# Render the node registry as a Markdown table -- a convenience VIEW of the
# user-configurable knobs; the node declarations (section 1z) are the source of
# truth and the preprocessor-flag catalog lives in section 3a. Gated on
# -DTCC_REGEN_DOCS=ON and written into the BUILD dir (not the source tree), so
# there is no tracked doc to keep in sync.
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

# configure flag                     -> CMake option
#   --enable-cross                   -> TCC_ENABLE_CROSS
#   --enable-static/--disable-static -> TCC_BUILD_STATIC (libtcc .a vs shared)
#   --disable-rpath                  -> TCC_DISABLE_RPATH
#   --debug                          -> CMAKE_BUILD_TYPE=Debug (prefer native)
#   --with-selinux                   -> TCC_WITH_SELINUX
#   --config-mingw32                 -> TCC_CONFIG_MINGW32
#   --config-bcheck=no               -> TCC_CONFIG_BCHECK
#   --config-backtrace=no            -> TCC_CONFIG_BACKTRACE
#   --config-predefs=no              -> TCC_CONFIG_PREDEFS
#   --config-pie / --config-pic      -> TCC_CONFIG_PIE / TCC_CONFIG_PIC
#   --config-new_macho               -> TCC_CONFIG_NEW_MACHO
#   --config-new_dtags               -> TCC_CONFIG_NEW_DTAGS
#   --config-codesign                -> TCC_CONFIG_CODESIGN
#   --config-uClibc / --config-musl  -> TCC_CONFIG_LIBC
#   --config-dwarf=x                 -> TCC_CONFIG_DWARF
#   --config-semlock=x               -> TCC_CONFIG_SEMLOCK

# Declared through tcc_config_node() (section 1z) so each carries its UI group,
# dropdown choices, and relevance condition (VISIBLE_WHEN) -- but the cache
# entry produced is byte-identical to the plain option()/set() it replaces, so
# every -DTCC_* keeps working unchanged. VISIBLE_WHEN only governs whether the
# knob is SHOWN in ccmake/cmake-gui for the resolved target; it never alters a
# value. Conditions referencing TCC_CPU/TCC_TARGETOS are evaluated later, by the
# visibility pass (after section 2 detection), not at declaration time.
set(_elf "NOT TCC_TARGETOS STREQUAL \"WIN32\" AND NOT TCC_TARGETOS STREQUAL \"Darwin\"")
set(_darwin "TCC_TARGETOS STREQUAL \"Darwin\"")

# Snapshot user-set TCC_* vars (-D / config-extra) BEFORE any node declaration
# so profile seeding can never overwrite them.
tcc_snapshot_user_presets()

# -- Toolchain profile / list (auto|gcc|clang|tcc|msvc; may be a LIST) --------
# Accepts an ORDERED LIST of toolchains, e.g. -DTCC_TOOLCHAIN_PROFILE="gcc;clang;tcc".
#   * single entry  -> ordinary in-tree build. The entry seeds a consistent set
#     of safe feature defaults (each overridable; an explicit -D always wins,
#     see tcc_profile_seed) but never sets CMAKE_C_COMPILER for THIS configure
#     (that is locked before project()) nor CC_NAME (stays derived, consumed by
#     tests). 'auto' = derive from the detected compiler and seed nothing.
#   * multiple entries -> this build becomes a SUPERBUILD ORCHESTRATOR: it
#     compiles the whole project once per toolchain, in order, each in its own
#     <build>/toolchain-<name> subdir with that toolchain's compiler, via
#     ExternalProject (see the orchestrator block just below). There each child
#     IS a single-entry in-tree build, so it CAN pin CMAKE_C_COMPILER at its own
#     configure time -- which is the valid way to build with N compilers.
tcc_config_node(TCC_TOOLCHAIN_PROFILE TYPE STRING DEFAULT auto GROUP "Build targets"
    HELP "Toolchain(s) to build with, in order (list, e.g. gcc;clang;tcc); single = profile+defaults"
    CHOICES auto gcc clang tcc msvc)

# Validate every entry up front (the value may be a list, so the generic enum
# check below cannot cover it).
set(_tc_allowed auto gcc clang tcc msvc)
foreach(_tc IN LISTS TCC_TOOLCHAIN_PROFILE)
    if(NOT _tc IN_LIST _tc_allowed)
        message(FATAL_ERROR
            "tcc config: TCC_TOOLCHAIN_PROFILE entry '${_tc}' is not one of: ${_tc_allowed}")
    endif()
endforeach()

# --- Multi-toolchain x multi-target superbuild orchestration ----------------
# TCC_TOOLCHAIN_PROFILE (toolchains) and TCC_TARGETS (cross-platform targets)
# are both ORDERED LISTS forming a build MATRIX. When the matrix has more than
# the single native cell (and this is not already a delegated child), the
# top-level build becomes an orchestrator: one ExternalProject child per
# (toolchain, target) cell, each a fresh single-cell build, then return().
#   * targets within a toolchain build SIMULTANEOUSLY (no DEPENDS between them).
#   * toolchain stages run IN ORDER by default (each stage's cells wait for the
#     whole previous stage) -- toggle with TCC_SUPERBUILD_SEQUENTIAL=OFF to make
#     the entire matrix build in parallel.
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
    # Forward the user's explicitly-set TCC_* options (snapshot) to every cell,
    # minus the orchestration controls themselves.
    set(_forward "")
    foreach(_v IN LISTS _TCC_PRESET_VARS)
        if(_v MATCHES "^TCC_(TOOLCHAIN_PROFILE|TARGETS|SUPERBUILD_TEST|SUPERBUILD_SEQUENTIAL)$")
            continue()
        endif()
        list(APPEND _forward "-D${_v}=${${_v}}")
    endforeach()
    set(_prev_stage "")          # all cells of the previous toolchain stage
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
            # Order: cells in a stage are parallel; a stage waits for the prior
            # stage (all its cells) when sequential.
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
    # Stop here: the orchestrator delegates all compilation to the children.
    return()
endif()

# -- Toolchain-profile seed table (node DEFAULTS only; -D always wins) --------
# Runs BEFORE the feature nodes so each seed becomes that node's default via a
# non-FORCE declaration. Only genuinely-safe, profile-differentiated defaults
# the build does not already set; this is the extensible seam. In a child build
# TCC_TOOLCHAIN_PROFILE is a single value.
#
# POLICY: every seed here must be (a) a REAL change from the node's own default
# for THAT profile -- never a value "matching current" -- and (b) safe, i.e. an
# explicit -D, config-extra, or prior cache value still wins. Defaults that the
# build already sets unconditionally (e.g. TCC_LIBTCC1_USEGCC=OFF, parallel
# multi-TU for gcc/clang) are deliberately NOT duplicated here: re-seeding them
# would be cosmetic noise and could mask a future change to the real default.
if(TCC_TOOLCHAIN_PROFILE STREQUAL "tcc")
    tcc_profile_seed(TCC_ONE_SOURCE ON)   # tcc self-hosts fastest as one TU
elseif(TCC_TOOLCHAIN_PROFILE STREQUAL "msvc")
    # The only established MSVC build of libtcc is the single `libtcc.c`
    # amalgamation (win32/build-tcc.bat: `cl ... ..\libtcc.c`, then tcc.exe with
    # -DONE_SOURCE=0); there is no per-TU MSVC build here. Real addition: the
    # node default is OFF (parallel multi-TU, which suits gcc/clang, not cl).
    tcc_profile_seed(TCC_ONE_SOURCE ON)
    # The tcctest reference is compiled with the host CC using GNU-only flags
    # (-std=gnu99 -fno-omit-frame-pointer -w -O0; see the run_tcctest driver),
    # which `cl` rejects -- so the suite can't run with MSVC as the host cc.
    # Real addition: the node default is ON. (-DTCC_BUILD_TESTS=ON to force it.)
    tcc_profile_seed(TCC_BUILD_TESTS OFF)
endif()

# -- Build targets / shape ---------------------------------------------------
tcc_config_node(TCC_ENABLE_CROSS  TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build all cross compilers (Makefile: make cross)")
tcc_config_node(TCC_BUILD_STATIC  TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build libtcc.a (ON) instead of shared libtcc (OFF)")
tcc_config_node(TCC_DISABLE_RPATH TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Do not bake -rpath into binaries linking libtcc.so"
    VISIBLE_WHEN "NOT TCC_BUILD_STATIC")          # rpath only with a shared libtcc
tcc_config_node(TCC_ONE_SOURCE    TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build libtcc from a single TU (Makefile: ONE_SOURCE)")
tcc_config_node(TCC_BUILD_TESTS   TYPE BOOL DEFAULT ON  GROUP "Build targets"
    HELP "Build/enable the test suite (tests/Makefile)")
# Umbrella "everything on" diagnostics switch. Turns the whole tree into a
# diagnostic build: verbose compiler warnings + full debug info on the core
# compiler TUs (wired in sections 7/8 via TCC_DIAG_FLAGS), AND builds every
# diagnostic variant by seeding the three knobs below ON -- tcc_s (ASan+UBSan:
# memory debugging + variable-lifetime/UB), tcc_p (-pg profiling), tcc_c
# (coverage). Seeds are DEFAULT-only, so an explicit -DTCC_BUILD_*=OFF always
# wins (e.g. -DTCC_BUILD_PROFILE=OFF on Darwin, where tcc_p's -static is
# unsupported, or on a non-GCC/Clang host where sanitize/profile are invalid).
tcc_config_node(TCC_DIAGNOSTICS TYPE BOOL DEFAULT OFF GROUP "Diagnostics" ADVANCED
    HELP "Everything-on diagnostics: verbose warnings + debug info + build tcc_s/tcc_p/tcc_c")
if(TCC_DIAGNOSTICS)
    tcc_profile_seed(TCC_BUILD_SANITIZE ON)   # memory + lifetime/UB (tcc_s)
    tcc_profile_seed(TCC_BUILD_COVERAGE ON)   # coverage (tcc_c)
    tcc_profile_seed(TCC_BUILD_PROFILE  ON)   # profiling (tcc_p)
endif()

# Optional diagnostic builds (consumed in the diagnostic-builds section). tcc_p
# (-pg) and tcc_s (-fsanitize) need a GCC/Clang host (validated); tcc_c coverage
# is compiled by the built tcc itself.
tcc_config_node(TCC_BUILD_PROFILE  TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build tcc_p (profiling: -pg -static)")
tcc_config_node(TCC_BUILD_COVERAGE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build tcc_c (coverage instrumentation)")
tcc_config_node(TCC_BUILD_SANITIZE TYPE BOOL DEFAULT OFF GROUP "Diagnostics"
    HELP "Also build tcc_s (-fsanitize=address,undefined)")
# Build the NATIVE libtcc1 runtime with the host compiler (+ -fPIC -gdwarf/-gstabs)
# instead of the freshly-built tcc -- the lib/Makefile <target>-libtcc1-usegcc=yes
# path, which yields ~40-60% faster bounds-checked objects. Native only.
tcc_config_node(TCC_LIBTCC1_USEGCC TYPE BOOL DEFAULT OFF GROUP "Build targets"
    HELP "Build the native libtcc1 with the host CC instead of tcc (faster bcheck)")
# Opt-in non-strict mode. By default the validation pass is STRICT: it only
# *reports* foot-gun / non-runnable combinations and never mutates a node (the
# back-compat contract). With this ON it instead AUTO-CORRECTS those few cases to
# a coherent configuration and prints what it changed. An explicit -D on the
# corrected knob still wins where the correction is a default-flip. See
# tcc_validate_config() for the exact rules.
tcc_config_node(TCC_CONFIG_AUTOCORRECT TYPE BOOL DEFAULT OFF GROUP "Build targets" ADVANCED
    HELP "Non-strict mode: auto-correct inert/non-runnable config combinations instead of only warning")

# -- tcc compiler features ---------------------------------------------------
tcc_config_node(TCC_CONFIG_MINGW32  TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Build a WIN32/mingw32 target (input: forces TCC_TARGETOS=WIN32)")
tcc_config_node(TCC_CONFIG_BACKTRACE TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Enable stack backtraces (-bt / -run)")
tcc_config_node(TCC_CONFIG_BCHECK   TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Enable bounds checker (-b)"
    VISIBLE_WHEN "TCC_CONFIG_BACKTRACE")          # bcheck runtime rides on backtrace
tcc_config_node(TCC_CONFIG_ASM      TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Enable the integrated assembler (inline asm, global asm, .s files, asm labels)")
tcc_config_node(TCC_CONFIG_PREDEFS  TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Compile tccdefs.h into the binary (c2str)")
tcc_config_node(TCC_CONFIG_PIE      TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Let tcc generate position independent executables"
    VISIBLE_WHEN "${_elf}")                       # PIE/PIC are ELF-only
tcc_config_node(TCC_CONFIG_PIC      TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Position independent code"
    VISIBLE_WHEN "${_elf}")
tcc_config_node(TCC_WITH_SELINUX    TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Use mmap for executable memory (tcc -run)")
tcc_config_node(TCC_CONFIG_NEW_DTAGS TYPE BOOL DEFAULT OFF GROUP "tcc features"
    HELP "Use DT_RUNPATH instead of DT_RPATH (tcc-emitted; always relevant on ELF)"
    VISIBLE_WHEN "${_elf}")

# Build-path-first runtime discovery. When ON, the freshly-built tcc (and the
# cross compilers / anything linking libtcc) auto-discovers its runtime
# (libtcc1.a + include/) from the build directory it lives in, so it runs with
# NO -B and the build dir is a complete, self-contained tccdir. It only falls
# back to the compiled-in install path (CONFIG_TCCDIR, the "system" location)
# when the binary is NOT sitting next to its runtime -- i.e. once installed
# under <prefix>/bin. This is what fixes bare in-tree runs that otherwise hit
# `error: file 'libtcc1.a' not found` / `include file 'stdarg.h' not found`.
# Implemented in libtcc.c (tcc_auto_tccdir); -B always overrides. See section 9.
tcc_config_node(TCC_AUTO_TCCDIR     TYPE BOOL DEFAULT ON GROUP "tcc features"
    HELP "Build-tree tcc auto-discovers libtcc1.a + headers locally, falling back to system CONFIG_TCCDIR")

# -- Enumerated feature values (dropdowns; validated in tcc_validate_config) --
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


# --- Path / string options (configure: install dirs & tcc runtime paths) ----
# Registered as ADVANCED nodes (group "Runtime paths") so they join the report/
# doc, but stay out of the basic ccmake view. Each is an empty-default STRING,
# byte-identical to the plain set(CACHE) it replaces; the per-OS defaults block
# below still overrides the (non-cache) value when the user leaves them empty.
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
# --extra-cflags / --extra-ldflags / --extra-libs (configure appends these to
# CFLAGS / LDFLAGS / extralibs). Applied globally so every host-compiled TU and
# every link picks them up, exactly like the Makefile's global CFLAGS/LDFLAGS.
# extralibs is appended to the libtcc link in section 7.
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
# --cross-prefix / --cc / --ar are CMake-native: pass -DCMAKE_C_COMPILER=<cc> and
# -DCMAKE_AR=<ar>, and drive cross builds with a CMake toolchain file (which sets
# CMAKE_CROSSCOMPILING + the prefixed tools) rather than replicating cross_prefix.

# ===========================================================================
# 2. TARGET (CPU/OS) DETECTION   (inlined here)
# ===========================================================================
#    configure: `uname` + big case statements normalizing cpu & targetos,
#    plus conftest.c runtime probe for compiler/endianness/triplet. Under CMake
#    we lean on CMAKE_SYSTEM_* / the compiler ID, which already encode most of
#    what conftest.c probes.

# --- CPU normalization (configure: the `case "$cpu"` block) ----------------
# Maps CMAKE_SYSTEM_PROCESSOR onto tcc's canonical names: i386, x86_64, arm,
# arm64, riscv64 (ppc/mips/s390/alpha are listed in configure but have no
# backend here).
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
    # cpuver (4/5/6/7) is derived from the compiler's __ARM_ARCH in the ARM
    # detection block below (configure's armv4l/.../armv7 case + Android cpuver=7).
elseif(_proc MATCHES "^riscv64$")
    set(TCC_CPU "riscv64")
else()
    # configure: `echo "Unsupported CPU"; exit 1`
    message(FATAL_ERROR "Unsupported CPU '${_proc}' (configure: Unsupported CPU)")
endif()
# Cross builds select the target via TCC_ENABLE_CROSS (section 10), not -DTCC_CPU;
# TCC_CPU here is always the native/host CPU (configure --cpu equivalent).

# --- OS normalization (configure: the `case $buildos`/`$targetos`) ---------
# --config-mingw32 forces a WIN32/PE target even on a non-Windows host
# (configure: `if mingw32=yes; then default targetos WIN32`). Treat it like a
# Windows target OS regardless of the build machine.
if(WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Windows|MINGW|MSYS|CYGWIN" OR TCC_CONFIG_MINGW32)
    set(TCC_TARGETOS "WIN32")
    set(TCC_LIBSUF ".lib")
    set(TCC_EXESUF ".exe")
    set(TCC_DLLSUF ".dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TCC_TARGETOS "Darwin")          # configure: targetos=Darwin, confvars OSX
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".dylib")
    # OSX dwarf=4 / codesign / new_macho defaults: the Darwin defaults block below.
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    set(TCC_TARGETOS "${CMAKE_SYSTEM_NAME}")   # configure: confvars BSD, ldl=no
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
    # TARGETOS_<bsd> define + per-BSD elfinterp: the BSD defaults block below.
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(TCC_TARGETOS "Android")
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
    # Android/Termux sysroot/triplet/libpaths/pie/dwarf: the Android block below.
else()
    set(TCC_TARGETOS "${CMAKE_SYSTEM_NAME}")   # Linux & friends
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
endif()

# --- Per-target-OS defaults (configure: the `case $targetos` confvars/defaults) -
# configure layers OS-specific defaults on top of the options above. Each value
# is only filled when the user did NOT override it (the `default`/`default_conf`
# helpers), so an explicit -DTCC_* on the command line always wins.
if(TCC_TARGETOS STREQUAL "Darwin")
    # configure Darwin case: `confvars_set OSX dwarf=4; default_conf codesign`,
    # plus new_macho autodetection from the running macOS major version.
    if("${TCC_CONFIG_DWARF}" STREQUAL "")
        set(TCC_CONFIG_DWARF 4)
    endif()
    if("${TCC_CONFIG_CODESIGN}" STREQUAL "")
        set(TCC_CONFIG_CODESIGN yes)
    endif()
    # new_macho: if unspecified and the host macOS major version is < 11, force
    # the old object format (configure: `sw_vers -productVersion`; default 11).
    if("${TCC_CONFIG_NEW_MACHO}" STREQUAL "" AND NOT CMAKE_CROSSCOMPILING)
        execute_process(COMMAND sw_vers -productVersion
            OUTPUT_VARIABLE _osxver OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_osxver MATCHES "^([0-9]+)" AND CMAKE_MATCH_1 LESS 11)
            set(TCC_CONFIG_NEW_MACHO no)
        endif()
    endif()
elseif(TCC_TARGETOS MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    # configure CONFIG_BSD case: per-BSD dynamic-linker path (TARGETOS_<bsd> is
    # emitted in section 3). Only fills CONFIG_TCC_ELFINTERP when unset.
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
    # configure Android/Termux case. Termux (env TERMUX_VERSION) relocates the
    # sysroot; otherwise it is /usr. Sets the search paths, pie (non-i386),
    # new_dtags, dwarf=4 and a shared libtcc, only where not user-overridden.
    if(DEFINED ENV{TERMUX_VERSION})
        set(_android_sysroot "/data/data/com.termux/files/usr")
    else()
        set(_android_sysroot "/usr")
    endif()
    if("${TCC_SYSROOT}" STREQUAL "")
        set(TCC_SYSROOT "${_android_sysroot}")
    endif()
    set(TCC_CONFIG_NEW_DTAGS ON)          # confvars: new_dtags
    set(TCC_DISABLE_RPATH ON)             # confvars: rpath=no
    if("${TCC_CONFIG_DWARF}" STREQUAL "")
        set(TCC_CONFIG_DWARF 4)
    endif()
    if(NOT TCC_CPU STREQUAL "i386")
        set(TCC_CONFIG_PIE ON)            # confvars: pie (skipped on i386)
    endif()
    set(TCC_BUILD_STATIC OFF)             # default_conf static=no -> shared libtcc
    # per-cpu triplet (configure: the Android `case "$cpu"`)
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
    # 64-bit cpus get the `64` linker/lib suffix (configure: S="64").
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

# os_release defaults to the host kernel release (configure:
# `default os_release "$(uname -r)"`). Emitted as CONFIG_OS_RELEASE; consumed
# for the Mach-O version-min, otherwise inert. Skipped when cross-compiling
# (the host release would not describe the target).
if("${TCC_OS_RELEASE}" STREQUAL "" AND NOT CMAKE_CROSSCOMPILING)
    set(TCC_OS_RELEASE "${CMAKE_HOST_SYSTEM_VERSION}")
endif()

# --- Endianness (configure: conftest.c `bigendian` probe) ------------------
if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    set(TCC_BIGENDIAN ON)     # configure: confvars_set BIGENDIAN
else()
    set(TCC_BIGENDIAN OFF)
endif()

# Triplet + lib/crt detection is done in the dedicated block below (`cc
# -dumpmachine` + /usr/lib<triplet>/lib64 crti.o probes). Compiler name+version
# (CC_NAME / GCC_MAJOR / GCC_MINOR) come from CMAKE_C_COMPILER_ID/_VERSION in the
# compiler-identity block. Cross builds are driven by TCC_ENABLE_CROSS (section
# 10) rather than a host cross-build probe.

message(STATUS "tcc target: CPU=${TCC_CPU} OS=${TCC_TARGETOS} bigendian=${TCC_BIGENDIAN}")

# ===========================================================================
# 3. config.h GENERATION   (inlined here)
# ===========================================================================
#    configure writes config.h from print_str/print_num of every CONFIG_*. We
#    build the file contents as a string and file(WRITE) it directly. Every value
#    (CC_NAME, GCC_MAJOR/MINOR, triplet, sysinclude/lib/crt, elfinterp, sysroot,
#    tccdir, dwarf, bcheck/backtrace/pie, ...) is wired from the options/detection
#    above, inside the `#if !(TCC_TARGET_*)` native guard so cross builds (which
#    pass -DTCC_TARGET_* on the command line) skip it. The dynamic-flag catalog
#    below is the reference for this generator.
#
# ===========================================================================
# 3a. DYNAMIC PREPROCESSOR FLAG CATALOG (config.h reference)
# ===========================================================================
# Every *dynamic* C preprocessor configuration macro in the TinyCC sources — the
# ones a build system (configure -> config.h, the Makefile DEF-<target> tables,
# or this file) sets to shape the build. Sections 1-8 below cover the
# build-settable macros; section 9 additionally catalogs the internal/derived
# and host-autodetected macros the code merely *reads* (folded in from the
# former docs/feature-matrix.md, now removed — this catalog supersedes it).
#
# Scale: the compiler sources (*.c/*.h, excluding the elf/dwarf/stab system
# tables) hold ~1,100 #if/#ifdef/#ifndef/#elif directives, ~360 distinct
# conditions, driven by ~70 identifiers. Regenerate the raw frequency ranking:
#   files=$(ls *.c *.h | grep -vE '^(elf|dwarf|stab)\.h$')
#   grep -hE '^\s*#\s*(if|ifdef|ifndef|elif)\b' $files \
#     | grep -oE '[A-Za-z_][A-Za-z0-9_]*' \
#     | grep -vwE 'defined|sizeof' | sort | uniq -c | sort -rn
#
# Sources of truth:
#   * tcc.h ~159-328 and ~1900-1976 — the `#ifndef X #define X <default>`
#     fallbacks. The default of any flag is whatever tcc.h falls back to when the
#     macro is absent.
#   * configure ~617-728 — how each macro is emitted into config.h (print_str =
#     quoted string, print_num = bare number).
#   * Makefile ~99-123 — the cross-compiler DEF-<target> flag tables.
#
# "Default" = the effective value when nothing defines the macro.
# "Scope": native-guard = emitted only for the native build (inside
#   `#if !(TCC_TARGET_*)`); always = emitted unconditionally.
#
# --- 1. Target selectors (set by the build, not by the user directly) --------
# Exactly one CPU macro is active. A native build defines it inside the
#   `#if !(TCC_TARGET_I386 || TCC_TARGET_X86_64 || TCC_TARGET_ARM ||
#     TCC_TARGET_ARM64 || TCC_TARGET_RISCV64)`
# guard; cross builds pass -DTCC_TARGET_* on the command line (so the guard
# suppresses the native block). If *no* CPU macro is defined, tcc.h:159
# auto-selects from the host (__x86_64__->X86_64, __arm__->ARM+EABI+VFP+HARDFLOAT,
# __aarch64__->ARM64, __riscv->RISCV64, else I386; plus PE on _WIN32, MACHO on
# __APPLE__).
#   TCC_TARGET_I386     32-bit x86 backend
#   TCC_TARGET_X86_64   64-bit x86 backend (also defines
#                       TCC_TARGET_NATIVE_STRUCT_COPY, x86_64-gen.c:111)
#   TCC_TARGET_ARM      32-bit ARM backend
#   TCC_TARGET_ARM64    AArch64 backend
#   TCC_TARGET_RISCV64  RISC-V 64 backend
#   TCC_TARGET_PE       Windows PE object/exe format (selects tccpe.c)
#   TCC_TARGET_MACHO    Apple Mach-O format (selects tccmacho.c)
#   TARGETOS_FreeBSD / _FreeBSD_kernel / _OpenBSD / _NetBSD
#                       BSD target OS; any of these defines TARGETOS_BSD
#                       (tcc.h:219)
#   TARGETOS_ANDROID    Android target
#
# Derived target macros (computed in tcc.h; do NOT set these yourself):
#   TCC_IS_NATIVE                  host OS+arch == target (tcc.h:185). Gates -run.
#   TCC_TARGET_UNIX                1 when not PE and not MachO (tcc.h:231); else
#                                  ELF_OBJ_ONLY is defined
#   TARGETOS_BSD                   any TARGETOS_*BSD* (tcc.h:219)
#   TARGETOS_Linux                 not PE, not MachO, not BSD (tcc.h:225) — for
#                                  tccdefs_.h
#   TCC_TARGET_NATIVE_STRUCT_COPY  defined in x86_64-gen.c:111
#   TCC_EH_FRAME                   1 unless ELF_OBJ_ONLY / ARM / BSD (tcc.h:1847)
#   TCC_USING_DOUBLE_FOR_LDOUBLE   PE, or MachO+ARM64 (tcc.h:236); also ARM when
#                                  LDOUBLE_SIZE==8
#   CONFIG_TCC_ASM                 defined by each arch *-gen.c/*-asm.c (all real
#                                  targets) — not a config.h flag
#
# --- 2. ARM float-ABI sub-features (Makefile DEF-arm*, configure triplet probe) -
# The Makefile DEF-arm family layers these; configure derives them from the
# triplet (...eabihf / ...eabi) or /proc/cpuinfo.
#   TCC_ARM_EABI         (default off) ARM EABI calling convention; sets
#                        LDOUBLE_ALIGN 8. Requires VFP (`#error` otherwise,
#                        arm-gen.c:26).
#   TCC_ARM_VFP          (default off) use VFP FP instructions; NB_REGS 9->13
#                        (arm-gen.c:31).
#   TCC_ARM_HARDFLOAT    (default off) default float_abi = ARM_HARD_FLOAT
#                        (libtcc.c:899). Set explicitly (not implied by VFP in
#                        code).
#   __ARM_FEATURE_IDIV   (default off) HW integer divide. Consumed only by the
#                        runtime lib/armeabi.c, NOT the compiler core. From
#                        --config-arm_idiv.
#   CONFIG_TCC_CPUVER    (default 5, arm-gen.c:37) ARM arch version; gates BLX
#                        (arm-link.c:203). configure sets 7 for Android/eabihf.
#   LDOUBLE_SIZE         (default 8 on ARM, arm-gen.c:117) long double byte size;
#                        12 via DEF-arm-fpa-ld. 8 => TCC_USING_DOUBLE_FOR_LDOUBLE.
# DEF-<target> reference (from Makefile):
#   DEF-arm-fpa     = -DTCC_TARGET_ARM
#   DEF-arm-fpa-ld  = -DTCC_TARGET_ARM -DLDOUBLE_SIZE=12
#   DEF-arm-vfp     = -DTCC_TARGET_ARM -DTCC_ARM_VFP
#   DEF-arm-eabi    = -DTCC_TARGET_ARM -DTCC_ARM_VFP -DTCC_ARM_EABI
#   DEF-arm-eabihf  = -DTCC_TARGET_ARM -DTCC_ARM_VFP -DTCC_ARM_EABI -DTCC_ARM_HARDFLOAT
#   DEF-arm         = DEF-arm-eabihf
#
# --- 3a. Tri-state toggles — default ON, disable by defining to 0 ------------
# tcc.h:201-217 uses `#if defined X && X==0 -> #undef X; else #define X 1`.
# Leaving these undefined means ON. To turn one off, emit `#define X 0` (which
# tcc.h then #undefs). configure only ever emits the 0 form (the --config-*=no
# case); the enabled state emits nothing.
#   CONFIG_TCC_BCHECK      (default 1) bounds/memory checker (-b); disabled by
#                          `#define ...=0` / --config-bcheck=no
#   CONFIG_TCC_BACKTRACE   (default 1) built-in stack backtraces (-bt, lib/bt-*);
#                          disabled by `#define ...=0` / --config-backtrace=no
#   CONFIG_NEW_MACHO       (default 1) new Mach-O chained-fixups codepath
#                          (tccmacho.c); disabled by `#define ...=0` /
#                          --config-new_macho=no
#
# --- 3b. Plain `#ifdef` toggles — default OFF, enable by defining (to 1) ------
#   CONFIG_SELINUX               (native-guard) -run uses a temp dual-mapping
#                                instead of RWX memory (tccrun.c:115).
#                                Set by --with-selinux.
#   CONFIG_TCC_PIE               (native-guard) TCC_OUTPUT_EXE => dynamic; PIE ELF
#                                header. Auto-defines CONFIG_TCC_PIC 1 (tcc.h:241).
#                                Set by --config-pie.
#   CONFIG_TCC_PIC               (native-guard) emit PIC/GOT-relative code (mainly
#                                i386). Set by --config-pic (or implied by PIE).
#   CONFIG_NEW_DTAGS             (native-guard) linker emits DT_RUNPATH not
#                                DT_RPATH (libtcc.c:902). Set by --config-new_dtags.
#   CONFIG_CODESIGN              (native-guard) ad-hoc Mach-O code signature
#                                (tccmacho.c:2237). Set by --config-codesign
#                                (default on macOS).
#   CONFIG_TCC_MUSL              (native-guard) use linux-musl OS-release/triplet
#                                (libtcc.c:1814). Set by --config-musl.
#   CONFIG_TCC_UCLIBC            (native-guard) legacy libc selector — NOT
#                                referenced in current sources. Set by
#                                --config-uClibc.
#   CONFIG_TCC_STATIC            (command line) static tcc: drop <dlfcn.h>, dummy
#                                dlopen (tcc.h:45, tccrun.c:1536). Set by tcc_p
#                                profiling build.
#   TCC_PROFILE                  (command line) `#define static`/`inline` empty so
#                                all fns are profiler-visible (tcc.h:355). tcc_p
#                                build.
#   LIBTCC_AS_DLL                (command line) export libtcc API as a Windows DLL
#                                (tcc.h:74). libtcc.dll build.
#   CONFIG_TCC_BACKTRACE_ONLY    (command line) strip run/relocate, keep only
#                                backtrace; set when lib/bt-exe.c includes
#                                tccrun.c. Runtime lib build.
#   CONFIG_TCCBOOT               (command line) bootable-kernel build; changes the
#                                -run exit path (tccrun.c:1561). Special builds.
#   ONE_SOURCE                   (command line) amalgamated single-TU build:
#                                ST_FUNC/ST_DATA become static (tcc.h:341).
#                                Default 0. Set by Makefile ONE_SOURCE.
#   TCC_LIBGCC                   (path/toggle) also link libgcc (tccelf.c:1857) —
#                                absolute path => add file, else add dll. Rarely.
#
# --- 4. Numeric / version macros ---------------------------------------------
#   CONFIG_TCC_PREDEFS    (default 1 from configure; no header fallback —
#                         `#if`-tested, so absent => 0) 1: compile predefined
#                         macros from tccdefs_.h into the binary; 0: load
#                         <tccdefs.h> at runtime (tccpp.c:3625).
#                         --config-predefs=no => 0.
#   CONFIG_DWARF_VERSION  (default 0, tcc.h:1920) default DWARF version for plain
#                         -g; 0 => emit stabs instead. Set by --config-dwarf=N
#                         (macOS/Android default 4).
#   CONFIG_TCC_SEMLOCK    (default 1, tcc.h:246) thread-safe libtcc via
#                         semaphore/critical-section; 0 removes all locking.
#                         Set by --config-semlock=N.
#   CONFIG_RUNMEM_RO      (default 1 on __APPLE__, else 0, tccrun.c:323) -run makes
#                         code memory read-only after relocation (W^X). Per-target.
#   CONFIG_OS_RELEASE     (absent => block skipped) "maj.min.patch" string ->
#                         Mach-O version-min (tccelf.c:2949). Set by --os-release.
# Note: DEFAULT_DWARF_VERSION (for an *explicit* -gdwarf) is separate: 2 on MachO
# else 5 (tcc.h:1907). Don't confuse it with CONFIG_DWARF_VERSION.
#
# --- 5. Path / string macros (triplet-aware) ---------------------------------
# tcc.h:260 defines USE_TRIPLET(s) / ALSO_TRIPLET(s): if CONFIG_TRIPLET is defined
# they append "/" CONFIG_TRIPLET; otherwise they are identity. Overriding a path
# macro directly bypasses the triplet expansion. In path values {B} is substituted
# at runtime by CONFIG_TCCDIR (or -B), {R} by CONFIG_SYSROOT. PATHSEP is ";" on
# Windows, ":" elsewhere (tcc.h:130).
#   CONFIG_TCCDIR               (default "/usr/local/lib/tcc" non-Win, tcc.h:256;
#                               runtime-computed on Win/LIBTCC_AS_DLL; scope always
#                               via `#ifndef`) base install dir = {B}; suppressed
#                               when tccdir==bindir (Win)
#   CONFIG_TCC_AUTO_TCCDIR      (default off; -DTCC_AUTO_TCCDIR=ON defines =1
#                               globally) build-path-first runtime discovery: the
#                               in-build tcc defaults its tccdir to its own exe
#                               dir when that holds the runtime, else falls back
#                               to CONFIG_TCCDIR (libtcc.c tcc_auto_tccdir). -B
#                               always overrides. POSIX analogue of the win32
#                               config_tccdir_w32 path.
#   CONFIG_SYSROOT              (default "", tcc.h:252; scope always via `#ifndef`)
#                               prefix {R} for system include/lib/crt
#   CONFIG_TRIPLET              (absent => identity, tcc.h:263; native-guard) GNU
#                               multiarch triplet (e.g. x86_64-linux-gnu)
#   CONFIG_TCC_CRTPREFIX        (default USE_TRIPLET(CONFIG_SYSROOT "/usr/lib"),
#                               tcc.h:269; native-guard) where crt1/crti/crtn.o live
#   CONFIG_TCC_SYSINCLUDEPATHS  (default PE: "{B}/include":"{B}/include/winapi";
#                               else "{B}/include":ALSO_TRIPLET(CONFIG_SYSROOT
#                               "/usr/include"), tcc.h:277; native-guard) system
#                               include search list
#   CONFIG_TCC_LIBPATHS         (default PE: "{B}/lib"; else "{B}":ALSO_TRIPLET(
#                               CONFIG_SYSROOT "/usr/lib"), tcc.h:288; native-guard)
#                               library search list
#   CONFIG_TCC_ELFINTERP        (target-derived, tcc.h:299: arm64
#                               /lib/ld-linux-aarch64.so.1, x86_64
#                               /lib64/ld-linux-x86-64.so.2, riscv64
#                               /lib/ld-linux-riscv64-lp64d.so.1, arm
#                               /lib/ld-linux.so.3, PE "-", else /lib/ld-linux.so.2;
#                               native-guard) PT_INTERP dynamic loader path
#   CONFIG_TCC_ELFINTERP_ARMHF  (ARM only: /lib/ld-linux-armhf.so.3, tcc.h:312;
#                               native-guard) hard-float ARM loader
#   CONFIG_TCC_SWITCHES         (absent; native-guard) options applied to every
#                               TCCState (libtcc.c:910); Android sets
#                               -Wl,-rpath=$sysroot/lib
#   TCC_LIBTCC1                 (default "libtcc1.a", tcc.h:319; scope always via
#                               `#ifndef`) runtime support archive name
#   CONFIG_TCC_CROSSPREFIX      (default "", tcc.h:324; scope always via `#ifndef`)
#                               prefix for libtcc1.a / cross tools
#
# --- 6. Compiler-identity macros (emitted by configure, consumed by tests) ---
# config.h always carries these (before the native guard). The compiler core does
# not read them; tests/tcctest.c does (e.g. CC_NAME == CC_clang).
#   CC_NAME      CC_gcc / CC_clang / CC_tcc / CC_msvc (conftest.c `c` probe)
#   GCC_MAJOR    host compiler major (0 if unknown) (conftest.c `v` probe)
#   GCC_MINOR    host compiler minor (0 if unknown) (conftest.c `m` probe)
#   TCC_VERSION  set inline at the top of this file (section 1)
#   TCC_GITHASH  "<date> <branch>@<hash>[*]" (Makefile git, applied ONLY to tcc.c)
#
# --- 7. Absent / legacy macros (in configure/Makefile history, not in *.c) ---
#   CONFIG_BIGENDIAN     written to config.mak but NEVER emitted to config.h;
#                        endianness is handled by explicit read*le/write*le helpers.
#   CONFIG_USE_LIBGCC    not present; only TCC_LIBGCC is used.
#   CONFIG_LDOUBLE       not a macro; use LDOUBLE_SIZE /
#                        TCC_USING_DOUBLE_FOR_LDOUBLE.
#
# --- 8. config.h emission order (the generator below mirrors this) -----------
#   #define TCC_VERSION "x.y.z"
#   #define CC_NAME CC_<id>
#   #define GCC_MAJOR <n>
#   #define GCC_MINOR <n>
#
#   #if !(TCC_TARGET_I386 || TCC_TARGET_X86_64 || TCC_TARGET_ARM \
#      || TCC_TARGET_ARM64 || TCC_TARGET_RISCV64)
#      /* native-guard block */
#      #define TCC_TARGET_<cpu> 1
#      /* + TCC_TARGET_PE / TCC_TARGET_MACHO / TARGETOS_* as applicable      */
#      /* + CONFIG_TCC_CPUVER, TCC_ARM_*, __ARM_FEATURE_IDIV for arm         */
#      /* + CONFIG_TCC_UCLIBC / CONFIG_TCC_MUSL                              */
#      /* + CONFIG_SELINUX, CONFIG_TCC_PIE/PIC, CONFIG_NEW_DTAGS,            */
#      /*   CONFIG_CODESIGN, CONFIG_NEW_MACHO=0, CONFIG_TCC_BCHECK=0,        */
#      /*   CONFIG_TCC_BACKTRACE=0, CONFIG_DWARF_VERSION, CONFIG_TCC_SEMLOCK */
#      /* + CONFIG_TCC_SYSINCLUDEPATHS, _LIBPATHS, _CRTPREFIX, _ELFINTERP,   */
#      /*   _SWITCHES, CONFIG_TRIPLET, CONFIG_OS_RELEASE (only if non-empty) */
#   #endif
#
#   #ifndef CONFIG_SYSROOT
#   # define CONFIG_SYSROOT "..."
#   #endif
#   #ifndef CONFIG_TCCDIR          /* omitted when tccdir==bindir (Win auto) */
#   # define CONFIG_TCCDIR "..."
#   #endif
#   #define CONFIG_TCC_PREDEFS <0|1>
# The two tri-state toggles and CONFIG_NEW_MACHO are emitted ONLY in their 0
# (disabled) form; everything else is emitted only when it differs from the tcc.h
# default (i.e. when an option was given), so config.h stays minimal.
#
# --- 9. Read-only macros the build does NOT set (reference) ------------------
# Folded in from the former docs/feature-matrix.md. These are NOT config.h flags;
# they are internal plumbing, target-derived facts, host-platform probes, or
# developer toggles. Listed so the catalog is self-contained.
#
# 9a. Internal build-plumbing (set by source #includes / per-arch, not the user):
#   TARGET_DEFS_ONLY   a *-gen.c is #included only for its macro defs, not its
#                      code (the amalgamation pulls each backend twice).
#   ELF_OBJ_ONLY       emit ELF .o only, skip exe/dynlink emission (~15 blocks);
#                      defined when not PE and not MachO without a native exe.
#   NEED_BUILD_GOT     i386/arm: hand-build the GOT/PLT (tccelf.c).
#   TCC_TARGET_NATIVE_STRUCT_COPY  x86_64: memcpy-style struct copy (x86_64-gen.c).
#   TCC_CROSS_TEST     test harness: force cross paths even on a native build.
#
# 9b. Target-derived sizes (computed from TCC_TARGET_*, drive ~60 codegen blocks;
#     never set directly):
#   PTR_SIZE     4 on i386/arm, else 8.
#   LONG_SIZE    8 on 64-bit non-PE; 4 on PE (LLP64) and 32-bit targets.
#   LDOUBLE_SIZE 12 i386, 16 x86_64/arm64, 8 arm (=> TCC_USING_DOUBLE_FOR_LDOUBLE).
#
# 9c. Host compiler/platform probes (read from the toolchain; gate host-specific
#     code; you never set these):
#   _WIN32 _WIN64 _MSC_VER __APPLE__ __linux__ __CYGWIN__ __FreeBSD__ __NetBSD__
#   __OpenBSD__ __DragonFly__ __ANDROID__ __GNUC__ __GNUC_MINOR__ __clang__
#   __TINYC__ __x86_64__ __i386__ __arm__ __aarch64__ __riscv __LP64__
#   __cplusplus __dietlibc__ SIGSTKSZ
#
# 9d. Developer debug/trace toggles (default off; edit the source to enable; no
#     build-system hook):
#   MEM_DEBUG TAL_DEBUG TAL_INFO PP_DEBUG INC_DEBUG BF_DEBUG SYM_DEBUG
#   PARSE_DEBUG ASM_DEBUG DEBUG_RELOC DEBUG_VERSION
#   PE_PRINT_SECTIONS
#
# Disabling CONFIG_TCC_BCHECK compiles out ~91 guarded blocks; TCC_IS_NATIVE
# gates ~21 (-run/JIT). Pick exactly one CPU backend (§1), layer one OS format,
# then toggle features (§3a/3b/4); §9b-9c are inferred from target + toolchain.
# ===========================================================================

# Map the detected CPU/OS to the TCC_TARGET_* define(s) the sources require
# (configure: print_num TCC_TARGET_<cpu>).
set(_tcc_cpu_define_i386    TCC_TARGET_I386)
set(_tcc_cpu_define_x86_64  TCC_TARGET_X86_64)
set(_tcc_cpu_define_arm     TCC_TARGET_ARM)
set(_tcc_cpu_define_arm64   TCC_TARGET_ARM64)
set(_tcc_cpu_define_riscv64 TCC_TARGET_RISCV64)
# --- Compiler identity (configure: conftest.c compiler/version/minor probes) -
# config.h carries CC_NAME / GCC_MAJOR / GCC_MINOR; the compiler core ignores
# them but tests/tcctest.c branches on `CC_NAME == CC_clang` etc.
if(CMAKE_C_COMPILER_ID MATCHES "Clang")          # AppleClang or Clang
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

# --- ARM sub-feature detection (configure: triplet + /proc/cpuinfo probe) ----
# Ask the host compiler for its predefined macros and read off EABI/VFP/hardfloat
# /idiv and the ARM arch version, the same facts configure derives by other means.
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

# Promote the autodetected ARM ABI facts into real, user-overridable cache
# nodes (configure exposed none of these; here autodetection seeds the DEFAULT
# and an explicit -DTCC_ARM_*/-DTCC_CPUVER always wins -- the same override
# contract as every other node). They are hidden off-arm via VISIBLE_WHEN and
# the config.h emit below reads the node values, not the detection accumulators.
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

# --- Triplet / crt / libpath detection (configure lines ~471-518) ------------
# Native, non-Windows: find where crt1.o/crti.o/crtn.o and the system libs live.
# Debian/Ubuntu use a multiarch dir /usr/lib/<triplet>; most 64-bit distros use
# /usr/lib64. Only fills values the user did NOT override.
if(NOT TCC_TARGETOS STREQUAL "WIN32" AND NOT CMAKE_CROSSCOMPILING)
    if("${TCC_TRIPLET}" STREQUAL "")
        # `cc -dumpmachine` is the closest CMake analogue of configure's conftest
        # TRIPLET probe (e.g. x86_64-linux-gnu on Debian, x86_64-pc-linux-gnu else).
        execute_process(COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
            OUTPUT_VARIABLE _dumpmachine OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_dumpmachine AND EXISTS "/usr/lib/${_dumpmachine}/crti.o")
            set(TCC_TRIPLET "${_dumpmachine}")   # multiarch layout -> CONFIG_TRIPLET
        endif()
    endif()
    # No multiarch dir but a populated /usr/lib64 (x86_64/arm64/riscv64): point
    # libs + crt at /usr/lib64 (configure's `/usr/lib64/crti.o` branch). Without
    # this, tcc.h defaults to /usr/lib, which is 32-bit on a multilib host.
    if("${TCC_TRIPLET}" STREQUAL "" AND "${TCC_LIBPATHS}" STREQUAL ""
       AND TCC_CPU MATCHES "^(x86_64|arm64|riscv64)$"
       AND EXISTS "/usr/lib64/crti.o")
        set(TCC_LIBPATHS "{B}:/usr/lib64")
        if("${TCC_CRTPREFIX}" STREQUAL "")
            set(TCC_CRTPREFIX "/usr/lib64")
        endif()
    endif()
    message(STATUS "tcc paths: triplet='${TCC_TRIPLET}' libpaths='${TCC_LIBPATHS}' crtprefix='${TCC_CRTPREFIX}'")
    # Suggest the alternate-libc switches when the matching loader is present and
    # nothing was configured (configure lines ~486-493).
    if("${TCC_CONFIG_LIBC}" STREQUAL "")
        if(EXISTS "/lib/ld-uClibc.so.0")
            message(STATUS "Perhaps you want -DTCC_CONFIG_LIBC=uClibc")
        endif()
        if(EXISTS "/lib/ld-musl-${TCC_CPU}.so.1")
            message(STATUS "Perhaps you want -DTCC_CONFIG_LIBC=musl")
        endif()
    endif()
endif()

# --- Host compiler flag tuning (configure lines ~532-552; Makefile fortify) --
# configure probes a couple of warning flags and adds them to CFLAGS for non-tcc
# host compilers; the Makefile disables _FORTIFY_SOURCE on the buggy gcc 4.0-4.4.
# These apply to every host-compiled TU (libtcc, tcc, tcc_p/_c/_s), exactly like
# the global CFLAGS in the Makefile, so we use add_compile_options().
include(CheckCCompilerFlag)
if(NOT TCC_CC_NAME STREQUAL "tcc" AND NOT MSVC)
    # (Removed -Wdeclaration-after-statement: TCC now targets C99 and
    # deliberately uses mixed declarations / declare-in-for; see CodingStyle.)
    # OPT2 disable-list: gcc warns on unused-result; clang gets nothing here
    if(NOT TCC_CC_NAME STREQUAL "clang")
        check_c_compiler_flag("-Wno-unused-result" TCC_HAS_WNO_UNUSED_RESULT)
        if(TCC_HAS_WNO_UNUSED_RESULT)
            add_compile_options(-Wno-unused-result)
        endif()
    endif()
endif()
# gcc4 _FORTIFY_SOURCE workaround (Makefile: gcc major 4, minor NOT in 5..9).
if(TCC_CC_NAME STREQUAL "gcc" AND TCC_GCC_MAJOR EQUAL 4
   AND NOT TCC_GCC_MINOR MATCHES "^[5-9]$")
    add_compile_options(-D_FORTIFY_SOURCE=0)
endif()

# --- TCC_DIAGNOSTICS: verbose-warning + debug flag set for the core compiler --
# Applied (in sections 7/8) only to libtcc + the tcc driver, which together
# compile every core TU, so the test fixtures in tests/ are not hardened.
# The set is CURATED so the signal stays clean: -Wall -Wextra (+ a clean
# -Wnull-dereference) catch real issues, while tcc's pervasive, intentional
# idioms -- each audited benign -- are silenced rather than churned:
#   -Wsign-compare      : signed/unsigned mixing throughout the compiler;
#   -Wunused-parameter  : uniform handler/callback signatures;
#   -Wtype-limits       : target-conditional constant comparisons (e.g.
#                         CONFIG_RUNMEM_RO==0 makes `f >= 0` always true here);
#   -Wunterminated-string-initialization (gcc>=15): the fixed-width ar
#                         header fields that deliberately omit the NUL terminator.
# Every flag is probed with check_c_compiler_flag so a non-GCC/Clang or older
# compiler simply drops the ones it does not understand.
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

# --- OS-specific host link flags (Makefile lines ~33-77) ---------------------
# macOS (CONFIG_OSX, non-tcc host compiler): a flat namespace + tolerate
# undefined symbols (the latter is deprecated in clang >= 15), and pin the
# deployment target the Makefile exports (MACOSX_DEPLOYMENT_TARGET := 10.6).
if(TCC_TARGETOS STREQUAL "Darwin" AND NOT TCC_CC_NAME STREQUAL "tcc")
    add_link_options(-flat_namespace)
    if(TCC_CC_NAME STREQUAL "clang" AND TCC_GCC_MAJOR LESS 15)
        add_link_options(-undefined warning)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6" CACHE STRING
            "Makefile: MACOSX_DEPLOYMENT_TARGET" FORCE)
    endif()
    # M1 with -DTCC_CPU=x86_64: build a wholly-x86_64 tcc that runs under Rosetta
    # (Makefile/configure: CFLAGS/LDFLAGS += -arch $cpu when cpu_sys is arm64).
    if(TCC_CPU STREQUAL "x86_64" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        add_compile_options(-arch x86_64)
        add_link_options(-arch x86_64)
    endif()
endif()

# WIN32 host link flags (Makefile lines ~27-37):
if(TCC_TARGETOS STREQUAL "WIN32")
    # mingw gcc: link -static to avoid libgcc_s_dw2-1.dll &c. runtime deps
    # (configure: `default LDFLAGS -static` when cc is gcc-based).
    if(TCC_CC_NAME STREQUAL "gcc")
        add_link_options(-static)
    endif()
    # Strip release binaries (Makefile: `LDFLAGS += -s`) unless debug/clang.
    if(NOT TCC_CC_NAME STREQUAL "clang"
       AND NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        add_link_options(-s)
    endif()
endif()

# Default the install prefix to a project-local directory instead of the
# system-absolute /usr/local. Without an explicit -DCMAKE_INSTALL_PREFIX, an
# in-tree build should not assume write access to (or pollute) system paths;
# it installs into <build>/install, a self-contained tree. CMake only reports
# the prefix as "initialized to default" when the user did NOT set it, so an
# explicit prefix on the command line always wins.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install"
        CACHE PATH "Install prefix (default: project-local <build>/install)" FORCE)
    message(STATUS "tcc install prefix: defaulted to local '${CMAKE_INSTALL_PREFIX}' "
                   "(pass -DCMAKE_INSTALL_PREFIX=/usr/local for a system install)")
endif()

# CONFIG_TCCDIR = install lib/tcc ({B}). Honors CMAKE_INSTALL_PREFIX; empty on
# WIN32 (configure: tccdir_auto -> runtime lookup, no #define emitted).
include(GNUInstallDirs)
tcc_config_node(TCC_INSTALL_TCCDIR TYPE STRING DEFAULT "" GROUP "Install" ADVANCED
    HELP "Runtime tccdir (CONFIG_TCCDIR); empty = <prefix>/<libdir>/tcc")

# CONFIG_TCC_PREDEFS: 1 = compile tccdefs_.h into the binary (configure default),
# 0 = load <tccdefs.h> at runtime. Section 5 generates tccdefs_.h via the c2str
# host tool.
if(TCC_CONFIG_PREDEFS)
    set(_tcc_predefs_value 1)
else()
    set(_tcc_predefs_value 0)
endif()

# ===========================================================================
# 2z. CONFIG NODE GRAPH — finalize (visibility, validation, report)
# ===========================================================================
# Everything the node graph needs is now resolved: TCC_CPU / TCC_TARGETOS
# (section 2), the per-OS default overrides, and CMAKE_C_COMPILER_ID. Register
# the derived target facts as read-only STATUS nodes, hide the knobs that don't
# apply to this target, validate the combination, and print the resolved config.
tcc_status_node(TCC_CPU         "${TCC_CPU}")
tcc_status_node(TCC_TARGETOS    "${TCC_TARGETOS}")
tcc_status_node(host_compiler   "${TCC_CC_NAME} (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION})")
tcc_status_node(cross_compiling "${CMAKE_CROSSCOMPILING}")

# Emulator launcher for running the freshly-built *native* tcc when it is a
# foreign binary (a real CMAKE_CROSSCOMPILING whole-build via a toolchain file).
# The build compiles libtcc1.a -- and optionally the coverage tcc, impdef, and
# the test suite -- by executing the just-built `tcc`; if that tcc targets a
# foreign host it cannot run here without an emulator. We honor CMake's standard
# CMAKE_CROSSCOMPILING_EMULATOR (e.g. -DCMAKE_CROSSCOMPILING_EMULATOR=wine for a
# WIN32 target, or qemu-arm for arm). Empty for an ordinary native build; the
# FATAL guard for "crosscompiling, runtime needed, no emulator" lives in
# tcc_validate_config(). (The separate TCC_ENABLE_CROSS model builds
# host-runnable cross *compilers* and never needs this.)
set(TCC_EMULATOR "${CMAKE_CROSSCOMPILING_EMULATOR}")
if(CMAKE_CROSSCOMPILING)
    tcc_status_node(emulator "${TCC_EMULATOR}")
endif()

if(TCC_CPU STREQUAL "arm")
    # ARM ABI facts are autodetected (cc -dM) then exposed as overridable nodes;
    # report the resolved (post-override) values.
    tcc_status_node(arm_abi "eabi=${TCC_ARM_EABI} vfp=${TCC_ARM_VFP} hardfloat=${TCC_ARM_HARDFLOAT} idiv=${TCC_ARM_IDIV} cpuver=${TCC_CPUVER}")
endif()

tcc_apply_node_visibility()    # hide irrelevant knobs (cosmetic; value-preserving)
tcc_validate_config()          # enum/range + invalid-combination checks
tcc_report_config()            # STATUS summary of the resolved configuration
if(TCC_REGEN_DOCS)             # opt-in: render the node table into the build dir
    tcc_generate_node_doc("${CMAKE_BINARY_DIR}/config-nodes.md")
endif()

# --- Build the config.h body --------------------------------------------------
# Faithful port of configure's print_str/print_num emission (see the section 3a
# catalog). `tcc.h` supplies every default via `#ifndef`, so we emit a macro ONLY
# when it differs from that default -> config.h stays small.

# emit `#define NAME "VAL"` into ${var}, but only when VAL is non-empty
# (mirrors configure's print_str, which skips empty values).
macro(tcc_def_str _var _name _val)
    if(NOT "${_val}" STREQUAL "")
        string(APPEND ${_var} "#define ${_name} \"${_val}\"\n")
    endif()
endmacro()

# ---- native-target guard block (configure: the `for v in $cpu $confvars`) ----
set(_nat "#define ${_tcc_cpu_define_${TCC_CPU}} 1\n")

# arm float-ABI sub-features + cpuver (configure: confvars arm_*/cpuver).
# Read the overridable nodes (seeded by autodetection above), not the raw
# detection accumulators, so an explicit -DTCC_ARM_*/-DTCC_CPUVER takes effect.
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

# OS object format / target-OS (configure: CONFIG_WIN32/OSX/BSD/Android cases)
if(TCC_TARGETOS STREQUAL "WIN32")
    string(APPEND _nat "#define TCC_TARGET_PE 1\n")
elseif(TCC_TARGETOS STREQUAL "Darwin")
    string(APPEND _nat "#define TCC_TARGET_MACHO 1\n")
elseif(TCC_TARGETOS STREQUAL "Android")
    string(APPEND _nat "#define TARGETOS_ANDROID 1\n")
elseif(TCC_TARGETOS MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    string(APPEND _nat "#define TARGETOS_${TCC_TARGETOS} 1\n")
endif()

# libc selector (configure: --config-musl / --config-uClibc)
if(TCC_CONFIG_LIBC STREQUAL "musl")
    string(APPEND _nat "#define CONFIG_TCC_MUSL 1\n")
elseif(TCC_CONFIG_LIBC STREQUAL "uClibc")
    string(APPEND _nat "#define CONFIG_TCC_UCLIBC 1\n")
endif()

# plain (default-OFF) feature toggles: emit only when enabled
if(TCC_WITH_SELINUX)
    string(APPEND _nat "#define CONFIG_SELINUX 1\n")
endif()
if(TCC_CONFIG_PIE)
    string(APPEND _nat "#define CONFIG_TCC_PIE 1\n")  # tcc.h auto-defines PIC
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

# tri-state toggles (default ON in tcc.h): emit ONLY the disabled `0` form
if(NOT TCC_CONFIG_BCHECK)
    string(APPEND _nat "#define CONFIG_TCC_BCHECK 0\n")
endif()
# CONFIG_TCC_ASM is otherwise defined (valueless) per-arch in each *-gen.c; the
# `0` form is reconciled to TCC_DISABLE_ASM in tcc.h (which gates those defines).
if(NOT TCC_CONFIG_ASM)
    string(APPEND _nat "#define CONFIG_TCC_ASM 0\n")
endif()
if(NOT TCC_CONFIG_BACKTRACE)
    string(APPEND _nat "#define CONFIG_TCC_BACKTRACE 0\n")
endif()
if(TCC_CONFIG_NEW_MACHO STREQUAL "no")
    string(APPEND _nat "#define CONFIG_NEW_MACHO 0\n")
endif()

# numeric overrides (configure: --config-dwarf / --config-semlock)
if(NOT "${TCC_CONFIG_DWARF}" STREQUAL "")
    string(APPEND _nat "#define CONFIG_DWARF_VERSION ${TCC_CONFIG_DWARF}\n")
endif()
if(NOT "${TCC_CONFIG_SEMLOCK}" STREQUAL "")
    string(APPEND _nat "#define CONFIG_TCC_SEMLOCK ${TCC_CONFIG_SEMLOCK}\n")
endif()

# path / string macros (configure: print_str of derived search paths)
tcc_def_str(_nat CONFIG_TCC_SYSINCLUDEPATHS "${TCC_SYSINCLUDEPATHS}")
tcc_def_str(_nat CONFIG_TCC_LIBPATHS        "${TCC_LIBPATHS}")
tcc_def_str(_nat CONFIG_TCC_CRTPREFIX       "${TCC_CRTPREFIX}")
tcc_def_str(_nat CONFIG_TCC_ELFINTERP       "${TCC_ELFINTERP}")
tcc_def_str(_nat CONFIG_TCC_SWITCHES        "${TCC_SWITCHES}")
tcc_def_str(_nat CONFIG_TRIPLET             "${TCC_TRIPLET}")
tcc_def_str(_nat CONFIG_OS_RELEASE          "${TCC_OS_RELEASE}")

# ---- always-emitted defines (outside the native guard) -----------------------
set(_always "")
# CONFIG_SYSROOT: only when non-default (configure print_str skips empty)
if(NOT "${TCC_SYSROOT}" STREQUAL "")
    string(APPEND _always
        "#ifndef CONFIG_SYSROOT\n# define CONFIG_SYSROOT \"${TCC_SYSROOT}\"\n#endif\n")
endif()
# CONFIG_TCCDIR: emitted everywhere except WIN32 (tccdir_auto -> runtime lookup)
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

include_directories("${CMAKE_CURRENT_BINARY_DIR}")   # find generated config.h (Makefile: -I$(TOP))
# The per-target/OS backend sources live in subdirectories (i386/, x86_64/, ...,
# windows/, linux/, macos/). Put the source root and every backend dir on the
# global include path so the quoted #includes between them (e.g. libtcc.c ->
# "i386-gen.c", i386-asm.c -> "x86_64-asm.h", tcctok.h -> "i386-tok.h") resolve
# regardless of which directory a TU is compiled from.
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

# Build-path-first runtime discovery (option TCC_AUTO_TCCDIR). Defined globally
# so every TU that compiles libtcc.c picks it up -- the native libtcc, the
# ONE_SOURCE tcc, and each cross compiler's own libtcc.c -- enabling
# tcc_auto_tccdir() in libtcc.c. Harmless in every other TU (the macro only
# guards that one function). Placed before any target so all inherit it.
if(TCC_AUTO_TCCDIR)
    add_compile_definitions(CONFIG_TCC_AUTO_TCCDIR=1)
endif()

# NOTE: config.mak is a Make-only artifact (consumed by the recursive Makefiles);
# it is intentionally NOT generated under CMake, which carries the same values as
# CMake variables/targets instead.

# ===========================================================================
# 4. GIT HASH  (Makefile: DEF_GITHASH = -DTCC_GITHASH="...")
# ===========================================================================
# Mirror of the Makefile recipe:
#   GITHASH = `git rev-parse --abbrev-ref HEAD`             (branch, else "no")
#   GITHASH = `git log -1 --date=short --pretty='%cd <branch>@%h'`
#   GITMODF = "*" when the working tree is dirty
# Applies ONLY to the tcc.c driver TU (see section 8), as in the Makefile.
# Computed at configure time; re-run CMake to refresh the hash.
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

# ===========================================================================
# 5. tccdefs_.h  (Makefile: c2str.exe converts include/tccdefs.h -> tccdefs_.h)
# ===========================================================================
# Needed only when TCC_CONFIG_PREDEFS is ON (configure default predefs=1): tccpp.c
# #includes "tccdefs_.h" under `#if CONFIG_TCC_PREDEFS`. c2str is a HOST tool
# (c2str.c), so it must run with the build machine's compiler. The generated
# header lands in the binary dir, already on the include path
# (include_directories above). libtcc depends on it (wired in section 7).
if(TCC_CONFIG_PREDEFS)
    if(CMAKE_CROSSCOMPILING)
        # c2str must RUN on the build host, but add_executable() would compile it
        # for the target. Build it here with a host compiler (CC_FOR_BUILD/HOSTCC
        # env, or TCC_HOSTCC, else cc/gcc/clang) and use that exe in the command.
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
        set(_c2str_dep "${_c2str_exe}")   # depend on the prebuilt exe, not a target
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

# ===========================================================================
# 6. SOURCE FILE LISTS  (Makefile: CORE_FILES + per-target *_FILES)
# ===========================================================================
# The source tree is organised by concern: compiler core TUs in src/, per-CPU
# backends in src/arch/{i386,x86_64,arm,arm64,riscv64}/, the OS object-format
# writers in src/objfmt/, and the vendored format tables in src/formats/. So the
# file lists are GLOBbed straight from those directories (CONFIGURE_DEPENDS
# re-runs the glob when files are added/removed). The few cross-references (the
# OS writers and i386-asm.c) are spelled out explicitly.

# Core: every src/*.c (minus tcc.c, the driver, kept out so libtcc can exclude
# it) plus src/objfmt/tccelf.c (the always-on ELF object writer).
file(GLOB TCC_CORE_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/tccelf.c")
list(REMOVE_ITEM TCC_CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tcc.c")

# Per-CPU backend: the subdirectory named after TCC_CPU *is* the source list.
# x86_64 additionally reuses the shared x86 assembler i386-asm.c.
file(GLOB TCC_BACKEND_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/${TCC_CPU}/*.c")
if(NOT TCC_BACKEND_SRC)
    message(FATAL_ERROR "No backend sources for CPU '${TCC_CPU}' (no src/arch/${TCC_CPU}/ directory)")
endif()
if(TCC_CPU STREQUAL "x86_64")
    list(APPEND TCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/arch/i386/i386-asm.c")
endif()

# OS-specific object-format writer: src/objfmt/tccpe.c (PE) or tccmacho.c (Mach-O).
if(TCC_TARGETOS STREQUAL "WIN32")
    list(APPEND TCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/tccpe.c")
elseif(TCC_TARGETOS STREQUAL "Darwin")
    list(APPEND TCC_BACKEND_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/objfmt/tccmacho.c")
endif()

# ===========================================================================
# 7. libtcc  (Makefile: libtcc.a / libtcc.so / libtcc.dll / libtcc.dylib)
# ===========================================================================
# All libtcc TUs (core + backend), minus tcc.c (driver) and tcctools.c.
set(LIBTCC_ALL_SRC ${TCC_CORE_SRC} ${TCC_BACKEND_SRC})
list(REMOVE_ITEM LIBTCC_ALL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tcctools.c")  # driver-only

# ONE_SOURCE=yes (Makefile): compile ONLY libtcc.c, which #includes every other
# TU (libtcc.c:21 defaults ONE_SOURCE to 1). Otherwise compile each TU separately
# and force ONE_SOURCE=0 so libtcc.c does NOT include the others.
if(TCC_ONE_SOURCE)
    set(LIBTCC_SRC src/libtcc.c)
else()
    set(LIBTCC_SRC ${LIBTCC_ALL_SRC})
endif()

if(TCC_BUILD_STATIC)
    add_library(libtcc STATIC ${LIBTCC_SRC})
    # clang/x86_64 needs -fPIC even in the static archive so `tcc -run` works
    # (Makefile: `ifeq ($(CC_NAME)-$(ARCH),clang-x86_64) ... -fPIC`).
    if(TCC_CC_NAME STREQUAL "clang" AND TCC_CPU STREQUAL "x86_64")
        set_target_properties(libtcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
else()
    add_library(libtcc SHARED ${LIBTCC_SRC})
    target_compile_definitions(libtcc PRIVATE LIBTCC_AS_DLL)  # Makefile: libtcc.dll DEFINES
    set_target_properties(libtcc PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(APPLE)
        # install_name @rpath/libtcc.dylib + current/compatibility version
        # (Makefile DYLIBVER). CMake's MACOSX_RPATH gives the @rpath/ install_name;
        # VERSION sets -current_version/-compatibility_version.
        set_target_properties(libtcc PROPERTIES
            MACOSX_RPATH ON VERSION "${TCC_VERSION_NUMERIC}")
    endif()
    # On ELF, CMake's default soname is `libtcc.so` (no SOVERSION) which matches
    # the Makefile's `-Wl,-soname,libtcc.so`; deliberately no versioned symlinks.
endif()
set_target_properties(libtcc PROPERTIES OUTPUT_NAME tcc PREFIX "lib")
if(NOT TCC_ONE_SOURCE)
    target_compile_definitions(libtcc PRIVATE ONE_SOURCE=0)  # Makefile: libtcc.o gets ONE_SOURCE=0
else()
    # libtcc.c pulls the others in via #include; track them so edits retrigger.
    set(_one_src_deps "")
    foreach(_s IN LISTS LIBTCC_ALL_SRC)
        if(NOT _s STREQUAL "libtcc.c")
            list(APPEND _one_src_deps "${CMAKE_CURRENT_SOURCE_DIR}/${_s}")
        endif()
    endforeach()
    set_source_files_properties(src/libtcc.c PROPERTIES OBJECT_DEPENDS "${_one_src_deps}")
endif()
# tccpp.c #includes the generated tccdefs_.h when CONFIG_TCC_PREDEFS=1 (section 5).
if(TARGET tcc_tccdefs)
    add_dependencies(libtcc tcc_tccdefs)
endif()
# NOTE: include/ holds tcc's RUNTIME headers (for libtcc1 / compiled programs),
# NOT host build headers. The Makefile only adds -I$(TOP) (config.h) + source
# dir when compiling the host objects, so we must not put include/ here.
# Interface generator expressions so both the build tree and an installed
# consumer (find_package(tcc)) resolve libtcc.h from the right place.
target_include_directories(libtcc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Link libraries (Makefile unix branch: -lm -ldl -lpthread; BSD sets ldl=no;
# win: none). CMAKE_DL_LIBS is already empty where libdl doesn't exist (BSD),
# and Threads::Threads expands to -lpthread only where needed.
if(NOT TCC_TARGETOS STREQUAL "WIN32")
    target_link_libraries(libtcc PUBLIC m)
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(libtcc PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    endif()
endif()
# --extra-libs (configure: extralibs, appended to LIBS). PUBLIC so the tcc driver
# inherits them through libtcc, like the Makefile's global $(LIBS).
if(NOT "${TCC_EXTRA_LIBS}" STREQUAL "")
    separate_arguments(_tcc_extra_libs NATIVE_COMMAND "${TCC_EXTRA_LIBS}")
    target_link_libraries(libtcc PUBLIC ${_tcc_extra_libs})
endif()
# Verbose-warning + debug diagnostics on the core library (TCC_DIAGNOSTICS).
if(TCC_DIAGNOSTICS)
    target_compile_options(libtcc PRIVATE ${TCC_DIAG_FLAGS})
endif()

# ===========================================================================
# 8. tcc driver executable  (Makefile: tcc$(EXESUF): tcc.o $(LIBTCC))
# ===========================================================================
# tcctools.c is #included by tcc.c (Makefile lists it only as a dependency of
# tcc.o), so it is NOT compiled as a separate TU here.
add_executable(tcc src/tcc.c)
set_source_files_properties(src/tcc.c PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tcctools.c")
target_compile_definitions(tcc PRIVATE ONE_SOURCE=0)  # Makefile: tcc.o gets ONE_SOURCE=0
target_link_libraries(tcc PRIVATE libtcc)
# -DTCC_GITHASH only on the driver TU (Makefile: applied to $(X)tcc.o alone).
# The tcc target compiles only tcc.c, so a PRIVATE def stays scoped to it.
if(TCC_GITHASH)
    target_compile_definitions(tcc PRIVATE "TCC_GITHASH=\"${TCC_GITHASH}\"")
endif()
# -rpath for a shared libtcc (Makefile: LINK_LIBTCC). For the build tree CMake
# already bakes in an rpath to the libtcc build dir; here we set the INSTALL_RPATH
# so the installed tcc finds the installed libtcc, honoring --disable-rpath.
if(NOT TCC_BUILD_STATIC AND NOT TCC_DISABLE_RPATH)
    if(APPLE)
        set_target_properties(tcc PROPERTIES
            INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR};${CMAKE_INSTALL_FULL_LIBDIR}")
    else()
        set_target_properties(tcc PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    endif()
endif()
# Verbose-warning + debug diagnostics on the tcc driver TU (TCC_DIAGNOSTICS).
if(TCC_DIAGNOSTICS)
    target_compile_options(tcc PRIVATE ${TCC_DIAG_FLAGS})
endif()

# --- Optional diagnostic builds (Makefile: tcc_p / tcc_c / tcc_s) ------------
# Built as ONE_SOURCE amalgamations from tcc.c (which, with ONE_SOURCE defaulting
# to 1, pulls in libtcc.c -> all TUs). Off by default; opt in with -DTCC_BUILD_*.
# (Declared as nodes up in section 1 so they join the graph/report/validation.)

# tcc_p: -pg -static -DCONFIG_TCC_STATIC -DTCC_PROFILE (Makefile CFLAGS_P).
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
    if(TARGET tcc_tccdefs)   # amalgamation pulls in tccpp.c -> tccdefs_.h
        add_dependencies(tcc_p tcc_tccdefs)
    endif()
endif()

# tcc_s: -fsanitize=address,undefined (Makefile tcc_s).
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

# tcc_c: in the Makefile, coverage tcc is compiled by the freshly-built tcc
# itself (`$(TCC) tcc.c -o tcc_c -ftest-coverage`). Wired as a custom command so
# it depends on the native tcc + libtcc1 runtime (defined in section 9).
# (Registered after libtcc1 below, where $<TARGET_FILE:tcc> and the runtime exist.)

# ===========================================================================
# 9. libtcc1.a runtime   (inlined; mirrors lib/Makefile)
# ===========================================================================
# KEY FACT: libtcc1.a is compiled by the freshly-built `tcc` (XCC = $(TOP)/tcc),
# NOT by the host compiler. Each object is produced via a custom command invoking
# $<TARGET_FILE:tcc>; the archive is built with `tcc -ar rcs`. (lib/Makefile:
#   $(X)%.o : %.c $(TCC)  ->  $(XCC) -c $< -o $@ $(XFLAGS),  $(XAR) rcs $@ ...)
# The EXTRA_O objects (runmain/bt-*/bcheck) are emitted standalone (installed into
# tccdir), NOT archived. The whole thing is parameterized so the cross compilers
# in section 10 reuse it for each <target>-libtcc1.a.

# EXTRA_O = objects emitted standalone, never archived (lib/Makefile line 59).
set(TCC_LIBTCC1_EXTRA_O runmain bt-exe bt-dll bt-log bcheck)

# Map a libtcc1 object basename to its source file (lib/ or win32/lib/, .c or .S;
# crt1w/wincrt1w are built from the non-w sources, lib/Makefile lines 99-100).
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
        # (atomic/alloca/alloca-bt/pic86 were ported from .S to .c -- file-scope
        #  __asm__ -- so they fall through to the .c default below.)
    else()
        set(${outvar} "${_lib}/${obj}.c" PARENT_SCOPE)
    endif()
endfunction()

# Build one libtcc1.a (+ standalone EXTRA_O objects) for (cpu, os), compiled by
# the tcc executable target ${tcctgt}. native=ON adds the Nat/Cbt/Cbc objects.
#   tname       : custom-target name (e.g. libtcc1 or x86_64-win32-libtcc1)
#   tcctgt      : tcc executable target used as the compiler
#   cpu, os     : tcc canonical cpu / TCC_TARGETOS
#   native      : ON for the host runtime (Nat/Cbt/Cbc), OFF for cross
#   archive     : output .a path
#   objdir      : where to drop .o files
#   out_extra   : parent-scope var name to receive standalone EXTRA_O .o paths
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

    # native-only objects (lib/Makefile Nat/Cbt/Cbc guards)
    if(native)
        list(APPEND _objs runmain tcov)                         # Nat
        if(TCC_CONFIG_BACKTRACE)
            list(APPEND _objs bt-exe bt-log)                    # Cbt
            if(_win)
                list(APPEND _objs bt-dll)
            endif()
            if(TCC_CONFIG_BCHECK)
                list(APPEND _objs bcheck)                       # Cbc
            endif()
        endif()
    endif()

    # per-OBJ extras (lib/Makefile OBJ-<target> lines)
    if(_win)
        # WIN_O (lib/Makefile:44). chkstk.S is x86/arm64 asm and is added ONLY to
        # i386/x86_64/arm64-win32 (Makefile:64-68); OBJ-arm-wince (Makefile:75)
        # omits it -- assembling x86 chkstk.S with arm-wince-tcc fails otherwise.
        list(APPEND _objs crt1 crt1w wincrt1 wincrt1w dllcrt1 dllmain winex)
        if(cpu STREQUAL "i386" OR cpu STREQUAL "x86_64" OR cpu STREQUAL "arm64")
            list(APPEND _objs chkstk)
        endif()
    elseif(_osx)
        if(cpu STREQUAL "x86_64")
            list(APPEND _objs va_list)
        endif()
    else()   # unix (linux/bsd): + per-cpu extra + LIN_O (dsohandle)
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

    # XFLAGS (lib/Makefile XFLAGS-unx / XFLAGS-win, then `XFLAGS += -I$(TOP)`).
    # -B lets tcc find include/; -I gives the headers; -I<build> is the $(TOP)
    # that holds config.h (needed by native bt-exe.c / bcheck.c -> tcc.h).
    # tcc.h pulls in the per-arch backend (TARGET_DEFS_ONLY -> "<arch>-gen.c"),
    # libtcc.h (include/), and the format tables (src/formats/), so -I src, the
    # public header dir, the format dir, and every backend/objfmt dir.
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

    # tcc finds libtcc1.a AND the standalone EXTRA_O objects (runmain.o, bcheck.o,
    # bt-*.o) in the SAME directory {B} at runtime, so the extras must sit next to
    # the archive (lib/Makefile emits both into $(TOP)). Archived objects are just
    # intermediates -> objdir.
    get_filename_component(_extra_dir "${archive}" DIRECTORY)
    file(MAKE_DIRECTORY "${objdir}")

    # USEGCC (native only, lib/Makefile <target>-libtcc1-usegcc=yes): compile the
    # objects with the host CC + AR instead of the freshly-built tcc. -B/-I tcc
    # flags are replaced by gcc-style ones; bcheck gets -gdwarf/-gstabs (BFLAGS).
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

    # The NATIVE runtime is compiled by the just-built host `tcc`; under a
    # CMAKE_CROSSCOMPILING whole-build that tcc is foreign, so run it through the
    # emulator. The cross compilers (native=OFF) are always host-runnable -> no
    # launcher. (Validated to be non-empty when crosscompiling, see section 2z.)
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
                list(APPEND _flags -bt)   # BFLAGS (lib/Makefile)
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

# Native runtime: libtcc1.a built by the host `tcc` for the host (cpu, os).
tcc_build_libtcc1(libtcc1 tcc "${TCC_CPU}" "${TCC_TARGETOS}" ON
    "${CMAKE_BINARY_DIR}/libtcc1.a" "${CMAKE_BINARY_DIR}/lib"
    LIBTCC1_EXTRA_OBJS)

# Make the build dir a complete, self-contained tccdir: libtcc1.a already lands
# at the build root (above), so mirror the install layout's headers too --
# {B}/include = runtime/include/*.h (which includes tcclib.h). With this in place the
# build dir IS a valid tccdir, which is exactly what TCC_AUTO_TCCDIR's runtime
# discovery (tcc_auto_tccdir in libtcc.c) keys off and what `tcc -B<build>`
# uses. Done unconditionally (not gated on tests) so a bare `cmake --build`
# yields a runnable in-tree tcc that resolves <stddef.h>/<stdarg.h>/libtcc1.a
# with no -B and no host CPATH.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${CMAKE_BINARY_DIR}/include" FILES_MATCHING PATTERN "*.h")

# tcc_c coverage build (Makefile: the freshly-built tcc compiles itself). Needs
# config.h (-I build), headers + runtime (-B src + -B build for libtcc1.a).
if(TCC_BUILD_COVERAGE)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/tcc_c"
        COMMAND ${TCC_EMULATOR} "$<TARGET_FILE:tcc>" "${CMAKE_CURRENT_SOURCE_DIR}/src/tcc.c"
                -o "${CMAKE_BINARY_DIR}/tcc_c" -ftest-coverage
                "-I${CMAKE_BINARY_DIR}" "-I${CMAKE_CURRENT_SOURCE_DIR}"
                "-B${CMAKE_CURRENT_SOURCE_DIR}" "-B${CMAKE_BINARY_DIR}"
        DEPENDS tcc libtcc1 "${CMAKE_CURRENT_SOURCE_DIR}/src/tcc.c"
        COMMENT "tcc -ftest-coverage tcc.c -> tcc_c" VERBATIM)
    # Drive the build with a target whose name differs from the output file's
    # basename: a custom target named `tcc_c` becomes a Ninja phony that collides
    # with the real `${CMAKE_BINARY_DIR}/tcc_c` file ("multiple rules generate
    # tcc_c" / "phony names itself as an input"). The binary is still named tcc_c.
    add_custom_target(tcc_cov ALL DEPENDS "${CMAKE_BINARY_DIR}/tcc_c")
endif()

# ===========================================================================
# 10. CROSS COMPILERS  (Makefile: TCC_X list, PROGS_CROSS, LIBTCC1_CROSS)
# ===========================================================================
# TCC_X = i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm64-win32
#         arm-wince riscv64 arm64-osx
if(TCC_ENABLE_CROSS)
    # Install dir for the cross libtcc1 archives (= native tccdir).
    if("${TCC_INSTALL_TCCDIR}" STREQUAL "")
        set(_tccdir_install "${CMAKE_INSTALL_FULL_LIBDIR}/tcc")
    else()
        set(_tccdir_install "${TCC_INSTALL_TCCDIR}")
    endif()

    # TCC_X cross targets (Makefile lines 136-137). Each <target>-tcc is a single
    # amalgamated TU: tcc.c defaults ONE_SOURCE=1, so it #includes libtcc.c (all
    # backends, selected by the DEF-<target> macros) + tcctools.c. This mirrors
    # the Makefile's cross rule (ONE_SOURCE defaults to yes for cross).
    set(TCC_X i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64
              arm64-win32 arm-wince riscv64 arm64-osx)
    foreach(_t IN LISTS TCC_X)
        # DEF-<target> macro table (Makefile lines 99-123) + cpu/os for the runtime.
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

        # Default sysroot/search paths for the Linux cross compilers (Makefile
        # lines 170-187): for a bare-arch target with a known GNU triplet, assume
        # the support files live under /usr/<triplet> and point CONFIG_SYSROOT
        # ({R}) + the include/lib/crt search lists there. The -win32/-osx/-wince
        # variants have no TRIPLET-<T> in the Makefile, so they get none of this.
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
                "CONFIG_SYSROOT=\"/usr/${_ctriplet}\""       # ROOT-$T
                "CONFIG_TCC_CRTPREFIX=\"{R}/lib\""           # CRT-$T
                "CONFIG_TCC_LIBPATHS=\"{R}/lib:{B}\""        # LIB-$T
                "CONFIG_TCC_SYSINCLUDEPATHS=\"{B}/include:{R}/include\"")  # INC-$T
        endif()

        add_executable(${_t}-tcc src/tcc.c)
        # config.h is on the global include path; libtcc.c/tcctools.c resolve
        # relative to tcc.c. CONFIG_TCC_CROSSPREFIX prefixes libtcc1/tool names.
        target_compile_definitions(${_t}-tcc PRIVATE ${_cdefs} ${_cpath_defs}
            "CONFIG_TCC_CROSSPREFIX=\"${_t}-\"")
        if(_cos STREQUAL "WIN32")
            target_compile_definitions(${_t}-tcc PRIVATE   # Makefile DEF-win
                "CONFIG_TCCDIR=\"${_tccdir_install}/win32\"")
        endif()
        if(TCC_GITHASH)
            target_compile_definitions(${_t}-tcc PRIVATE "TCC_GITHASH=\"${TCC_GITHASH}\"")
        endif()
        if(TARGET tcc_tccdefs)
            add_dependencies(${_t}-tcc tcc_tccdefs)
        endif()
        install(TARGETS ${_t}-tcc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

        # cross runtime archive, compiled BY this cross tcc (native=OFF: no
        # backtrace/bcheck/runmain).
        tcc_build_libtcc1(${_t}-libtcc1 ${_t}-tcc "${_ccpu}" "${_cos}" OFF
            "${CMAKE_BINARY_DIR}/${_t}-libtcc1.a"
            "${CMAKE_BINARY_DIR}/lib-${_t}" _cross_extra)
        install(FILES "${CMAKE_BINARY_DIR}/${_t}-libtcc1.a"
            DESTINATION "${_tccdir_install}")
    endforeach()
endif()

# ===========================================================================
# 11. WIN32 import lib  (Makefile: libtcc.def via `tcc -impdef libtcc.dll`)
# ===========================================================================
# When targeting WIN32 with a shared libtcc, the just-built tcc generates the
# import .def from the DLL (Makefile: `$(XTCC) -impdef libtcc.dll -o libtcc.def`).
if(TCC_TARGETOS STREQUAL "WIN32" AND NOT TCC_BUILD_STATIC)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/libtcc.def"
        COMMAND ${TCC_EMULATOR} "$<TARGET_FILE:tcc>" -impdef "$<TARGET_FILE:libtcc>"
                -o "${CMAKE_BINARY_DIR}/libtcc.def"
        DEPENDS tcc libtcc
        COMMENT "tcc -impdef libtcc.dll -> libtcc.def" VERBATIM)
    add_custom_target(libtcc_def ALL DEPENDS "${CMAKE_BINARY_DIR}/libtcc.def")
endif()

# ===========================================================================
# 13. INSTALL  (Makefile: install-unx / install-win, uninstall-*)
# ===========================================================================
include(GNUInstallDirs)
# tccdir (= libdir/tcc) holds the runtime: libtcc1.a + the standalone EXTRA_O.
if("${TCC_INSTALL_TCCDIR}" STREQUAL "")
    set(_tccdir "${CMAKE_INSTALL_FULL_LIBDIR}/tcc")
else()
    set(_tccdir "${TCC_INSTALL_TCCDIR}")
endif()

# tcc driver (Makefile install-unx: IBw $(PROGS) -> bindir). Cross *-tcc and
# *-libtcc1.a are installed in section 10. install-strip is `cmake --install
# --strip`, so no separate CONFIG_strip handling is needed here.
install(TARGETS tcc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# libtcc: shared -> bindir-style perms but into libdir; static -> libdir
# (Makefile: into $(libdir); RUNTIME handles the Windows DLL). EXPORT records
# the target so downstream projects can find_package(tcc) (see below).
install(TARGETS libtcc EXPORT tccTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
# libtcc.h -> includedir (Makefile: IF $(TOPSRC)/libtcc.h "$(includedir)").
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/libtcc.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# --- CMake package config: find_package(tcc) -> tcc::libtcc -----------------
# Modern consumer story. Generated entirely here (no tracked template file), so
# downstream can: find_package(tcc); target_link_libraries(app PRIVATE tcc::libtcc).
include(CMakePackageConfigHelpers)
set(_tcc_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/tcc")
install(EXPORT tccTargets
    NAMESPACE tcc::
    FILE tccTargets.cmake
    DESTINATION "${_tcc_cmakedir}")
# libtcc's PUBLIC link interface can include Threads::Threads (an imported
# target), so the consumer must resolve it before loading tccTargets.cmake.
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

# Runtime archive + EXTRA_O standalone objects -> tccdir (Makefile:
# IFw $(LIBTCC1) $(EXTRA_O) "$(tccdir)"). LIBTCC1_EXTRA_OBJS comes from section 9.
install(FILES "${CMAKE_BINARY_DIR}/libtcc1.a" ${LIBTCC1_EXTRA_OBJS}
    DESTINATION "${_tccdir}")

# tcc runtime headers -> tccdir/include (Makefile: include/*.h + tcclib.h).
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/"
    DESTINATION "${_tccdir}/include" FILES_MATCHING PATTERN "*.h")

# On WIN32 the layout differs (win32/lib/*.def, win32/include tree, examples);
# this block installs the essentials for a Windows target.
if(TCC_TARGETOS STREQUAL "WIN32")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/include/"
        DESTINATION "${_tccdir}/include")
    # (the tcc runtime headers are already installed to ${_tccdir}/include by the
    # generic install(DIRECTORY include/) above -- no extra win32 copy needed.)
    file(GLOB _win_defs "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win32/lib/*.def")
    install(FILES ${_win_defs} DESTINATION "${_tccdir}/lib")
    if(NOT TCC_BUILD_STATIC)
        install(FILES "${CMAKE_BINARY_DIR}/libtcc.def"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL)
    endif()
    # examples (build-tcc.bat copied examples/*.c + tests/libtcc_test.c into the
    # win32 'examples' layout): ship them so the readme's examples resolve.
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/"
        DESTINATION examples FILES_MATCHING PATTERN "*.c")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/libtcc_test.c"
        DESTINATION examples)
    # Windows usage readme. Migrated here from the former standalone
    # win32/tcc-win32.txt (win32/build-tcc.bat used to copy it into doc/); the
    # text is the single source now, generated at configure time and installed to
    # <prefix>/doc for WIN32 builds.
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

# ===========================================================================
# 13b. NATIVE BUILDER TOOL  (tools/build.c)
# ===========================================================================
# tools/build.c is the self-contained host-CC bootstrap builder (normally run as
# `cc tools/build.c -o build && ./build`). Give it a CMake target too so the file
# is built/verified by the tree. It is host-only POSIX code (system(), uname),
# so it builds only on a UNIX host that isn't cross-compiling.
if(UNIX AND NOT CMAKE_CROSSCOMPILING)
    add_executable(tccbuild tools/build.c)
endif()

# ===========================================================================
# 14. TESTS   (inlined; mirrors tests/Makefile + tests2 + pp)
# ===========================================================================
# All tests run the freshly-built `tcc` against fixtures and diff output, so
# every add_test() depends on the tcc / libtcc / libtcc1 targets being built.
if(TCC_BUILD_TESTS)
    enable_testing()
    set(_tcc_exe "$<TARGET_FILE:tcc>")
    set(_bdir "${CMAKE_BINARY_DIR}")                          # {B}: runtime+libtcc1
    set(_idir "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")          # tcc's own headers

    # (The former per-file `run_tcc_test.cmake` driver + tests/{tests2,pp}/*.expect
    # were replaced by the native-C tests2_runner below; its goldens live in
    # tests/tests2_data.h.)

    # Driver: compile SRCS with tcc to an exe and run it, asserting exit 0
    # (tests/Makefile: hello-exe, vla_test-run). Optional RUNARGS.
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

    # Sibling driver: the flagship auto test (tests/Makefile: test.ref + test1/
    # test4). Compile tests/tcctest.c with the host CC (reference) and with tcc,
    # run both as plain exes (robust capture; no -run fork), and diff. TCCARGS adds
    # e.g. -b for the bound-checked variant.
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

    # Sibling driver: asm-c-connect (tests/Makefile). Build the two TUs both as a
    # single exe and as separately-assembled objects linked together, run each,
    # and diff their outputs (they must agree).
    set(_asmconnect_driver "${CMAKE_BINARY_DIR}/run_asm_c_connect.cmake")
    file(WRITE "${_asmconnect_driver}" "set(EMU \"${TCC_EMULATOR}\")\n")
    file(APPEND "${_asmconnect_driver}" [==[
set(_1 "${SRCDIR}/asm-c-connect-1.c")
set(_2 "${SRCDIR}/asm-c-connect-2.c")
set(_flags "-B${BDIR}" "-I${IDIR}")
# single-exe build
execute_process(COMMAND ${EMU} "${TCC}" ${_flags} "${_1}" "${_2}"
        -o "${OUTDIR}/asm-c-connect" RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "asm-c-connect single build failed:\n${_o}${_e}")
endif()
execute_process(COMMAND ${EMU} "${OUTDIR}/asm-c-connect"
    OUTPUT_FILE "${OUTDIR}/asm-c-connect.out1")
# separate objects then link
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

    # The build dir is already populated as a complete tccdir (include/*.h +
    # tcclib.h + libtcc1.a) in section 9, so `tcc -B<build>` here resolves
    # <stddef.h>/<tcclib.h>/... without a host CPATH.

    # CTest doesn't build targets itself; a fixture-setup test runs the build so
    # `ctest` alone produces tcc + libtcc1 before any fixture test executes.
    add_test(NAME tcc_build
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}")
    set_tests_properties(tcc_build PROPERTIES FIXTURES_SETUP TCC_BUILT)

    # --- tests2 + pp suites: native-C runner (tests/tests2_runner.c) ---------
    # Formerly a per-file `cmake -P` driver diffing tcc output against
    # tests/{tests2,pp}/*.expect. The goldens now live embedded in
    # tests/tests2_data.h and a single C program (built by the host CC) compiles+
    # runs each tests2 case and `-E -P`'s each pp case, comparing internally with
    # the same srcdir-strip + diff -Nbu (whitespace/trailing-blank) tolerance.
    # It covers exactly the cases the old driver ran (the Makefile's
    # ARGS/GEN/NORUN/stderr/-lm/other-arch specials remain out of scope -- their
    # *.expect are untouched). On a no-asm tcc the three inline-asm cases are
    # passed as skip args.
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

    # --- libtcc API test (tests/Makefile: libtest) --------------------------
    # libtcc_test compiles an in-memory program; -B points it at the runtime,
    # -I at tcc's headers (it forwards -B/-I/-L to the embedded TCCState).
    add_executable(libtcc_test tests/libtcc_test.c)
    target_link_libraries(libtcc_test PRIVATE libtcc)
    target_include_directories(libtcc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME libtest
        COMMAND libtcc_test "-B${CMAKE_BINARY_DIR}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
    set_tests_properties(libtest PROPERTIES FIXTURES_REQUIRED TCC_BUILT)

    # libtcc_test_mt (multithreaded) — dropped when backtrace is disabled, like
    # the Makefile (TESTS := filter-out libtest_mt when CONFIG_backtrace=no).
    if(TCC_CONFIG_BACKTRACE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/libtcc_test_mt.c")
        add_executable(libtcc_test_mt tests/libtcc_test_mt.c)
        target_link_libraries(libtcc_test_mt PRIVATE libtcc)
        target_include_directories(libtcc_test_mt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(libtcc_test_mt PRIVATE Threads::Threads)
        endif()
        # Compiled for parity, but not registered as a gating CTest case: the
        # multithreaded harness needs the in-tree libtcc1 reachable the way the
        # Makefile arranges (TESTINSTALL paths) and is flaky as a bare run here.
        # Run manually: ./libtcc_test_mt -B<build> -I<src>/include
    endif()

    # --- hello-run / hello-exe (tests/Makefile): the ex1.c hello world ------
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        add_test(NAME hello-run
            COMMAND ${_tcc_exe} "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include"
                    -run "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        set_tests_properties(hello-run PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        # hello-exe: compile ex1.c to an exe and run it.
        add_test(NAME hello-exe COMMAND "${CMAKE_COMMAND}"
            -DTCC=${_tcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCS=${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c
            -DOUTFILE=${CMAKE_BINARY_DIR}/hello${TCC_EXESUF} -P "${_exe_driver}")
        set_tests_properties(hello-exe PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    # --- vla_test-run (tests/Makefile): compile vla_test.c to an exe + run ---
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/vla_test.c")
        add_test(NAME vla_test-run COMMAND "${CMAKE_COMMAND}"
            -DTCC=${_tcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCS=${CMAKE_CURRENT_SOURCE_DIR}/tests/vla_test.c
            -DOUTFILE=${CMAKE_BINARY_DIR}/vla_test${TCC_EXESUF} -P "${_exe_driver}")
        set_tests_properties(vla_test-run PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    # --- abitest (tests/Makefile: abitest-cc) -------------------------------
    # abitest.c drives libtcc to JIT snippets and checks they are binary-compatible
    # with the host CC's ABI. (The abitest-tcc variant, which recompiles libtcc.c
    # with tcc, is not registered; run it via the Makefile.)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/abitest.c")
        add_executable(abitest tests/abitest.c)
        target_link_libraries(abitest PRIVATE libtcc)
        target_include_directories(abitest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME abitest-cc
            COMMAND abitest "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/runtime/include")
        set_tests_properties(abitest-cc PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    # --- tcctest auto test (tests/Makefile: test1/test4) --------------------
    # The flagship correctness test: diff the host CC's tcctest.c output against
    # tcc's. The bound-checked variant (test1b: -b) is added when bcheck is on.
    # REFFLAGS mirrors the per-host test.ref tweaks (formerly tests/Makefile and,
    # for Windows, tests/test-win32.bat): arm builds with -marm (thumb skews
    # function alignment); 32-bit-non-Windows i386 disables PIC (get_asm_string
    # trips GCC's PIC checks); WIN32 builds the reference freestanding against
    # msvcrt (the bat's REF_LINK), matching how tcc links on Windows.
    set(_tcctest_refflags "")
    if(TCC_TARGETOS STREQUAL "WIN32")
        set(_msvcrt_start "${CMAKE_CURRENT_SOURCE_DIR}/tests/msvcrt_start.c")
        if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
            set(_tcctest_refflags "-nostdlib ${_msvcrt_start} -lkernel32 msvcrt.lib")
        else() # mingw gcc/clang
            set(_tcctest_refflags
                "-nostdlib ${_msvcrt_start} -lgcc -lkernel32 c:/windows/system32/msvcrt.dll")
        endif()
    elseif(TCC_CPU STREQUAL "arm")
        set(_tcctest_refflags "-marm")
    elseif(TCC_CPU STREQUAL "i386")
        set(_tcctest_refflags "-fno-PIC -fno-PIE -Wl,-z,notext")
    endif()
    # tcctest.c contains inline assembler, so it requires an asm-enabled tcc.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/tcctest.c" AND TCC_CONFIG_ASM)
        add_test(NAME tcctest COMMAND "${CMAKE_COMMAND}"
            -DCC=${CMAKE_C_COMPILER} -DTCC=${_tcc_exe}
            -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/tcctest.c
            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DOUTDIR=${CMAKE_BINARY_DIR} "-DREFFLAGS=${_tcctest_refflags}"
            -P "${_tcctest_driver}")
        set_tests_properties(tcctest PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        if(TCC_CONFIG_BCHECK)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bcheck")  # driver writes here
            add_test(NAME tcctest-bcheck COMMAND "${CMAKE_COMMAND}"
                -DCC=${CMAKE_C_COMPILER} -DTCC=${_tcc_exe}
                -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/tests/tcctest.c
                -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -DBDIR=${_bdir} -DIDIR=${_idir}
                -DOUTDIR=${CMAKE_BINARY_DIR}/bcheck "-DREFFLAGS=${_tcctest_refflags}"
                -DTCCARGS=-b -P "${_tcctest_driver}")
            set_tests_properties(tcctest-bcheck PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        endif()
    endif()

    # --- asm-c-connect (tests/Makefile): C<->asm symbol connection ----------
    # Only on i386/x86_64 (the fixture hardcodes x86 asm). Compile the two TUs as
    # one exe and as separate objects, run both, and diff the outputs.
    if(TCC_CPU MATCHES "^(i386|x86_64)$" AND TCC_CONFIG_ASM
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm-c-connect-1.c")
        add_test(NAME asm-c-connect-test COMMAND "${CMAKE_COMMAND}"
            -DTCC=${_tcc_exe} -DBDIR=${_bdir} -DIDIR=${_idir}
            -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}/tests -DOUTDIR=${CMAKE_BINARY_DIR}
            -P "${_asmconnect_driver}")
        set_tests_properties(asm-c-connect-test PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    # Additional tests from tests/Makefile NOT registered here (each needs a
    # bespoke harness or is host/arch-fragile); run them via the Makefile:
    #   test2/test3      iterated auto tests (tcc compiles tcc then tcctest)
    #   test4_static     -static auto test (incomplete reloc types)
    #   btest / tccb     bounds-checker pass/fail matrix + self-rebuild
    #   memtest          MEM_DEBUG=2 leak check while compiling tcc.c
    #   dlltest          build libtcc as a DLL via tcc, then a tcc that links it
    #   cross-test       quick -DTCC_CROSS_TEST sanity for every CROSS-TGT
    #   asmtest/weaktest/speedtest   objdump/gcc diff, symbol weakness, timing
    # Special test MODES (tcov-test / sani-test / test-install) map to the tcc_c /
    # tcc_s diagnostic builds (section 8) + `cmake --install`; opt-in, not gating.
    # --- Ported shell harnesses (formerly tests/*.sh) -----------------------
    # The driver logic lives here and is emitted into the build dir, then run via
    # `cmake -P` -- same pattern as run_tcc_test.cmake above -- so no separate
    # .sh or .cmake files are needed. Three former scripts:
    #   i386-fastcall-abi.sh  -> ctest `i386-fastcall-abi` (when an i386 tcc exists)
    #   gcctestsuite.sh       -> opt-in target `gcctestsuite`  (needs c-torture)
    #   arm-asm-testsuite.sh  -> opt-in target `arm-asm-testsuite` (needs ARM as)
    set(TCC_GCCTESTSUITE_PATH "" CACHE PATH
        "gcc.c-torture directory for the `gcctestsuite` target")
    set(TCC_ARM_CROSS_COMPILE "" CACHE STRING
        "binutils prefix (e.g. arm-linux-gnueabi-) for `arm-asm-testsuite`")

    # (1) i386 __fastcall ABI cross-check: the i386 backend's fastcall register
    # assignment must match gcc -m32, both call directions + tcc<->tcc. Skips
    # cleanly without gcc / 32-bit multilib / an i386 tcc.
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

    # (2) gcc c-torture runner: compile (+run) each case with tcc, classifying
    # PASS/FAIL/SKIP/FAILEXE and skipping the families tcc doesn't implement.
    # Opt-in target; needs -DTCC_GCCTESTSUITE_PATH=<gcc.c-torture> (or $ENV).
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

    # (3) ARM integrated-assembler differential test: for every arm-tok.h
    # mnemonic, assemble a battery of operand forms with GNU as and with tcc and
    # diff objdump. Opt-in; needs an ARM tcc + ARM binutils.
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

    # --- orphan-source coverage --------------------------------------------
    # Standalone programs not built by any other target: the examples and the
    # boundtest/testfp/bug test programs. Their run semantics vary (X11 in ex4,
    # nonzero exits, bug.c has no main), so compile-check each with the freshly
    # built tcc -- enough to verify the file is live C the compiler accepts.
    # GLOB so new files are covered.
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
    # Windows demos need a PE-target tcc (windows.h + the win32 header tree).
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

# ===========================================================================
# 15. DEVELOPER TARGETS  (Makefile: tags / ETAGS / tar)
# ===========================================================================
# Convenience targets, each built only when its tool exists. None are part of
# `all`; invoke explicitly (e.g. `cmake --build . --target tags`).
# All source across the tree (now that backends live in subdirs); skip the build
# output and VCS dirs. (Was the Makefile's "*.[ch] include/*.h lib/*.[chS]", which
# no longer reaches the per-arch subdirectories.)
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
# `dist`: a release tarball from the current git branch (Makefile: tar). The
# Makefile names it tinycc-mob-<short-hash>; `git archive` reproduces that
# content deterministically (without the Makefile's win32 CRLF rewrite).
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

# ===========================================================================
# 16. MISC FEATURES  (status of the remaining configure/Makefile bits)
# ===========================================================================
# Handled in earlier sections:
#   * compiler flag tuning (-Wdeclaration-after-statement / -Wno-unused-result)
#     and the gcc4 _FORTIFY_SOURCE=0 workaround -> the flag-tuning block.
#   * ARM eabi/vfp/hardfloat/idiv + cpuver detection -> the ARM detection block
#     (`cc -dM`), emitted into config.h's native guard.
#   * triplet / lib64 crt detection -> the triplet/crt block.
#   * out-of-tree builds -> native to CMake (no fn_makelink symlinks needed).
#   * --extra-cflags/-ldflags/-libs -> add_compile_options/add_link_options +
#     the libtcc link (section 1/7).
#   * per-target-OS defaults (the per-target-OS defaults block, section 2):
#     - Darwin dwarf=4, codesign default, new_macho autodetect (sw_vers).
#     - per-BSD CONFIG_TCC_ELFINTERP + TARGETOS_<bsd> define.
#     - Android/Termux sysroot/triplet/libpaths/crtprefix/elfinterp/switches +
#       pie/new_dtags/dwarf=4/shared-libtcc.
#   * --config-mingw32 forcing a WIN32/PE target on a non-Windows host (section 2).
#   * host-built c2str when CMAKE_CROSSCOMPILING (TCC_HOSTCC, section 5).
#   * os_release default = host kernel release (section 2).
#   * macOS host link flags (-flat_namespace, -undefined warning for clang<15,
#     MACOSX_DEPLOYMENT_TARGET=10.6) + M1 -arch x86_64 rosetta build (section 3).
#   * WIN32 mingw -static / strip -s (section 3).
#   * uClibc/musl `Perhaps you want ...` suggestions (section 3).
#   * cross-compiler default sysroot/triplet/inc/lib/crt paths (section 10).
#   * config-extra.cmake custom-config hook (section 1).
#   * tags / ETAGS / dist developer targets (section 15).
#   * extra ctest cases: hello-exe, vla_test-run, abitest-cc, tcctest(+bcheck),
#     asm-c-connect-test (section 14).
#
# Intentionally not emitted (no CMake-side effect on this build):
#   * big-endian CONFIG_BIGENDIAN: configure writes it to config.mak only; no
#     C source consumes it (read*le/write*le handle endianness). Nothing to emit.
#   * uClibc/musl elfinterp defaults: TCC_CONFIG_LIBC selects the libc; the
#     per-libc ld.so path defaults live in tcc.h. TCC_ELFINTERP overrides them.
#   * macOS SDK/xcrun sysincludepaths: Apple-host specifics; the option plumbing
#     accepts the values, the C sources carry the platform defaults.
#
# Divergences kept deliberately (run via the Makefile if needed):
#   * lib/Makefile `<target>-libtcc1-usegcc=yes` (host-CC-built libtcc1, ~40-60%
#     faster bounds-checked objects) is now the TCC_LIBTCC1_USEGCC option
#     (section 9, native only) -- no longer a divergence.
#   * iterated/self-rebuild & host-fragile tests (test2/test3, memtest, dlltest,
#     btest/tccb, cross-test, asmtest/weaktest/speedtest): see the note at the end
#     of section 14.
#   * `make tar` win32-CRLF rewrite: the `dist` target (section 15) ships the raw
#     git content; CPack (section 17) also covers packaging.

# ===========================================================================
# 17. CPACK PACKAGING  (replaces `make tar` / the dist tarball)
# ===========================================================================
# One `cpack`/`cpack --config CPackSourceConfig.cmake` produces binary/source
# archives on every platform. Runs after all install() rules (section 13).
set(CPACK_PACKAGE_NAME "tcc")
set(CPACK_PACKAGE_VENDOR "TinyCC")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_CONTACT "tinycc-devel")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
endif()
# Per-platform binary generators (NSIS/WIX need their tools; ZIP/TGZ are universal).
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
# Source package: tarball/zip of the tree minus VCS + build dirs.
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/" "/cmake-build-[^/]*/" "/\\.idea/" "\\.o$" "\\.a$")
include(CPack)
