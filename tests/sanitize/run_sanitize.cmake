# Driver for the sanitize-smoke test: compile SRC with the sanitizer-instrumented
# mcc (mcc_s), run the result, and compare stdout. Invoked via `cmake -P` with
# -DMCC -DBDIR -DSRC -DOUT set (-DASAN_BINDIR optional: the dir holding the MSVC
# clang_rt.asan_dynamic-*.dll, prepended to PATH so mcc_s can load it).
#
# This is the one place the sanitizer actually *executes*: mcc_s is fully
# instrumented, so any failure here (ASan report, UBSan trap -> SIGILL, or a
# wrong result) is a real defect in mcc, not a missing-toolchain skip.

set(_expect "fib=55 dot=39\n")

if(ASAN_BINDIR AND EXISTS "${ASAN_BINDIR}")
	set(ENV{PATH} "${ASAN_BINDIR};$ENV{PATH}")
endif()

execute_process(
	COMMAND "${MCC}" "-B${BDIR}" "${SRC}" -o "${OUT}"
	RESULT_VARIABLE _crc OUTPUT_VARIABLE _cout ERROR_VARIABLE _cerr)
if(NOT _crc EQUAL 0)
	message(FATAL_ERROR "sanitized mcc failed to compile (${_crc}):\n${_cout}${_cerr}")
endif()

execute_process(
	COMMAND "${OUT}"
	RESULT_VARIABLE _rrc OUTPUT_VARIABLE _rout ERROR_VARIABLE _rerr)
if(NOT _rrc EQUAL 0)
	message(FATAL_ERROR "sanitized-mcc output binary exited ${_rrc} (132=UBSan trap, 139=SIGSEGV):\n${_rout}${_rerr}")
endif()
if(NOT "${_rout}" STREQUAL "${_expect}")
	message(FATAL_ERROR "output mismatch:\n  expected: ${_expect}\n  actual:   ${_rout}")
endif()

message(STATUS "sanitize smoke: OK (${MCC})")
