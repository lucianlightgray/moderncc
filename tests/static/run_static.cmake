# Driver for the static-glibc smoke test: link SRC fully static with mcc, run
# it, and compare stdout to the expected line. Invoked via `cmake -P` with
# -DMCC -DBDIR -DIDIR -DSRC -DOUT set. The enclosing add_test() is only created
# when a static glibc libc.a is present on the host, so a link failure here is a
# real defect, not a missing-toolchain skip.

set(_expect "i=42 s=tls digit=1 up=A heap\n")

execute_process(
	COMMAND "${MCC}" "-B${BDIR}" "-I${IDIR}" -static "${SRC}" -o "${OUT}"
	RESULT_VARIABLE _lrc OUTPUT_VARIABLE _lout ERROR_VARIABLE _lerr)
if(NOT _lrc EQUAL 0)
	message(FATAL_ERROR "static link failed (${_lrc}):\n${_lout}${_lerr}")
endif()

execute_process(
	COMMAND "${OUT}"
	RESULT_VARIABLE _rrc OUTPUT_VARIABLE _rout)
if(NOT _rrc EQUAL 0)
	message(FATAL_ERROR "static binary crashed/exited ${_rrc} (SIGSEGV=139); output:\n${_rout}")
endif()
if(NOT "${_rout}" STREQUAL "${_expect}")
	message(FATAL_ERROR "output mismatch:\n  expected: ${_expect}\n  actual:   ${_rout}")
endif()

message(STATUS "static-glibc smoke: OK")
