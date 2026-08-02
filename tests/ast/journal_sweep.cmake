cmake_minimum_required(VERSION 3.22)
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

macro(_skip msg)
    message(STATUS "journal-sweep[${KEY}/${MODE}]: SKIP -- ${msg}")
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "SKIP\t${msg}\n")
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
if(_k MATCHES "^([a-z0-9_]+)-([a-z0-9]+)$")
    set(_cpu "${CMAKE_MATCH_1}")
    set(_os "${CMAKE_MATCH_2}")
else()
    message(FATAL_ERROR "journal_sweep: cannot parse KEY '${KEY}' as <cpu>-<os>[-<libc>]")
endif()

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
if(NOT _libc STREQUAL "${_default_libc}")
    set(_bkey "${_cpu}-${_os}-${_libc}")
endif()

if(NOT _cpu MATCHES "^(x86_64|arm64|i386|riscv64|arm)$")
    _skip("cpu '${_cpu}' is outside the MCC_JOURNAL_HOOKS gate in src/mcc.h -- \
it journals 0 rows, so there is no oracle to compare. Widen the gate first")
endif()
if(_os STREQUAL "darwin" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    _skip("darwin needs a macOS host; there is no Darwin loader on \
${CMAKE_HOST_SYSTEM_NAME} and docker's foreign-arch path is unusable (host \
binfmt is flags:OC with no F). Run this same target ON macOS -- it is gated on \
the host, not disabled")
endif()
if(_os STREQUAL "wince" AND MODE STREQUAL "native")
    _skip("wince has no emulator or device here, so a native depth ceiling \
cannot be measured; the cross mode of this same target does run")
endif()

set(_mcc "${XDIR}/mcc-${_triple}")
if(NOT _libc STREQUAL "${_default_libc}" AND _os STREQUAL "linux")
    set(_mcc "${XDIR}/mcc-${_triple}-${_libc}")
endif()
if(NOT EXISTS "${_mcc}")
    _skip("no cross compiler at ${_mcc} -- build the cross preset \
(cmake --preset cross), and for a non-default libc set MCC_BUILD_MUSL=ON")
endif()

set(_sysroot "")
set(_incflags "")
set(_hosthdrs 0)
if(_os STREQUAL "linux")
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
        set(_incflags "--sysroot=${_sysroot}" "-I${_sysroot}/usr/include")
    endif()
elseif(_os STREQUAL "win32")
    if(_libc STREQUAL "ucrt")
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
    elseif(_libc STREQUAL "msvcrt")
        if(NOT MODE STREQUAL "native")
            _skip("msvcrt is measurable only on a Windows host: a cross sweep \
stages the same bundled win32 headers for every PE key, so it would bank a copy \
of the ucrt figures under the msvcrt name. Run journal-regen-${KEY} there")
        endif()
    else()
        _skip("libc '${_libc}' is not a PE libc this tree knows -- the PE keys \
are ucrt and msvcrt")
    endif()
    set(_incflags "-B${ROOT}/runtime/win32" "-I${ROOT}/runtime/include")
elseif(_os STREQUAL "wince")
    set(_incflags "-B${ROOT}/runtime/win32" "-I${ROOT}/runtime/include")
endif()

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
        set(_incflags "-B${_pathprefix}${ROOT}/runtime/win32"
                      "-I${_pathprefix}${ROOT}/runtime/include")
    else()
        set(_incflags "-B${XDIR}" ${_incflags})
    endif()
elseif(NOT MODE STREQUAL "cross")
    message(FATAL_ERROR "journal_sweep: MODE must be 'cross' or 'native'")
endif()

set(_corpus "${ROOT}/tests/exec")
if(_cpu STREQUAL "riscv64")
    set(_corpus "${TMPROOT}/corpus-riscv64")
    if(NOT IS_DIRECTORY "${_corpus}")
        file(COPY "${ROOT}/tests/exec/" DESTINATION "${_corpus}")
        file(REMOVE "${_corpus}/structs_unions/struct_byval.c")
    endif()
    message(STATUS "journal-sweep[${KEY}/${MODE}]: excluding structs_unions/struct_byval.c \
(riscv64 arch_transfer_ret_regs abort, TODO P0)")
endif()

set(_jbase "${ROOT}/tests/ast/journal-baseline/${_bkey}.txt")
set(_jdepth_path "${ROOT}/tests/ast/journal-baseline/${_bkey}.depth.txt")
set(_jdepthfile "")
if(NOT _hosthdrs)
    set(_jbase "${ROOT}/tests/ast/journal-baseline/${_bkey}-sysroot.txt")
    set(_jdepth_path "${ROOT}/tests/ast/journal-baseline/${_bkey}-sysroot.depth.txt")
    set(_jdepthfile "-DJDEPTHFILE=${_jdepth_path}")
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
        message(STATUS "journal-sweep[${KEY}]: banking BREADTH from a cross run, which \
is valid -- cross and native honesty files are byte-identical. The depth ceiling is \
native-sensitive (cross reads native-1 on arm64/riscv64) and is NOT banked here.")
        set(_regen "-DJREGEN=1")
    else()
        set(_regen "-DJREGEN=1;-DJDEPTHREGEN=1")
    endif()
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
    if(NOT EXISTS "${_jdepth_path}")
        message(STATUS "journal-sweep[${KEY}/${MODE}]: SKIP -- breadth is banked and \
checked, depth is not: no ceiling at ${_jdepth_path}, and depth may only be banked from \
a MODE=native run. Half-ratcheted, so not reported as a pass")
    else()
        message(STATUS "journal-sweep[${KEY}/${MODE}]: SKIP from verify_ratchet (no journal hooks)")
    endif()
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "SKIP\tbuild has no MCC_JOURNAL_HOOKS\n")
elseif(NOT _rc EQUAL 0)
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "FAIL\trc=${_rc}\n")
    if(KEEPGOING)
        message(STATUS "journal-sweep[${KEY}/${MODE}]: FAILED (rc=${_rc}) -- continuing, see journal-sweep-report")
    else()
        message(FATAL_ERROR "journal-sweep[${KEY}/${MODE}]: FAILED (rc=${_rc})")
    endif()
else()
    file(WRITE "${TMPROOT}/${KEY}-${MODE}.status" "OK\t${_libc} ${OPT}\n")
    message(STATUS "journal-sweep[${KEY}/${MODE}]: OK")
endif()
