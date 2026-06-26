# ===========================================================================
# Tiny C Compiler - CMake build scaffold
# ===========================================================================
#
# This single file is a SCAFFOLD that mirrors the existing hand-written
# `configure` script + recursive `Makefile` / `lib/Makefile` / `tests/Makefile`.
# Every feature found in those files is represented below, either implemented or
# marked with a `TODO:` so it can be ported incrementally.
#
# Everything lives in this one CMakeLists.txt on purpose: the target detection
# (was cmake/TccDetectTarget.cmake), the config.h template (was
# cmake/config.h.in), the libtcc1 runtime build (was lib/CMakeLists.txt) and the
# test suite (was tests/CMakeLists.txt) are all inlined as sections below.
#
# Source-of-truth mapping:
#   configure        -> options, target/OS detection, config.h / config.mak gen
#   Makefile         -> tcc exe, libtcc[.a|.so|.dll|.dylib], cross compilers, docs
#   lib/Makefile     -> libtcc1.a runtime (built BY the freshly-built tcc)
#   tests/Makefile   -> test suite
#
# Status legend used throughout:
#   [DONE]  scaffolded/implemented enough to be usable
#   TODO:   feature present in configure/Makefile, not yet ported
# ===========================================================================

cmake_minimum_required(VERSION 3.16)

# --- Version (configure: `version=$(head VERSION)`) -------------------------
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TCC_VERSION LIMIT_COUNT 1)
string(STRIP "${TCC_VERSION}" TCC_VERSION)
# VERSION file may contain a non-numeric suffix (e.g. "0.9.28rc"); strip it so
# project() gets a clean x.y.z. Keep the full string for config.h / dylib.
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" TCC_VERSION_NUMERIC "${TCC_VERSION}")

project(tcc
    VERSION "${TCC_VERSION_NUMERIC}"
    LANGUAGES C ASM
    DESCRIPTION "Tiny C Compiler")

# ===========================================================================
# 1. OPTIONS  (mirror of ./configure flags)
# ===========================================================================
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

option(TCC_ENABLE_CROSS    "Build all cross compilers (Makefile: make cross)" OFF)
option(TCC_BUILD_STATIC    "Build libtcc.a (ON) instead of shared libtcc (OFF)" ON)
option(TCC_DISABLE_RPATH   "Do not bake -rpath into binaries linking libtcc.so" OFF)
option(TCC_WITH_SELINUX    "Use mmap for executable memory (tcc -run)" OFF)
option(TCC_CONFIG_MINGW32  "Build a WIN32/mingw32 target" OFF)
option(TCC_CONFIG_BCHECK   "Enable bounds checker (-b)" ON)
option(TCC_CONFIG_BACKTRACE "Enable stack backtraces (-bt / -run)" ON)
option(TCC_CONFIG_PREDEFS  "Compile tccdefs.h into the binary (c2str)" ON)
option(TCC_CONFIG_PIE      "Let tcc generate position independent executables" OFF)
option(TCC_CONFIG_PIC      "Position independent code" OFF)
option(TCC_ONE_SOURCE      "Build libtcc from a single TU (Makefile: ONE_SOURCE)" OFF)
option(TCC_BUILD_TESTS     "Build/enable the test suite (tests/Makefile)" ON)
option(TCC_BUILD_DOC       "Build documentation (man/html/info)" OFF)

set(TCC_CONFIG_LIBC ""   CACHE STRING "Target libc: '', uClibc, or musl")
set(TCC_CONFIG_DWARF ""  CACHE STRING "DWARF debug version (2..5); empty = stabs")
set(TCC_CONFIG_SEMLOCK "" CACHE STRING "CONFIG_TCC_SEMLOCK value")
set(TCC_CONFIG_NEW_MACHO "" CACHE STRING "Force apple object format: yes|no|auto")
option(TCC_CONFIG_NEW_DTAGS "Use DT_RUNPATH instead of DT_RPATH" OFF)
option(TCC_CONFIG_CODESIGN  "Use codesign on apple to sign executables" OFF)

# --- Path / string options (configure: install dirs & tcc runtime paths) ----
# Implemented via GNUInstallDirs below + cache strings here.
set(TCC_SYSROOT          "" CACHE STRING "configure --sysroot")
set(TCC_TRIPLET          "" CACHE STRING "configure --triplet")
set(TCC_SYSINCLUDEPATHS  "" CACHE STRING "configure --sysincludepaths (colon sep)")
set(TCC_LIBPATHS         "" CACHE STRING "configure --libpaths (colon sep)")
set(TCC_CRTPREFIX        "" CACHE STRING "configure --crtprefix (colon sep)")
set(TCC_ELFINTERP        "" CACHE STRING "configure --elfinterp")
set(TCC_SWITCHES         "" CACHE STRING "configure --tcc-switches")
set(TCC_OS_RELEASE       "" CACHE STRING "configure --os-release")
# TODO: --extra-cflags / --extra-ldflags / --extra-libs
#       (configure appends to CFLAGS/LDFLAGS/extralibs). Wire to CMAKE_C_FLAGS
#       and target_link_options once the basic build works.
# TODO: --cross-prefix / --cc / --ar
#       (configure prefixes the toolchain for cross builds). Prefer a CMake
#       toolchain file per target instead of replicating cross_prefix here.

# ===========================================================================
# 2. TARGET (CPU/OS) DETECTION   (inlined; was cmake/TccDetectTarget.cmake)
# ===========================================================================
#    configure: `uname` + big case statements normalizing cpu & targetos,
#    plus conftest.c runtime probe for compiler/endianness/triplet. Under CMake
#    we lean on CMAKE_SYSTEM_* / the compiler ID, which already encode most of
#    what conftest.c discovered at configure time.

# --- CPU normalization (configure: the `case "$cpu"` block) ----------------
# Maps CMAKE_SYSTEM_PROCESSOR onto tcc's canonical names: i386, x86_64, arm,
# arm64, riscv64 (c67 is cross-only; ppc/mips/s390/alpha are listed in configure
# but have no backend here).
set(_proc "${CMAKE_SYSTEM_PROCESSOR}")
set(TCC_CPUVER "")
if(_proc MATCHES "^(x86|i[3-6]86|i86pc|BePC)$")
    set(TCC_CPU "i386")
elseif(_proc MATCHES "^(x86_64|amd64|x86-64|AMD64)$")
    set(TCC_CPU "x86_64")
elseif(_proc MATCHES "^(aarch64|arm64|ARM64)$")
    set(TCC_CPU "arm64")
elseif(_proc MATCHES "^arm")
    set(TCC_CPU "arm")
    # TODO: derive cpuver (4/5/6/7) like configure's armv4l/armv5/armv6/armv7
    #       case, and the Android arm default cpuver=7.
elseif(_proc MATCHES "^riscv64$")
    set(TCC_CPU "riscv64")
else()
    # configure: `echo "Unsupported CPU"; exit 1`
    message(FATAL_ERROR "Unsupported CPU '${_proc}' (configure: Unsupported CPU)")
endif()
# TODO: honor a -DTCC_CPU=... override for cross builds (configure --cpu).

# --- OS normalization (configure: the `case $buildos`/`$targetos`) ---------
if(WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Windows|MINGW|MSYS|CYGWIN")
    set(TCC_TARGETOS "WIN32")
    set(TCC_LIBSUF ".lib")
    set(TCC_EXESUF ".exe")
    set(TCC_DLLSUF ".dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TCC_TARGETOS "Darwin")          # configure: targetos=Darwin, confvars OSX
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".dylib")
    # TODO: OSX dwarf=4, codesign default, new_macho autodetect (configure).
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD|DragonFly")
    set(TCC_TARGETOS "${CMAKE_SYSTEM_NAME}")   # configure: confvars BSD, ldl=no
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
    # TODO: per-BSD elfinterp defaults + TARGETOS_<bsd> define.
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(TCC_TARGETOS "Android")
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
    # TODO: Android/Termux sysroot, triplet, libpaths, pie, dwarf=4.
else()
    set(TCC_TARGETOS "${CMAKE_SYSTEM_NAME}")   # Linux & friends
    set(TCC_LIBSUF ".a")
    set(TCC_EXESUF "")
    set(TCC_DLLSUF ".so")
endif()
# TODO: --config-mingw32 forces WIN32 target on a non-windows host.

# --- Endianness (configure: conftest.c `bigendian` probe) ------------------
if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    set(TCC_BIGENDIAN ON)     # configure: confvars_set BIGENDIAN
else()
    set(TCC_BIGENDIAN OFF)
endif()

# TODO: triplet detection (configure: conftest.c `triplet` + crti.o probe).
#       e.g. x86_64-linux-gnu; feeds CONFIG_TRIPLET and the default lib/crt
#       search paths. Could shell out to `${CMAKE_C_COMPILER} -dumpmachine`.
# TODO: cross-build detection (cpu != host cpu, os != host os, or cross_prefix
#       set) -> configure sets build_cross and skips probes.
# TODO: compiler name + version (configure: conftest.c `compiler`/`version`/
#       `minor`). Map from CMAKE_C_COMPILER_ID / _VERSION to CC_NAME / GCC_*.

message(STATUS "tcc target: CPU=${TCC_CPU} OS=${TCC_TARGETOS} bigendian=${TCC_BIGENDIAN}")

# ===========================================================================
# 3. config.h GENERATION   (inlined; was cmake/config.h.in + configure_file)
# ===========================================================================
#    configure writes config.h from print_str/print_num of every CONFIG_*.
#    Here we build the file contents as a string and file(WRITE) it directly
#    (CMake ${} expands inline, so no separate @ONLY template is needed).
# TODO: populate every value below from the options/detection above (CC_NAME,
#       GCC_MAJOR/MINOR, triplet, sysinclude/lib/crt paths, elfinterp, sysroot,
#       tccdir, dwarf, bcheck/backtrace/pie/...).
# TODO: replicate the `#if !(TCC_TARGET_*)` guard block so config.h only fixes
#       the native target when no -DTCC_TARGET_* is passed (cross builds set it
#       on the command line, see DEF-<target> in the Makefile).

# SCAFFOLD: map the detected CPU/OS to the TCC_TARGET_* define(s) the sources
# require (configure: print_num TCC_TARGET_<cpu>). Enough to build native tcc.
set(_tcc_cpu_define_i386    TCC_TARGET_I386)
set(_tcc_cpu_define_x86_64  TCC_TARGET_X86_64)
set(_tcc_cpu_define_arm     TCC_TARGET_ARM)
set(_tcc_cpu_define_arm64   TCC_TARGET_ARM64)
set(_tcc_cpu_define_riscv64 TCC_TARGET_RISCV64)
set(_tcc_cpu_define_c67     TCC_TARGET_C67)
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
if(TCC_CPU STREQUAL "arm" AND NOT TCC_CPUVER)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_empty.c" "")
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -dM -E -
        INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/_empty.c"
        OUTPUT_VARIABLE _cc_defs ERROR_QUIET RESULT_VARIABLE _cc_defs_rc)
    if(_cc_defs_rc EQUAL 0)
        if(_cc_defs MATCHES "__ARM_ARCH[ \t]+([0-9]+)")
            set(TCC_CPUVER "${CMAKE_MATCH_1}")
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
endif()

# --- Host compiler flag tuning (configure lines ~532-552; Makefile fortify) --
# configure probes a couple of warning flags and adds them to CFLAGS for non-tcc
# host compilers; the Makefile disables _FORTIFY_SOURCE on the buggy gcc 4.0-4.4.
# These apply to every host-compiled TU (libtcc, tcc, tcc_p/_c/_s), exactly like
# the global CFLAGS in the Makefile, so we use add_compile_options().
include(CheckCCompilerFlag)
if(NOT TCC_CC_NAME STREQUAL "tcc" AND NOT MSVC)
    # OPT1 enable-list: -Wdeclaration-after-statement (if accepted)
    check_c_compiler_flag("-Wdeclaration-after-statement" TCC_HAS_WDECL_AFTER_STMT)
    if(TCC_HAS_WDECL_AFTER_STMT)
        add_compile_options(-Wdeclaration-after-statement)
    endif()
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

# CONFIG_TCCDIR = install lib/tcc ({B}). Honors CMAKE_INSTALL_PREFIX; empty on
# WIN32 (configure: tccdir_auto -> runtime lookup, no #define emitted).
include(GNUInstallDirs)
set(TCC_INSTALL_TCCDIR "" CACHE STRING
    "Runtime tccdir (CONFIG_TCCDIR); empty = <prefix>/<libdir>/tcc")

# CONFIG_TCC_PREDEFS: 1 = compile tccdefs_.h into the binary (configure default),
# 0 = load <tccdefs.h> at runtime. Honored here because section 5 now generates
# tccdefs_.h via the c2str host tool.
if(TCC_CONFIG_PREDEFS)
    set(_tcc_predefs_value 1)
else()
    set(_tcc_predefs_value 0)
endif()

# --- Build the config.h body --------------------------------------------------
# Faithful port of configure's print_str/print_num emission (see
# docs/preprocessor-flags.md). `tcc.h` supplies every default via `#ifndef`, so
# we emit a macro ONLY when it differs from that default -> config.h stays small.

# emit `#define NAME "VAL"` into ${var}, but only when VAL is non-empty
# (mirrors configure's print_str, which skips empty values).
macro(tcc_def_str _var _name _val)
    if(NOT "${_val}" STREQUAL "")
        string(APPEND ${_var} "#define ${_name} \"${_val}\"\n")
    endif()
endmacro()

# ---- native-target guard block (configure: the `for v in $cpu $confvars`) ----
set(_nat "#define ${_tcc_cpu_define_${TCC_CPU}} 1\n")

# arm float-ABI sub-features + cpuver (configure: confvars arm_*/cpuver)
if(TCC_CPU STREQUAL "arm")
    if(TCC_CPUVER)
        string(APPEND _nat "#define CONFIG_TCC_CPUVER ${TCC_CPUVER}\n")
    endif()
    if(_arm_eabi)
        string(APPEND _nat "#define TCC_ARM_EABI 1\n")
    endif()
    if(_arm_vfp)
        string(APPEND _nat "#define TCC_ARM_VFP 1\n")
    endif()
    if(_arm_hardfloat)
        string(APPEND _nat "#define TCC_ARM_HARDFLOAT 1\n")
    endif()
    if(_arm_idiv)
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
 * when it differs from that default. See docs/preprocessor-flags.md for the
 * full catalog and emission rules.
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
   || TCC_TARGET_ARM64 || TCC_TARGET_RISCV64 || TCC_TARGET_C67)
${_nat}#endif

/* ---- always-emitted defines (outside the native guard) ------------------ */
${_always}/* CONFIG_TCC_PREDEFS 1 -> tccdefs_.h compiled in (section 5);
 * 0 -> tcc loads <tccdefs.h> at runtime. */
#define CONFIG_TCC_PREDEFS ${_tcc_predefs_value}
")
message(STATUS "tcc config.h: CC_NAME=CC_${TCC_CC_NAME} predefs=${_tcc_predefs_value} "
               "bcheck=${TCC_CONFIG_BCHECK} backtrace=${TCC_CONFIG_BACKTRACE}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")   # find generated config.h (Makefile: -I$(TOP))

# TODO: config.texi generation -> `@set VERSION $version` (Makefile/doc).
# TODO: config.mak is a Make artifact; intentionally NOT generated under CMake.

# ===========================================================================
# 4. GIT HASH  (Makefile: DEF_GITHASH = -DTCC_GITHASH="...")
# ===========================================================================
# Reproduce the Makefile recipe:
#   GITHASH = `git rev-parse --abbrev-ref HEAD`             (branch, else "no")
#   GITHASH = `git log -1 --date=short --pretty='%cd <branch>@%h'`
#   GITMODF = "*" when the working tree is dirty
# Applied ONLY to the tcc.c driver TU (see section 8), as in the Makefile.
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
# (built from conftest.c with -DC2STR), so it must run with the build machine's
# compiler. The generated header lands in the binary dir, already on the include
# path (include_directories above). libtcc depends on it (wired in section 7).
if(TCC_CONFIG_PREDEFS)
    if(CMAKE_CROSSCOMPILING)
        # TODO: when CMake itself cross-compiles, import a natively-built c2str
        #       (e.g. via a host-tools ExternalProject) instead of add_executable.
        message(WARNING
            "TCC_CONFIG_PREDEFS with CMAKE_CROSSCOMPILING: c2str is built for the "
            "target and may not run on the build host. Consider -DTCC_CONFIG_PREDEFS=OFF.")
    endif()
    add_executable(c2str conftest.c)
    target_compile_definitions(c2str PRIVATE C2STR)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tccdefs_.h"
        COMMAND c2str
                "${CMAKE_CURRENT_SOURCE_DIR}/include/tccdefs.h"
                "${CMAKE_CURRENT_BINARY_DIR}/tccdefs_.h"
        DEPENDS c2str "${CMAKE_CURRENT_SOURCE_DIR}/include/tccdefs.h"
        COMMENT "Generating tccdefs_.h (c2str include/tccdefs.h)"
        VERBATIM)
    add_custom_target(tcc_tccdefs
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/tccdefs_.h")
endif()

# ===========================================================================
# 6. SOURCE FILE LISTS  (Makefile: CORE_FILES + per-target *_FILES)
# ===========================================================================
# Core sources shared by every target (Makefile: CORE_FILES, minus tcc.c which
# is the driver, kept separate so libtcc can exclude it).
set(TCC_CORE_SRC
    libtcc.c tccpp.c tccgen.c tccdbg.c tccelf.c tccasm.c tccrun.c tcctools.c)

# Per-CPU backend sources (Makefile: <cpu>_FILES). Selected by TCC_CPU.
# TODO: verify each list against the Makefile *_FILES entries.
set(TCC_BACKEND_i386    i386-gen.c i386-link.c i386-asm.c)
set(TCC_BACKEND_x86_64  x86_64-gen.c x86_64-link.c i386-asm.c)
set(TCC_BACKEND_arm     arm-gen.c arm-link.c arm-asm.c)
set(TCC_BACKEND_arm64   arm64-gen.c arm64-link.c arm64-asm.c)
set(TCC_BACKEND_riscv64 riscv64-gen.c riscv64-link.c riscv64-asm.c)
set(TCC_BACKEND_c67     c67-gen.c c67-link.c tcccoff.c)

if(NOT DEFINED TCC_BACKEND_${TCC_CPU})
    message(FATAL_ERROR "No backend source list for CPU '${TCC_CPU}' (configure: 'Unsupported CPU')")
endif()
set(TCC_BACKEND_SRC ${TCC_BACKEND_${TCC_CPU}})

# OS-specific object-format sources (Makefile: tccpe.c for PE, tccmacho.c for OSX)
# TODO: append tccpe.c when targeting WIN32, tccmacho.c when targeting OSX.
if(TCC_TARGETOS STREQUAL "WIN32")
    list(APPEND TCC_BACKEND_SRC tccpe.c)
elseif(TCC_TARGETOS STREQUAL "Darwin")
    list(APPEND TCC_BACKEND_SRC tccmacho.c)
endif()

# ===========================================================================
# 7. libtcc  (Makefile: libtcc.a / libtcc.so / libtcc.dll / libtcc.dylib)
# ===========================================================================
# All libtcc TUs (core + backend), minus tcc.c (driver) and tcctools.c.
set(LIBTCC_ALL_SRC ${TCC_CORE_SRC} ${TCC_BACKEND_SRC})
list(REMOVE_ITEM LIBTCC_ALL_SRC tcctools.c)   # tcctools.c belongs to the tcc driver

# ONE_SOURCE=yes (Makefile): compile ONLY libtcc.c, which #includes every other
# TU (libtcc.c:21 defaults ONE_SOURCE to 1). Otherwise compile each TU separately
# and force ONE_SOURCE=0 so libtcc.c does NOT include the others.
if(TCC_ONE_SOURCE)
    set(LIBTCC_SRC libtcc.c)
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
    set_source_files_properties(libtcc.c PROPERTIES OBJECT_DEPENDS "${_one_src_deps}")
endif()
# tccpp.c #includes the generated tccdefs_.h when CONFIG_TCC_PREDEFS=1 (section 5).
if(TARGET tcc_tccdefs)
    add_dependencies(libtcc tcc_tccdefs)
endif()
# NOTE: include/ holds tcc's RUNTIME headers (for libtcc1 / compiled programs),
# NOT host build headers. The Makefile only adds -I$(TOP) (config.h) + source
# dir when compiling the host objects, so we must not put include/ here.
target_include_directories(libtcc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

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

# ===========================================================================
# 8. tcc driver executable  (Makefile: tcc$(EXESUF): tcc.o $(LIBTCC))
# ===========================================================================
# tcctools.c is #included by tcc.c (Makefile lists it only as a dependency of
# tcc.o), so it is NOT compiled as a separate TU here.
add_executable(tcc tcc.c)
set_source_files_properties(tcc.c PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tcctools.c")
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

# --- Optional diagnostic builds (Makefile: tcc_p / tcc_c / tcc_s) ------------
# Built as ONE_SOURCE amalgamations from tcc.c (which, with ONE_SOURCE defaulting
# to 1, pulls in libtcc.c -> all TUs). Off by default; opt in with -DTCC_BUILD_*.
option(TCC_BUILD_PROFILE  "Also build tcc_p (profiling: -pg -static)" OFF)
option(TCC_BUILD_COVERAGE "Also build tcc_c (coverage instrumentation)" OFF)
option(TCC_BUILD_SANITIZE "Also build tcc_s (-fsanitize=address,undefined)" OFF)

# tcc_p: -pg -static -DCONFIG_TCC_STATIC -DTCC_PROFILE (Makefile CFLAGS_P).
if(TCC_BUILD_PROFILE)
    add_executable(tcc_p tcc.c)
    set_source_files_properties(tcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tcctools.c")
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
    add_executable(tcc_s tcc.c)
    set_source_files_properties(tcc.c PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tcctools.c")
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
# 9. libtcc1.a runtime   (inlined; was lib/CMakeLists.txt / lib/Makefile)
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
    set(_lib "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    set(_win "${CMAKE_CURRENT_SOURCE_DIR}/win32/lib")
    if(obj STREQUAL "crt1w")
        set(${outvar} "${_win}/crt1.c" PARENT_SCOPE)
    elseif(obj STREQUAL "wincrt1w")
        set(${outvar} "${_win}/wincrt1.c" PARENT_SCOPE)
    elseif(obj MATCHES "^(crt1|wincrt1|dllcrt1|dllmain|winex)$")
        set(${outvar} "${_win}/${obj}.c" PARENT_SCOPE)
    elseif(obj STREQUAL "chkstk")
        set(${outvar} "${_win}/chkstk.S" PARENT_SCOPE)
    elseif(obj MATCHES "^(atomic|alloca|alloca-bt|pic86)$")
        set(${outvar} "${_lib}/${obj}.S" PARENT_SCOPE)   # tcc assembles .S too
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
    if(cpu STREQUAL "c67")
        return()   # c67 has no runtime lib (lib/Makefile LIBTCC1_X filters it out)
    endif()
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
        list(APPEND _objs chkstk crt1 crt1w wincrt1 wincrt1w dllcrt1 dllmain winex)
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
    if(_win)
        set(_xflags -B${CMAKE_CURRENT_SOURCE_DIR}/win32
                    -I${CMAKE_CURRENT_SOURCE_DIR}/include
                    -I${CMAKE_CURRENT_SOURCE_DIR}
                    -I${CMAKE_BINARY_DIR})
    else()
        set(_xflags -B${CMAKE_CURRENT_SOURCE_DIR}
                    -I${CMAKE_CURRENT_SOURCE_DIR}
                    -I${CMAKE_BINARY_DIR})
    endif()

    # tcc finds libtcc1.a AND the standalone EXTRA_O objects (runmain.o, bcheck.o,
    # bt-*.o) in the SAME directory {B} at runtime, so the extras must sit next to
    # the archive (lib/Makefile emits both into $(TOP)). Archived objects are just
    # intermediates -> objdir.
    get_filename_component(_extra_dir "${archive}" DIRECTORY)
    file(MAKE_DIRECTORY "${objdir}")
    set(_archived "")
    set(_extra "")
    foreach(obj IN LISTS _objs)
        _tcc_libtcc1_src("${obj}" _src)
        set(_flags ${_xflags})
        if(obj STREQUAL "bcheck")
            list(APPEND _flags -bt)   # BFLAGS (lib/Makefile)
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
            COMMAND "$<TARGET_FILE:${tcctgt}>" -c "${_src}" -o "${_out}" ${_flags}
            DEPENDS ${tcctgt} "${_src}"
            COMMENT "tcc[${tname}] -c ${obj}"
            VERBATIM)
    endforeach()

    add_custom_command(
        OUTPUT "${archive}"
        COMMAND "$<TARGET_FILE:${tcctgt}>" -ar rcs "${archive}" ${_archived}
        DEPENDS ${_archived} ${tcctgt}
        COMMENT "tcc -ar ${tname}"
        VERBATIM)
    add_custom_target(${tname} ALL DEPENDS "${archive}" ${_extra})
    set(${out_extra} "${_extra}" PARENT_SCOPE)
endfunction()

# Native runtime: libtcc1.a built by the host `tcc` for the host (cpu, os).
tcc_build_libtcc1(libtcc1 tcc "${TCC_CPU}" "${TCC_TARGETOS}" ON
    "${CMAKE_BINARY_DIR}/libtcc1.a" "${CMAKE_BINARY_DIR}/lib"
    LIBTCC1_EXTRA_OBJS)

# tcc_c coverage build (Makefile: the freshly-built tcc compiles itself). Needs
# config.h (-I build), headers + runtime (-B src + -B build for libtcc1.a).
if(TCC_BUILD_COVERAGE)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/tcc_c"
        COMMAND "$<TARGET_FILE:tcc>" "${CMAKE_CURRENT_SOURCE_DIR}/tcc.c"
                -o "${CMAKE_BINARY_DIR}/tcc_c" -ftest-coverage
                -I"${CMAKE_BINARY_DIR}" -I"${CMAKE_CURRENT_SOURCE_DIR}"
                -B"${CMAKE_CURRENT_SOURCE_DIR}" -B"${CMAKE_BINARY_DIR}"
        DEPENDS tcc libtcc1 "${CMAKE_CURRENT_SOURCE_DIR}/tcc.c"
        COMMENT "tcc -ftest-coverage tcc.c -> tcc_c" VERBATIM)
    add_custom_target(tcc_c ALL DEPENDS "${CMAKE_BINARY_DIR}/tcc_c")
endif()

# ===========================================================================
# 10. CROSS COMPILERS  (Makefile: TCC_X list, PROGS_CROSS, LIBTCC1_CROSS)
# ===========================================================================
# TCC_X = i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm64-win32
#         arm-wince c67 riscv64 arm64-osx
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
              arm64-win32 arm-wince c67 riscv64 arm64-osx)
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
        elseif(_t STREQUAL "c67")
            set(_ccpu c67)
            set(_cdefs TCC_TARGET_C67)
        elseif(_t STREQUAL "riscv64")
            set(_ccpu riscv64)
            set(_cdefs TCC_TARGET_RISCV64)
        elseif(_t STREQUAL "arm64-osx")
            set(_ccpu arm64)
            set(_cos Darwin)
            set(_cdefs TCC_TARGET_ARM64 TCC_TARGET_MACHO)
        endif()

        add_executable(${_t}-tcc tcc.c)
        # config.h is on the global include path; libtcc.c/tcctools.c resolve
        # relative to tcc.c. CONFIG_TCC_CROSSPREFIX prefixes libtcc1/tool names.
        target_compile_definitions(${_t}-tcc PRIVATE ${_cdefs}
            "CONFIG_TCC_CROSSPREFIX=\"${_t}-\"")
        if(_cos STREQUAL "WIN32")
            target_compile_definitions(${_t}-tcc PRIVATE   # Makefile DEF-win
                "CONFIG_TCCDIR=\"${_tccdir_install}/win32\"")
        endif()
        if(_t STREQUAL "c67")
            target_compile_options(${_t}-tcc PRIVATE -w)   # DEF-c67 adds -w
        endif()
        if(TCC_GITHASH)
            target_compile_definitions(${_t}-tcc PRIVATE "TCC_GITHASH=\"${TCC_GITHASH}\"")
        endif()
        if(TARGET tcc_tccdefs)
            add_dependencies(${_t}-tcc tcc_tccdefs)
        endif()
        install(TARGETS ${_t}-tcc RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

        # cross runtime archive, compiled BY this cross tcc (native=OFF: no
        # backtrace/bcheck/runmain). c67 has no runtime lib.
        if(NOT _ccpu STREQUAL "c67")
            tcc_build_libtcc1(${_t}-libtcc1 ${_t}-tcc "${_ccpu}" "${_cos}" OFF
                "${CMAKE_BINARY_DIR}/${_t}-libtcc1.a"
                "${CMAKE_BINARY_DIR}/lib-${_t}" _cross_extra)
            install(FILES "${CMAKE_BINARY_DIR}/${_t}-libtcc1.a"
                DESTINATION "${_tccdir_install}")
        endif()
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
        COMMAND "$<TARGET_FILE:tcc>" -impdef "$<TARGET_FILE:libtcc>"
                -o "${CMAKE_BINARY_DIR}/libtcc.def"
        DEPENDS tcc libtcc
        COMMENT "tcc -impdef libtcc.dll -> libtcc.def" VERBATIM)
    add_custom_target(libtcc_def ALL DEPENDS "${CMAKE_BINARY_DIR}/libtcc.def")
endif()

# ===========================================================================
# 12. DOCUMENTATION  (Makefile doc section)
# ===========================================================================
# Best-effort, like the Makefile's run-if helper: each target builds only if its
# tool (makeinfo / perl+pod2man) is present.
if(TCC_BUILD_DOC)
    set(_texi "${CMAKE_CURRENT_SOURCE_DIR}/tcc-doc.texi")
    find_program(MAKEINFO_EXE makeinfo)
    find_program(PERL_EXE perl)
    find_program(POD2MAN_EXE pod2man)
    set(_doc_outputs "")
    if(MAKEINFO_EXE)
        add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tcc-doc.html"
            COMMAND "${MAKEINFO_EXE}" --no-split --html --number-sections
                    -o "${CMAKE_BINARY_DIR}/tcc-doc.html" "${_texi}"
            DEPENDS "${_texi}" COMMENT "makeinfo --html tcc-doc" VERBATIM)
        add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tcc-doc.info"
            COMMAND "${MAKEINFO_EXE}" -o "${CMAKE_BINARY_DIR}/tcc-doc.info" "${_texi}"
            DEPENDS "${_texi}" COMMENT "makeinfo tcc-doc.info" VERBATIM)
        list(APPEND _doc_outputs
            "${CMAKE_BINARY_DIR}/tcc-doc.html" "${CMAKE_BINARY_DIR}/tcc-doc.info")
    endif()
    if(PERL_EXE AND POD2MAN_EXE)
        # texi -> pod (texi2pod.pl) -> man page (pod2man writes in.pod out.1)
        add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tcc-doc.pod"
            COMMAND "${PERL_EXE}" "${CMAKE_CURRENT_SOURCE_DIR}/texi2pod.pl"
                    "${_texi}" "${CMAKE_BINARY_DIR}/tcc-doc.pod"
            DEPENDS "${_texi}" COMMENT "texi2pod tcc-doc" VERBATIM)
        add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/tcc.1"
            COMMAND "${POD2MAN_EXE}" --section=1 --center=Tiny\ C\ Compiler
                    --release=${TCC_VERSION}
                    "${CMAKE_BINARY_DIR}/tcc-doc.pod" "${CMAKE_BINARY_DIR}/tcc.1"
            DEPENDS "${CMAKE_BINARY_DIR}/tcc-doc.pod" COMMENT "pod2man tcc.1" VERBATIM)
        list(APPEND _doc_outputs "${CMAKE_BINARY_DIR}/tcc.1")
    endif()
    if(_doc_outputs)
        add_custom_target(tcc_docs ALL DEPENDS ${_doc_outputs})
    else()
        message(WARNING "TCC_BUILD_DOC=ON but no makeinfo/perl+pod2man found")
    endif()
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
# (Makefile: into $(libdir); RUNTIME handles the Windows DLL).
install(TARGETS libtcc
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
# libtcc.h -> includedir (Makefile: IF $(TOPSRC)/libtcc.h "$(includedir)").
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/libtcc.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Runtime archive + EXTRA_O standalone objects -> tccdir (Makefile:
# IFw $(LIBTCC1) $(EXTRA_O) "$(tccdir)"). LIBTCC1_EXTRA_OBJS comes from section 9.
install(FILES "${CMAKE_BINARY_DIR}/libtcc1.a" ${LIBTCC1_EXTRA_OBJS}
    DESTINATION "${_tccdir}")

# tcc runtime headers -> tccdir/include (Makefile: include/*.h + tcclib.h).
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${_tccdir}/include" FILES_MATCHING PATTERN "*.h")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tcclib.h"
    DESTINATION "${_tccdir}/include")

# docs (best-effort; only if produced). Mirrors install-unx man/info/html.
if(TCC_BUILD_DOC)
    install(FILES "${CMAKE_BINARY_DIR}/tcc.1"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1" OPTIONAL)
    install(FILES "${CMAKE_BINARY_DIR}/tcc-doc.info"
        DESTINATION "${CMAKE_INSTALL_INFODIR}" OPTIONAL)
    install(FILES "${CMAKE_BINARY_DIR}/tcc-doc.html"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}" OPTIONAL)
endif()

# On WIN32 the layout differs (win32/lib/*.def, win32/include tree, examples).
# Port the essentials when actually targeting Windows.
if(TCC_TARGETOS STREQUAL "WIN32")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/win32/include/"
        DESTINATION "${_tccdir}/include")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/" # tcc headers too
        DESTINATION "${_tccdir}/include" OPTIONAL)
    file(GLOB _win_defs "${CMAKE_CURRENT_SOURCE_DIR}/win32/lib/*.def")
    install(FILES ${_win_defs} DESTINATION "${_tccdir}/lib")
    if(NOT TCC_BUILD_STATIC)
        install(FILES "${CMAKE_BINARY_DIR}/libtcc.def"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL)
    endif()
endif()

# ===========================================================================
# 14. TESTS   (inlined; was tests/CMakeLists.txt / tests/Makefile + tests2 + pp)
# ===========================================================================
# All tests run the freshly-built `tcc` against fixtures and diff output, so
# every add_test() depends on the tcc / libtcc / libtcc1 targets being built.
if(TCC_BUILD_TESTS)
    enable_testing()
    set(_tcc_exe "$<TARGET_FILE:tcc>")
    set(_bdir "${CMAKE_BINARY_DIR}")                          # {B}: runtime+libtcc1
    set(_idir "${CMAKE_CURRENT_SOURCE_DIR}/include")          # tcc's own headers
    set(_driver "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_tcc_test.cmake")
    set(_t2 "${CMAKE_CURRENT_SOURCE_DIR}/tests/tests2")
    set(_pp "${CMAKE_CURRENT_SOURCE_DIR}/tests/pp")

    # CTest doesn't build targets itself; a fixture-setup test runs the build so
    # `ctest` alone produces tcc + libtcc1 before any fixture test executes.
    add_test(NAME tcc_build
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}")
    set_tests_properties(tcc_build PROPERTIES FIXTURES_SETUP TCC_BUILT)

    # --- pp subdir: tcc -E -P <f> | diff <f>.expect (tests/pp/Makefile) -----
    file(GLOB _pp_srcs "${_pp}/*.c" "${_pp}/*.S")
    foreach(_f IN LISTS _pp_srcs)
        get_filename_component(_n "${_f}" NAME_WE)
        if(EXISTS "${_pp}/${_n}.expect")
            add_test(NAME pp.${_n} COMMAND "${CMAKE_COMMAND}"
                -DTCC=${_tcc_exe} -DMODE=pp -DSRCFILE=${_f}
                -DEXPECT=${_pp}/${_n}.expect -DBDIR=${_bdir} -DIDIR=${_idir}
                -DOUTFILE=${CMAKE_BINARY_DIR}/pp_${_n}.output -P "${_driver}")
            set_tests_properties(pp.${_n} PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        endif()
    endforeach()

    # --- tests2 subdir: tcc -run <f> | diff <f>.expect (tests/tests2/Makefile) -
    # SKIP mirrors the Makefile's SKIP/ARGS/GEN/NORUN special-casing plus the
    # other-arch asm tests; these need bespoke harnesses, not plain -run+diff.
    set(_t2_skip
        31_args 46_grep                                  # need ARGS
        95_bitfields_ms 104_inline 120_alias             # GEN-ed expect
        73_arm64 138_arm64_encoding 139_arm64_errors     # other-arch asm
        140_arm64_extasm 141_riscv_asm
        112_backtrace 113_btdll                          # backtrace/dll harness
        114_bound_signal 115_bound_setjmp 116_bound_setjmp2  # NORUN / bound
        117_builtins 128_run_atexit 144_tls)             # custom flags / NORUN
    file(GLOB _t2_srcs "${_t2}/[0-9][0-9]_*.c" "${_t2}/[0-9][0-9][0-9]_*.c")
    foreach(_f IN LISTS _t2_srcs)
        get_filename_component(_n "${_f}" NAME_WE)
        if(EXISTS "${_t2}/${_n}.expect" AND NOT _n IN_LIST _t2_skip)
            add_test(NAME tests2.${_n} COMMAND "${CMAKE_COMMAND}"
                -DTCC=${_tcc_exe} -DMODE=run -DSRCFILE=${_f}
                -DEXPECT=${_t2}/${_n}.expect -DBDIR=${_bdir} -DIDIR=${_idir}
                -DOUTFILE=${CMAKE_BINARY_DIR}/t2_${_n}.output -P "${_driver}")
            set_tests_properties(tests2.${_n} PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
        endif()
    endforeach()

    # --- libtcc API test (tests/Makefile: libtest) --------------------------
    # libtcc_test compiles an in-memory program; -B points it at the runtime,
    # -I at tcc's headers (it forwards -B/-I/-L to the embedded TCCState).
    add_executable(libtcc_test tests/libtcc_test.c)
    target_link_libraries(libtcc_test PRIVATE libtcc)
    target_include_directories(libtcc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME libtest
        COMMAND libtcc_test "-B${CMAKE_BINARY_DIR}"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/include")
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
        add_test(NAME libtest_mt
            COMMAND libtcc_test_mt "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/include")
        set_tests_properties(libtest_mt PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    # --- hello-run (tests/Makefile): tcc -run a hello-world fixture ---------
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        add_test(NAME hello-run
            COMMAND ${_tcc_exe} "-B${CMAKE_BINARY_DIR}"
                    "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
                    -run "${CMAKE_CURRENT_SOURCE_DIR}/examples/ex1.c")
        set_tests_properties(hello-run PROPERTIES FIXTURES_REQUIRED TCC_BUILT)
    endif()

    # Special test modes (tcov-test / sani-test / test-install) map to building
    # tcc_c / tcc_s (section 8) and `cmake --install`; not registered as CTest
    # cases here since they are opt-in diagnostic builds.
    message(STATUS "tcc tests: registered pp + tests2 + libtcc API tests (ctest)")
endif()

# ===========================================================================
# 15. UNPORTED / MISC FEATURES still to cover
# ===========================================================================
# TODO: compiler flag tuning (configure lines ~532-549): probe & add
#       -Wdeclaration-after-statement, -Wno-unused-result / clang variants.
# TODO: gcc4 _FORTIFY_SOURCE=0 workaround (Makefile lines 15-17).
# TODO: ARM sub-feature detection: eabi/eabihf/vfp/idiv from triplet or
#       /proc/cpuinfo (configure lines ~510-529) -> TCC_ARM_* defines + cpuver.
# TODO: big-endian -> CONFIG_BIGENDIAN (configure: bigendian probe).
# TODO: uClibc/musl elfinterp selection + "Perhaps you want --config-*" hints.
# TODO: BSD elfinterp defaults per OS (configure CONFIG_BSD case).
# TODO: Android/Termux sysroot/triplet/libpaths/pie defaults (configure).
# TODO: macOS specifics: SDK include path via xcrun, new_macho autodetect by
#       product version, -arch x86_64 under rosetta, MACOSX_DEPLOYMENT_TARGET,
#       -flat_namespace / -undefined warning for old clang.
# TODO: triplet-based crt detection (/usr/lib/<triplet>/crti.o, /usr/lib64).
# TODO: out-of-tree build symlinks (configure fn_makelink) -> not needed; CMake
#       supports out-of-tree natively.
# TODO: `make tags/ETAGS`, `make tar` release tarball -> optional custom targets.
