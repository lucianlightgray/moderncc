cmake_minimum_required(VERSION 3.22)
#
# Per-triple operation-journal sweep driver.
#
# One entry point for every arch/triple/libc the tree can measure, so a sweep is
# `cmake --build . --target journal-sweep-<key>` instead of a hand-built wrapper
# script. Resolves a key to (compiler, sysroot, launcher, baseline), probes every
# prerequisite, and delegates to verify_ratchet.cmake.
#
#   cmake -DKEY=arm64-linux-glibc -DMODE=native -DROOT=<srcdir> -DXDIR=<crossdir> \
#         -DTMPROOT=<dir> [-DOPT=-O1] [-DJREGEN=1] -P tests/ast/journal_sweep.cmake
#
# KEY is <cpu>-<os>[-<libc>], but the BASELINE key is <cpu>-<os> alone: libc is
# not a filename axis. A non-default libc therefore has no bank of its own and
# skips with that reason, rather than scoring itself against the default libc's
# corpus -- which is a different corpus, and would read as a verdict.
#
# MODE:
#   cross  - run the host-hosted cross compiler. Fast, and valid for the honesty
#            census: cross and native honesty files are byte-identical (measured
#            2026-07-31 on i386/arm64/riscv64).
#   native - build a target-native mcc with the cross compiler, then run it under
#            qemu-user or wine. REQUIRED to bank a depth ceiling: cross reads
#            native-1 on arm64/riscv64 and exactly native on i386, so a ceiling
#            banked from a cross run is wrong on two targets out of three.
#
# Exits 77 (ctest SKIP) with a specific reason whenever a prerequisite is absent,
# so a machine that lacks qemu, wine or a sysroot reports honestly instead of
# silently measuring a narrower corpus. That last failure mode is the one this
# script exists to prevent: sweeping a cross target with no sysroot compiles only
# the freestanding subset (77 of 275 files) and reports a 28% census as if it
# were the whole thing.
#
foreach(_req KEY ROOT)
    if(NOT ${_req})
        message(FATAL_ERROR "journal_sweep: ${_req} is required")
    endif()
endforeach()
if(NOT MODE)
    set(MODE "cross")
endif()
if(NOT OPT)
    set(OPT "-O1")
endif()
if(NOT XDIR)
    set(XDIR "${ROOT}/cmake-cross")
endif()
if(NOT TMPROOT)
    set(TMPROOT "${ROOT}/cmake-cross/journal-sweep")
endif()

file(MAKE_DIRECTORY "${TMPROOT}")

# A macro, not a function: `return()` inside a macro unwinds the CALLER, which
# at script top level ends the script. A function would only return from itself
# and the sweep would carry on with an unmet prerequisite. Exits 0 by design --
# an unavailable target is not a build failure, and the marker file is what the
# journal-sweep-report target reads to say so out loud.
macro(_skip msg)
    message(STATUS "journal-sweep[${KEY}/${MODE}]: SKIP -- ${msg}")
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "SKIP\t${msg}\n")
    return()
endmacro()

# ---------------------------------------------------------------- key parsing
set(_libc "")
set(_k "${KEY}")
foreach(_l glibc musl ucrt msvcrt)
    if(_k MATCHES "^(.+)-${_l}$")
        set(_libc "${_l}")
        set(_k "${CMAKE_MATCH_1}")
        break()
    endif()
endforeach()
# _k is now <cpu>-<os>
if(_k MATCHES "^([a-z0-9_]+)-([a-z0-9]+)$")
    set(_cpu "${CMAKE_MATCH_1}")
    set(_os "${CMAKE_MATCH_2}")
else()
    message(FATAL_ERROR "journal_sweep: cannot parse KEY '${KEY}' as <cpu>-<os>[-<libc>]")
endif()

# default libc per OS, and the cross-compiler triple name used by cmake-cross
set(_default_libc "glibc")
if(_os STREQUAL "win32")
    set(_default_libc "ucrt")
elseif(_os STREQUAL "darwin")
    set(_default_libc "libsystem")
endif()
if(_libc STREQUAL "")
    set(_libc "${_default_libc}")
endif()

set(_triple "${_cpu}")
if(_os STREQUAL "win32")
    set(_triple "${_cpu}-win32")
elseif(_os STREQUAL "darwin")
    set(_triple "${_cpu}-osx")
elseif(_os STREQUAL "wince")
    set(_triple "${_cpu}-wince")
endif()

set(_bkey "${_cpu}-${_os}")

# ------------------------------------------------------- journal gate check
# The gate in src/mcc.h admits these five CPUs. Anything else journals 0 rows,
# which would otherwise look like a clean sweep rather than an absent oracle.
if(NOT _cpu MATCHES "^(x86_64|arm64|i386|riscv64|arm)$")
    _skip("cpu '${_cpu}' is outside the MCC_JOURNAL_HOOKS gate in src/mcc.h -- \
it journals 0 rows, so there is no oracle to compare. Widen the gate first")
endif()
if(NOT _libc STREQUAL "${_default_libc}")
    _skip("baselines are keyed by <cpu>-<os> alone, so '${_libc}' has no bank of \
its own and the '${_default_libc}' one measures a different corpus. Scoring \
against it would read as a verdict rather than as the gap it is")
endif()
if(_os STREQUAL "darwin" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    _skip("darwin needs a macOS host; there is no Darwin loader on \
${CMAKE_HOST_SYSTEM_NAME} and docker's foreign-arch path is unusable (host \
binfmt is flags:OC with no F). Run this same target ON macOS -- it is gated on \
the host, not disabled")
endif()
# wince is INSIDE the journal gate: it is MCC_TARGET_ARM plus MCC_TARGET_PE,
# the same define set as arm-win32, and the gate keys on the cpu macro alone.
# Only the native mode is blocked, and by the loader rather than by the gate --
# there is no wince emulator here and wine does not run an ARM PE image.
if(_os STREQUAL "wince" AND MODE STREQUAL "native")
    _skip("wince has no emulator or device here, so a native depth ceiling \
cannot be measured; the cross mode of this same target does run")
endif()

# ------------------------------------------------------------- the compiler
set(_mcc "${XDIR}/mcc-${_triple}")
if(NOT _libc STREQUAL "${_default_libc}" AND _os STREQUAL "linux")
    set(_mcc "${XDIR}/mcc-${_triple}-${_libc}")
endif()
if(NOT EXISTS "${_mcc}")
    _skip("no cross compiler at ${_mcc} -- build the cross preset \
(cmake --preset cross), and for a non-default libc set MCC_BUILD_MUSL=ON")
endif()

# ---------------------------------------------------------------- the sysroot
set(_sysroot "")
set(_incflags "")
set(_hosthdrs 0)
if(_os STREQUAL "linux")
    # The HOST triple at its default libc is the one key the native ctest cell
    # (ast-journal-parity) also banks, and it banks it with the host's own
    # headers rather than a vendored sysroot. That is a DIFFERENT corpus from
    # the gentoo stage3 one swept below, so each has its own bank: <key>.txt /
    # <key>.depth.txt on this path, <key>-sysroot.* on that one. Sharing one
    # file is what let x86_64 read fix=30027/792584 ops against a banked
    # 31003/795697 and PASS, because lower reads as "IMPROVED". Use the host
    # build's compiler and host headers so this path compares like with like.
    if(HOSTMCC AND HOSTKEY AND _bkey STREQUAL "${HOSTKEY}" AND MODE STREQUAL "cross")
        if(NOT EXISTS "${HOSTMCC}")
            _skip("host key ${_bkey} needs the host build's mcc at ${HOSTMCC}")
        endif()
        set(_mcc "${HOSTMCC}")
        set(_incflags "")
        set(_sysroot "")
        set(_hosthdrs 1)
        message(STATUS "journal-sweep[${KEY}/${MODE}]: host triple -- using the \
host build's mcc and host headers, matching how this baseline was banked")
    else()
        set(_sysroot "${ROOT}/vendor/gentoo-stage3-${_cpu}-${_libc}")
        if(NOT IS_DIRECTORY "${_sysroot}")
            _skip("no ${_libc} sysroot at ${_sysroot} -- without it the sweep \
compiles only the freestanding subset and silently reports a partial census")
        endif()
        # --sysroot alone is not enough: it yields 77 of 275 files. The explicit
        # usr/include is what takes it to 261.
        set(_incflags "--sysroot=${_sysroot}" "-I${_sysroot}/usr/include")
    endif()
elseif(_os STREQUAL "win32")
    if(_libc STREQUAL "ucrt")
        # winlibs ships x86 only, and the compile flags below never name it --
        # every PE sweep stages against mcc's own bundled headers. So the
        # vendored runtime is checked for the two cpus it can actually be
        # present for, and an arm PE key is NOT gated on an i686 directory
        # that has nothing to do with it: that read as "no ucrt runtime" and
        # skipped a triple the sweep can measure.
        set(_mingw "")
        if(_cpu STREQUAL "i386")
            set(_mingw "i686")
        elseif(_cpu STREQUAL "x86_64")
            set(_mingw "x86_64")
        endif()
        if(NOT _mingw STREQUAL "")
            set(_sysroot "${ROOT}/vendor/winlibs-mingw-w64-16.1.0-ucrt-${_mingw}")
            if(NOT IS_DIRECTORY "${_sysroot}")
                _skip("no ucrt PE runtime at ${_sysroot}")
            endif()
        endif()
    else()
        _skip("libc '${_libc}' is not vendored for PE -- only the winlibs ucrt \
runtimes are in tree. Vendor an msvcrt toolchain (mstorsjo-llvm-msvcrt) first")
    endif()
    set(_incflags "-B${ROOT}/runtime/win32" "-I${ROOT}/runtime/include")
elseif(_os STREQUAL "wince")
    # Same staging as win32: a wince mcc is arm-win32's define set with a
    # different PE subsystem, and it reads the same bundled PE headers.
    set(_incflags "-B${ROOT}/runtime/win32" "-I${ROOT}/runtime/include")
endif()

# ------------------------------------------------------------------ the mode
set(_launch "")
set(_pathprefix "")
if(MODE STREQUAL "native")
    if(_os STREQUAL "linux")
        set(_qemu "qemu-${_cpu}")
        if(_cpu STREQUAL "arm64")
            set(_qemu "qemu-aarch64")
        endif()
        find_program(_qemubin "${_qemu}")
        if(NOT _qemubin)
            _skip("${_qemu} not on PATH -- a native depth ceiling needs it")
        endif()
        set(_launch "${_qemubin}" "-L" "${_sysroot}")
    elseif(_os STREQUAL "win32")
        # On Windows a PE mcc IS native -- no emulator, no Z: rewriting. Wine is
        # only the stand-in when the host is not Windows, and whether it is a
        # faithful one is still unvalidated against real hardware (TODO).
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            set(_launch "")
            set(_pathprefix "")
        else()
            find_program(_winebin wine)
            if(NOT _winebin)
                _skip("wine not on PATH -- a native PE run needs it off-Windows")
            endif()
            set(_launch "${_winebin}")
            set(_pathprefix "Z:")
        endif()
    endif()
    # the native compiler itself
    set(_natdir "${TMPROOT}/native-${_bkey}")
    set(_natmcc "${_natdir}/mcc")
    if(_os STREQUAL "win32")
        set(_natmcc "${_natdir}/mcc.exe")
    endif()
    if(NOT EXISTS "${_natmcc}")
        _skip("no native compiler at ${_natmcc} -- build it first with the \
journal-native-${KEY} target, which compiles src/mcc.c with the cross mcc")
    endif()
    set(_mcc "${_natmcc}")
    if(_os STREQUAL "win32")
        # Under wine the PE mcc needs its base dir as a Windows path; on a real
        # Windows host _pathprefix is empty and these are plain paths.
        set(_incflags "-B${_pathprefix}${ROOT}/runtime/win32"
                      "-I${_pathprefix}${ROOT}/runtime/include")
    else()
        set(_incflags "-B${XDIR}" ${_incflags})
    endif()
elseif(NOT MODE STREQUAL "cross")
    message(FATAL_ERROR "journal_sweep: MODE must be 'cross' or 'native'")
endif()

# ---------------------------------------------------------------- the corpus
set(_corpus "${ROOT}/tests/exec")
# riscv64 aborts on this one file (pre-existing arch_transfer_ret_regs assert,
# tracked P0 in docs/TODO). The abort truncates the sweep and desyncs the census,
# so exclude it rather than let the whole target report nothing.
if(_cpu STREQUAL "riscv64")
    set(_corpus "${TMPROOT}/corpus-riscv64")
    if(NOT IS_DIRECTORY "${_corpus}")
        # Copy the WHOLE tree, not just *.c. The corpus has 4 headers
        # (goldens.h, preprocessor/{include,inc_header,include2}.h) and a
        # .c-only copy makes every file that includes one fail to compile --
        # which reads as a smaller op count, not as an error, and so shows up
        # as a spurious depth IMPROVEMENT that the ratchet happily accepts.
        file(COPY "${ROOT}/tests/exec/" DESTINATION "${_corpus}")
        file(REMOVE "${_corpus}/structs_unions/struct_byval.c")
    endif()
    message(STATUS "journal-sweep[${KEY}/${MODE}]: excluding structs_unions/struct_byval.c \
(riscv64 arch_transfer_ret_regs abort, TODO P0)")
endif()

set(_jbase "${ROOT}/tests/ast/journal-baseline/${_bkey}.txt")
set(_jdepthfile "")
if(NOT _hosthdrs)
    set(_jbase "${ROOT}/tests/ast/journal-baseline/${_bkey}-sysroot.txt")
    set(_jdepthfile "-DJDEPTHFILE=${ROOT}/tests/ast/journal-baseline/${_bkey}-sysroot.depth.txt")
endif()
set(_vbase "${ROOT}/tests/ast/verify-baseline/${_bkey}.txt")
if(NOT EXISTS "${_vbase}")
    message(STATUS "journal-sweep[${KEY}/${MODE}]: no recorder-fidelity baseline at \
${_vbase}. JOURNAL=1 measures the parity delta live and returns before the ratchet \
ever reads that file, so the sweep is unaffected -- but the non-journal \
ast-verify-ratchet has nothing to compare against for this key. Bank one with \
REGEN=1 if you want it. No other target's baseline is substituted: x86_64's gap set \
is a different corpus and would read as a verdict")
endif()
if(NOT EXISTS "${_jbase}" AND NOT JREGEN)
    _skip("no journal baseline at ${_jbase} -- bank one with the \
journal-regen-${KEY} target (use MODE=native, a cross-banked depth ceiling is wrong)")
endif()

message(STATUS "journal-sweep[${KEY}/${MODE}]: mcc=${_mcc}")
message(STATUS "journal-sweep[${KEY}/${MODE}]: baseline key=${_bkey} libc=${_libc} opt=${OPT}")

set(_regen "")
if(JREGEN)
    if(MODE STREQUAL "cross")
        message(WARNING "journal-sweep[${KEY}]: banking from a CROSS run. The honesty \
file is valid this way, but the depth ceiling is native-sensitive (cross reads \
native-1 on arm64/riscv64). Prefer MODE=native.")
    endif()
    set(_regen "-DJREGEN=1")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            "-DMCC=${_mcc}"
            "-DMCCLAUNCH=${_launch}"
            "-DMCCFLAGS=${_incflags}"
            "-DMCCPATHPREFIX=${_pathprefix}"
            "-DCORPUS=${_corpus}"
            "-DEXTRA=${ROOT}/tests/diff/full_language.c"
            "-DBASELINE=${_vbase}"
            "-DJBASELINE=${_jbase}"
            ${_jdepthfile}
            "-DTMPDIR=${TMPROOT}/tmp-${_bkey}-${MODE}-${OPT}"
            "-DOPT=${OPT}"
            "-DJOURNAL=1"
            ${_regen}
            -P "${ROOT}/tests/ast/verify_ratchet.cmake"
    RESULT_VARIABLE _rc)
if(_rc EQUAL 77)
    message(STATUS "journal-sweep[${KEY}/${MODE}]: SKIP from verify_ratchet (no journal hooks)")
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "SKIP\tbuild has no MCC_JOURNAL_HOOKS\n")
elseif(NOT _rc EQUAL 0)
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "FAIL\trc=${_rc}\n")
    # KEEPGOING lets the aggregate target measure the WHOLE matrix and defer the
    # verdict to journal-sweep-report. Without it the first failing triple stops
    # ninja and every later key silently reads as "not run" -- which looks the
    # same as a machine that cannot host them, and that ambiguity is the thing
    # this matrix exists to remove. A directly-invoked single target still fails.
    if(KEEPGOING)
        message(STATUS "journal-sweep[${KEY}/${MODE}]: FAILED (rc=${_rc}) -- continuing, see journal-sweep-report")
    else()
        message(FATAL_ERROR "journal-sweep[${KEY}/${MODE}]: FAILED (rc=${_rc})")
    endif()
else()
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "OK\t${_libc} ${OPT}\n")
    message(STATUS "journal-sweep[${KEY}/${MODE}]: OK")
endif()
